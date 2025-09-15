import concurrent.futures
import concurrent.futures
import logging
import os
import re
import shutil
import time
from typing import Type, Optional, Union, Hashable, Tuple
from typing import cast

import networkx as nx
import pandas as pd
import yaml
from networkx.classes.reportviews import NodeView
from networkx.readwrite import json_graph
from typing_extensions import Self
from yaml import MappingNode

from .HAKCBase import HAKCDivisionEnum, HAKCDBNode
from .HAKCDatabase import HAKCDatabase
from .HAKCLogger import HAKCLogger
from .HAKCObjects import HAKCSymbol, HAKCDefinitionLocation, HAKCFunction, HAKCType, HAKCCompartment, HAKCDivision, \
    HAKCScope, HAKCGlobalVariable, HAKCAdjustment, HAKCCompartmentalizationAdjustment
from .HAKCStaticAnalysis import init_mp_database, compute_dag_edges_for_symbol
from .HAKCUtils import batched

logging.setLoggerClass(HAKCLogger)

logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc-compartmentalization'))


class HAKCCompartmentalization(yaml.YAMLObject, nx.MultiDiGraph):
    no_enforcement_compartment_id = 0
    no_enforcement_division: int = HAKCDivisionEnum.NO_DIVISION.value
    default_division: int = HAKCDivisionEnum.TEAL_DIVISION.value
    DefaultDivisionCount = max(1, len(HAKCDivisionEnum) - 1)
    persisted_attr = 'persisted'
    yaml_tag = "!HAKCCompartmentalization"

    def __init__(self, max_division_count: Optional[int] = 16, G: Optional[nx.MultiDiGraph] = None):
        yaml.YAMLObject.__init__(self)
        nx.MultiDiGraph.__init__(self)
        self.conn = None
        if G:
            self.load_from_yaml(G)
        self.max_division_count = max_division_count

    def load_from_yaml(self, G: nx.MultiDiGraph) -> Self:
        symbol_mapping = dict()
        symbol_division_mapping = dict()
        division_compartment_mapping = dict()
        for symbol in self.get_filtered_nodes(G, node_filter=lambda n: isinstance(n, HAKCFunction) or isinstance(n,
                                                                                                                 HAKCGlobalVariable)):
            symbol: Union['HAKCFunction', 'HAKCGlobalVariable']  # suppress type warning
            # logger.fatal(f"symbol found: {symbol}")
            new_symbol = HAKCFunction(Name=symbol.name) if isinstance(symbol, HAKCFunction) else HAKCGlobalVariable(
                Name=symbol.name)
            new_symbol.set_computed_hash(symbol.computed_hash)
            for nbr in G.neighbors(symbol):
                for edge_data in G.get_edge_data(symbol, nbr):
                    if edge_data == HAKCSymbol.relation_type:
                        # assert isinstance(nbr, HAKCType), f"assert(isinstance({nbr}, HAKCType)"
                        new_symbol.type = nbr
                    elif edge_data == HAKCSymbol.relation_scope:
                        new_symbol.scope = nbr
                    elif edge_data == HAKCSymbol.relation_definition_location:
                        defining_line = G.get_edge_data(symbol, nbr, HAKCSymbol.relation_definition_location)[
                            "DefiningLine"]
                        new_symbol.definition_location = nbr
                        new_symbol.definition_location.defining_line = defining_line
                    elif edge_data == HAKCSymbol.relation_division:
                        division = nbr
                        for comp_nbr in G.neighbors(division):
                            for comp_edge_data in G.get_edge_data(division, comp_nbr):
                                if comp_edge_data == HAKCDivision.relation_compartment:
                                    division_compartment_mapping[division] = comp_nbr
                                    break
                        symbol_division_mapping[symbol] = division
            symbol_mapping[symbol] = new_symbol
        # now that all the HACKSymbols have been created, add the HAKCFunction info using new_symbols
        for symbol, new_symbol in symbol_mapping.items():
            for nbr in G.neighbors(symbol):
                for edge_data in G.get_edge_data(symbol, nbr):
                    if edge_data == HAKCFunction.relation_direct_calls:
                        # need to add the new_symbol created which has all the required fields set
                        assert (isinstance(nbr, HAKCSymbol))
                        new_symbol.direct_calls.append(symbol_mapping[nbr])
                    elif edge_data == HAKCFunction.relation_indirect_calls:
                        assert (isinstance(nbr, HAKCType))
                        new_symbol.indirect_calls.append(nbr)
                    elif edge_data == HAKCSymbol.relation_dag:
                        dag_weight = G.get_edge_data(symbol, nbr, HAKCSymbol.relation_dag)["weight"]
                        # logger.error(f"Adding DAG Edge from {new_symbol} to {symbol_mapping[symbol]} with weight {dag_weight}")
                        self.add_dag_edge(new_symbol, symbol_mapping[nbr], dag_weight)
                    elif edge_data == HAKCSymbol.relation_symbol:
                        new_symbol.used_symbols.append(symbol_mapping[nbr])

            symbol_mapping[symbol] = new_symbol

        # now add all the functions
        for symbol, new_symbol in symbol_mapping.items():
            if isinstance(symbol, HAKCFunction):
                self.__add_function(new_symbol)
            else:
                self.__add_global_variable(new_symbol)
            # The yaml outputted from analysis will not have compartments or divisions, meaning the mapping dicts will be empty
            division = symbol_division_mapping[symbol] if symbol in symbol_division_mapping else None
            compartment = division_compartment_mapping[division] if division in division_compartment_mapping else None
            if division and compartment:
                self.__add_division_compartment(new_symbol, division, compartment)
        return self

    @classmethod
    def to_yaml(cls, dumper: yaml.Dumper, data) -> MappingNode:
        compartmentalization_data = json_graph.node_link_data(data, edges='edges')
        compartmentalization_data['max_division_count'] = data.max_division_count
        return dumper.represent_mapping(cls.yaml_tag, compartmentalization_data)

    @classmethod
    def from_yaml(cls, loader: yaml.CLoader, node):
        # HAKCCompartmentalization.add_yaml_constructors()
        graph_data = loader.construct_mapping(node, deep=True)
        graph = json_graph.node_link_graph(graph_data, edges='edges')
        return cls(max_division_count=graph_data.get('division_count', 16), G=graph)

    @staticmethod
    def parse_yaml(filename: str):
        import traceback
        logger.add_file_handler(filename.replace(".yml", ".static-analysis.log").replace(".dag", ""))
        logger.info(f"Parsing {filename}")
        HAKCCompartmentalization.add_yaml_constructors()
        with open(filename, 'rb') as f:
            try:
                compartmentalization = yaml.load(f, Loader=yaml.CLoader)
                logger.debug(f"Loaded {compartmentalization} from {filename}")
                return compartmentalization
            except Exception as e:
                logger.fatal(f"Failed to load yaml with error: {str(e)} in {filename}")
                traceback.print_exc()

    def __str__(self):
        divs_comps = len(self.get_divisions_compartments())
        return f"HAKCCompartmentalization with {len(self.nodes)} nodes, {len(self.edges)} edges [{len(self.get_symbols())} Symbols, {len(self.get_types())} Types, {len(self.get_scopes())} Scopes, {len(self.get_definition_locations())} DefinitionLocations{f' , {divs_comps} (Divisions, Compartments) tuples' if divs_comps > 0 else ''}]"

    def add_symbols(self, functions: list[HAKCFunction], global_variables: list[HAKCGlobalVariable]) -> None:
        for global_variable in global_variables:
            self.__add_global_variable(global_variable)

        for function in functions:
            # logger.debug(f"Adding function {function} with defining location {function.definition_location}")
            self.__add_function(function)

    def add_function(self, function: HAKCFunction) -> None:
        return self.__add_function(function)

    def __add_function(self, function: HAKCFunction) -> None:
        assert (isinstance(function, HAKCFunction))

        # add symbol with all its attributes
        self.__add_symbol(function)
        # add function specific attributes,
        for direct_call in function.direct_calls:
            assert (isinstance(direct_call, HAKCFunction))
            self.__add_symbol(direct_call)
            self.__add_persistent_edge(function, direct_call, key=HAKCFunction.relation_direct_calls)

        for indirect_call in function.indirect_calls:
            # TODO handle indirect call sources?
            # logger.error(f"indirect_call: {type(indirect_call)}")
            # assert(isinstance(indirect_call, HAKCType))
            if isinstance(indirect_call, HAKCType):
                # TODO: should types already be persisted? getting duplicate key error when using data columns as hakctype hashable inputs
                # self.__add_type(indirect_call, already_persisted = True)
                # self.__add_type(indirect_call)
                self.__add_persistent_edge(function, indirect_call, key=HAKCFunction.relation_indirect_calls)

    def add_global_variable(self, global_variable: HAKCGlobalVariable) -> None:
        return self.__add_global_variable(global_variable)

    def __add_global_variable(self, global_variable: HAKCGlobalVariable) -> None:
        assert (isinstance(global_variable, HAKCGlobalVariable))
        self.__add_symbol(global_variable)

    def __add_type(self, _type: HAKCType, already_persisted: bool = False) -> None:
        assert (isinstance(_type, HAKCType))
        self.__add_persistent_node(_type, already_persisted=already_persisted)

    def add_symbol(self, symbol, already_persisted: bool = False) -> None:
        # function to allow for external access
        self.__add_symbol(symbol, already_persisted)

    def __add_symbol(self, symbol: HAKCSymbol, already_persisted: bool = False) -> None:
        # add 'sanitized' version of the HAKCSymbol (e.g., don't include HAKCTypePerm as a node, since it should be an edge?)
        assert (isinstance(symbol, HAKCSymbol))

        # add the symbol node
        self.__add_persistent_node(symbol, already_persisted=already_persisted)

        # now add all the edges from the symbol node to node
        self.__add_persistent_node(symbol.type, already_persisted=already_persisted)
        self.__add_persistent_edge(symbol, symbol.type, already_persisted=already_persisted,
                                   key=HAKCSymbol.relation_type)
        self.__add_persistent_node(symbol.scope, already_persisted=already_persisted)
        self.__add_persistent_edge(symbol, symbol.scope, already_persisted=already_persisted,
                                   key=HAKCSymbol.relation_scope)
        if symbol.definition_location:
            # logger.fatal(f"Persisting symbol definition location {symbol} -> {symbol.definition_location}")
            self.__add_persistent_node(symbol.definition_location, already_persisted=already_persisted)
            self.__add_persistent_edge(symbol, symbol.definition_location, already_persisted=already_persisted,
                                       key=HAKCSymbol.relation_definition_location,
                                       DefiningLine=symbol.definition_location.defining_line)
        for used_symbol in symbol.used_symbols:
            self.__add_persistent_node(used_symbol, already_persisted=already_persisted)
            self.__add_persistent_edge(symbol, used_symbol, already_persisted=already_persisted,
                                       key=HAKCSymbol.relation_symbol)

    def add_dag_edge(self, head: HAKCSymbol, tail: HAKCSymbol, dag_edge_weight: int) -> None:
        if dag_edge_weight > 0:
            self.__add_persistent_edge(head, tail, key=HAKCSymbol.relation_dag, weight=dag_edge_weight)

    def __add_persistent_node(self, node: HAKCDBNode, already_persisted: bool = False) -> HAKCDBNode:
        # Note: networkx determines if a node is already in the graph if the id(node) exists, meaning the memory address, not the actual hash
        # need to maintain internal map of object hashes to the actual networkx node (prevent duplicates, ensure data normalcy)
        assert (isinstance(node, HAKCDBNode))
        if node not in self:
            attrs = {HAKCCompartmentalization.persisted_attr: already_persisted}
            self.add_node(node, **attrs)
            # self.node_map[node_hash] = node
        # assert self.node_map[node_hash] == node, f"{self.node_map[node_hash]} =?= {node}"
        # print(f"FOUND {self.node_map[node_hash]} =?= {node}")
        # return self.node_map[node_hash]
        return node

    def get_compartment_from_division(self, division: HAKCDivision) -> HAKCCompartment:
        assert (isinstance(division, HAKCDivision))
        for edge in self.edges(division):
            compartment = edge[1]
            if isinstance(compartment, HAKCCompartment):
                return compartment
        assert False, f"Division {division} should always return a HAKCCompartment!"

    def get_divisions_compartments(self) -> set[Tuple[HAKCDivision, HAKCCompartment]]:
        nodes = set()
        for division in self.get_filtered_nodes(self, node_filter=lambda n: isinstance(n, HAKCDivision)):
            compartment = self.get_compartment_from_division(division)
            nodes.add((division, compartment))
        return nodes

    def add_division_compartment(self, symbol: HAKCSymbol, division: HAKCDivision,
                                 compartment: HAKCCompartment) -> None:
        return self.__add_division_compartment(symbol, division, compartment)

    def __add_division_compartment(self, symbol: HAKCSymbol, division: HAKCDivision,
                                   compartment: HAKCCompartment) -> None:
        assert (isinstance(symbol, HAKCSymbol))
        assert (isinstance(division, HAKCDivision))
        assert (isinstance(compartment, HAKCCompartment))

        self.__add_persistent_edge(symbol, division, key=HAKCSymbol.relation_division)
        self.__add_persistent_edge(division, compartment, key=HAKCDivision.relation_compartment)

    def add_default_compartmentalization(self, db_dir: Optional[str] = None, create_schema: bool = False) -> None:
        # TODO: add check that create_schema is always false if conn is none
        logger.info(f'Adding Default Compartmentalization')
        compartment_id = HAKCCompartmentalization.no_enforcement_compartment_id + 1
        division_id = HAKCCompartmentalization.default_division
        symbols = list(self.get_symbols())
        # first construct all the compartments and divisions

        for symbol in logger.progress_bar(iterable=symbols, desc='Adding default compartmentalization'):
            # TODO: update; setting some default value for now, will update with Derrick (current implementation is probably logically incorrect)
            compartment = HAKCCompartment(compartment_id)
            compartment.entry_token = self.compute_entry_token(compartment_id)
            division = HAKCDivision(division_id)
            division.access_token = self.compute_access_token(division_id, compartment_id)
            logger.debug(f"adding div {division} -> comp {compartment} for symbol {symbol}")
            self.__add_division_compartment(symbol, division, compartment)
            compartment_id += 1

        if db_dir:
            self.persist_to_database(self.conn, create_schema=create_schema)

    def create_dag_multithread(self, core_count: int, db_dir: str):
        # TODO: We might be able to rewrite this without depending on the compartmentalization object
        logger.info(f'{self} starting DAG creation using {core_count} cores')
        symbol_hashes = self.get_symbol_hashes()
        batch_size = 100
        dag_edges_added = 0
        # the conn must be closed since N threads are opened using init_mp_database
        self.close_conn()
        with concurrent.futures.ProcessPoolExecutor(max_workers=core_count, initializer=init_mp_database,
                                                    initargs=(db_dir,)) as executor:
            futures = list()
            with logger.progress_bar(total=int(len(symbol_hashes) / batch_size) + 1,
                                     desc='DAG Edge computation scheduling') as pbar:
                try:
                    for symbol_hash_batch in batched(symbol_hashes.keys(), batch_size):
                        future = executor.submit(compute_dag_edges_for_symbol, symbol_hash_batch)
                        futures.append(future)
                        pbar.update(1)
                except Exception as e:
                    logger.error(f'Error submitting tasks: {str(e)}')
                    executor.shutdown(wait=False, cancel_futures=True)
                    raise e

            with logger.progress_bar(total=len(futures), desc='DAG Edge addition') as pbar:
                try:
                    dag_edges = dict()
                    for future in concurrent.futures.as_completed(futures):
                        pbar.update(1)
                        try:
                            edge_weights = future.result()
                            # logger.error(f"edge weights: {edge_weights}")
                            for (head_hash, tail_hash, dag_edge_weight) in edge_weights:
                                if head_hash not in dag_edges:
                                    dag_edges[head_hash] = dict()
                                head_symbol = symbol_hashes[head_hash]
                                tail_symbol = symbol_hashes[tail_hash]
                                self.add_dag_edge(head_symbol, tail_symbol, dag_edge_weight=dag_edge_weight)
                                dag_edges[head_hash][tail_hash] = dag_edge_weight
                                dag_edges_added += 1
                        except Exception as e:
                            logger.error(f'Error computing DAG edge: {str(e)}')
                except KeyboardInterrupt as ki:
                    logger.info(f'Stopping edge computation')
                    executor.shutdown(wait=False, cancel_futures=True)
                    raise ki
        # reopen connection
        self.open_conn(db_dir)
        logger.info(f'Adding {dag_edges_added} DAG edges {self}')
        # conn = self.conn if self.conn else HAKCDatabase(db_dir, max_num_threads=core_count)
        self.conn.persist_dag_edges(dag_edges)
        return self

    def create_dag(self, core_count: int, db_dir: str):
        core_count = max(1, core_count)
        logger.info(f'Starting DAG construction from {self} with {core_count} cores')
        start = time.time()
        self.create_dag_multithread(core_count, db_dir)
        end = time.time()

        logger.info(f'Finished creating DAG {self}')
        logger.info(f'    Total Time: {end - start} seconds')

    def adjust_compartmentalization(self, adjust_path: str, db_dir: str):
        adjustment = None
        with open(adjust_path, 'r') as f:
            adjustment = yaml.load(f, Loader=yaml.Loader)

        if not adjustment:
            raise RuntimeError(f"Unable to load Adjustments from {adjust_path}")

        logger.info(f'Adjusting compartmentalization based on {adjust_path}')

        symbols = self.conn.get_symbols()
        assert len(symbols) != 0, f"No symbols were returned from database!"
        nec_division = None
        no_enforcement_compartment = None
        if adjustment.add_no_enforcement_compartment:
            nec_division = HAKCDivision(HAKCCompartmentalization.no_enforcement_division)
            no_enforcement_compartment = HAKCCompartment(HAKCCompartmentalization.no_enforcement_compartment_id)
            self.add_division(nec_division, no_enforcement_compartment)

        for symbol in logger.progress_bar(iterable=symbols, desc='Adjusting Compartmentalization'):
            if symbol.definition_location is not None and symbol.definition_location.defining_file and "rosdemo" in symbol.definition_location.defining_file:
                print(f'Found {symbol.definition_location.defining_file} in {symbol.definition_location}')
            adjustments = adjustment.get_adjusted_division_and_compartment(
                symbol.definition_location.defining_file if symbol.definition_location else None)
            adjusted_division = None
            adjusted_compartment = None
            if adjustments is None:
                if adjustment.add_no_enforcement_compartment:
                    adjusted_division = nec_division
                    adjusted_compartment = no_enforcement_compartment
                else:
                    adjustments = self.conn.get_division_id_compartment_id_from_symbol(symbol)
                    if adjustments is None:
                        logger.error(f'Could not find original division for {symbol}')
                        continue

            if adjustments is not None:
                adjusted_division, adjusted_compartment = adjustments

            if adjusted_division is not None:
                if adjusted_division != nec_division:
                    logger.info(f'{symbol} is moving to {adjusted_division}')
                else:
                    logger.debug(f'{symbol} is moving to NEC {adjusted_division}')
                self.set_symbol_division_by_object(symbol, adjusted_division, adjusted_compartment)
            else:
                logger.info(f'{symbol} is unchanged')

        logger.info(f'Removing existing compartments')
        self.conn.delete_all_compartments()
        self.persist_to_database(self.conn)
        logger.info(f'Done adjusting compartmentalization')
        logger.info(
            f'Compartmentalization now has {len(self.conn.get_all_divisions())} divisions across {len(self.conn.get_all_compartments())} compartments')
        # conn.close()

    def compute_entry_token(self, compartment_id: int, divisions=None) -> int:
        if divisions is None:
            divisions = []
        token = compartment_id << self.max_division_count
        # TODO: ask derrick - does this apply to all divisions, or just divisions in the same compartment?
        for division in divisions:
            token |= (1 << division.division_id)
        return token

    def compute_access_token(self, division_id: int, compartment_id: int,
                             allowable_accesses=None) -> int:
        if allowable_accesses is None:
            allowable_accesses = []
        if division_id != HAKCDivisionEnum.NO_DIVISION.value:
            access_token = (compartment_id << self.max_division_count) | (1 << division_id)
            for division in allowable_accesses:
                compartment = self.get_compartment_from_division(division)
                if compartment.compartment_id != compartment_id:
                    raise RuntimeError(f'Trying to add access to Compartment {compartment.compartment_id} to {self}')
                access_token |= (1 << division.division_id)
        else:
            access_token = 0xFFFF
        return access_token

    def __add_persistent_edge(self, u_for_edge: HAKCDBNode, v_for_edge: HAKCDBNode, key,
                              already_persisted: bool = False, **attr) -> None:
        if not (self.has_edge(u_for_edge, v_for_edge, key)):
            attr[HAKCCompartmentalization.persisted_attr] = already_persisted
            self.add_edge(u_for_edge, v_for_edge, key, **attr)

    def _get_neighbors(self, symbol: HAKCSymbol, edge_key: str) -> list:
        nbrs = list()
        if symbol not in self.nodes:
            # Note: sometimes a symbol in a query is incompletely defined (thus hashes do not match) and we need to search harder for the correct symbol in our graph
            logger.debug(f"Searching for symbol: {symbol}")
            found_symbols = self.get_symbol_by_name(symbol.name)
            if len(found_symbols) != 1:
                raise RuntimeError(
                    f'Symbol {symbol} could not be found by hash, but found {len(found_symbols)} symbols when searching by name!')
            symbol = list(found_symbols)[0]
        for nbr, edges in self.adj[symbol].items():
            if edge_key in edges:
                nbrs.append(nbr)
        return nbrs

    def get_indirect_calls(self, symbol: HAKCSymbol) -> list[HAKCType]:
        return self._get_neighbors(symbol, HAKCFunction.relation_indirect_calls)

    def get_compartment_node(self, compartment_id: int) -> Optional[HAKCCompartment]:
        for compartment in self.get_filtered_nodes(self, node_filter=lambda
                n: isinstance(n, HAKCCompartment)):
            if compartment.compartment_id == compartment_id:
                return compartment
        return None

    def get_division_node(self, division_id: int, compartment_id: int) -> Optional[HAKCDivision]:
        for division in self.get_filtered_nodes(self, node_filter=lambda n: isinstance(n, HAKCDivision)):
            if division_id == division.division_id:
                compartments = self._get_neighbors(division, HAKCDivision.relation_compartment)
                # A division can only be part of a single compartment, no more and no less
                assert (len(compartments) == 0)
                if compartment_id == compartments[0].compartment_id:
                    return division
        return None

    def set_symbol_division_by_id(self, symbol: HAKCSymbol, division_id: int, compartment_id: int) -> None:
        division = self.get_division_node(division_id, compartment_id)
        compartment = self.get_compartment_node(compartment_id)
        if compartment is None:
            compartment = HAKCCompartment(compartment_id)
        if division is None:
            division = HAKCDivision(division_id)
        self.set_symbol_division_by_object(symbol, division, compartment)

    def add_division(self, division: HAKCDivision, compartment: HAKCCompartment) -> None:
        self.__add_persistent_edge(division, compartment, key=HAKCDivision.relation_compartment)

    def set_symbol_division_by_object(self, symbol: HAKCSymbol, division: HAKCDivision,
                                      compartment: HAKCCompartment) -> None:
        # print(f"Adding symbol division for {symbol} in division {division} with compartment {compartment}")
        self.add_division(division, compartment)
        self.__add_persistent_edge(symbol, division, key=HAKCSymbol.relation_division)

    def get_division(self, symbol: HAKCSymbol) -> Optional[HAKCDivision]:
        nbrs = self._get_neighbors(symbol, HAKCSymbol.relation_division)
        if len(nbrs) == 0:
            return None
        elif len(nbrs) > 1:
            for division in nbrs:
                logger.error(f'{division}')
            raise RuntimeError(f'Symbol {symbol} is in {len(nbrs)} divisions.')
        else:
            return nbrs[0]

    @staticmethod
    def get_filtered_nodes(G: nx.MultiDiGraph,
                           node_filter) -> 'NodeView[Union[HAKCType, HAKCScope, HAKCSymbol, HAKCCompartment, HAKCDivision, HAKCDefinitionLocation, HAKCFunction, HAKCGlobalVariable, HAKCCompartmentalization, Hashable]]':
        return nx.subgraph_view(G, filter_node=node_filter).nodes

    def get_types(self) -> 'NodeView[HAKCType | Hashable]':
        return self.get_filtered_nodes(self, node_filter=lambda n: isinstance(n, HAKCType))

    def get_functions(self) -> 'NodeView[HAKCFunction | Hashable]':
        return self.get_filtered_nodes(self, node_filter=lambda n: isinstance(n, HAKCFunction))

    def get_global_variables(self) -> 'NodeView[HAKCGlobalVariable | Hashable]':
        return self.get_filtered_nodes(self, node_filter=lambda n: isinstance(n, HAKCGlobalVariable))

    def get_symbols(self) -> 'NodeView[Union[HAKCFunction, HAKCGlobalVariable, Hashable]]':
        return self.get_filtered_nodes(self, node_filter=lambda n: isinstance(n, HAKCFunction) or isinstance(n,
                                                                                                             HAKCGlobalVariable))

    def get_scopes(self) -> 'NodeView[HAKCScope | Hashable]':
        return self.get_filtered_nodes(self, node_filter=lambda n: isinstance(n, HAKCScope))

    def get_definition_locations(self) -> 'NodeView[HAKCDefinitionLocation | Hashable]':
        return self.get_filtered_nodes(self, node_filter=lambda n: isinstance(n, HAKCDefinitionLocation))

    def get_divisions(self) -> 'NodeView[HAKCDivision | Hashable]':
        return self.get_filtered_nodes(self, node_filter=lambda n: isinstance(n, HAKCDivision))

    def get_symbol_by_name(self, symbol_name: str) -> 'NodeView[Union[HAKCFunction, HAKCGlobalVariable, Hashable]]':
        assert (isinstance(symbol_name, str))
        return self.get_filtered_nodes(self, node_filter=lambda n: (isinstance(n, HAKCFunction) or isinstance(n,
                                                                                                              HAKCGlobalVariable)) and n.name == symbol_name)

    def get_symbol_compartment_id(self, symbol: HAKCSymbol) -> int:
        for neighbor in self.neighbors(symbol):
            if isinstance(neighbor, HAKCDivision):
                for div_neighbor in self.neighbors(neighbor):
                    if isinstance(div_neighbor, HAKCCompartment):
                        return div_neighbor.compartment_id
        raise RuntimeError(f'Symbol {symbol} is not in a compartment!')

    def get_valid_targets_from_compartment_id(self, compartment_id: int) -> Optional[list[int]]:
        # going to brute force for now
        logger.debug(f'Getting valid targets for {compartment_id}')
        valid_targets = set()

        all_symbols_by_compartment_id = set()
        # get all symbols in compartment
        for symbol in self.get_symbols():
            if self.get_symbol_compartment_id(symbol) == compartment_id:
                all_symbols_by_compartment_id.add(symbol)
        # logger.debug(f"Got {len(self.get_symbols())} symbols")
        # now search all valid neighbors
        for caller in all_symbols_by_compartment_id:
            for callee in self.neighbors(caller):
                if self.has_edge(caller, callee, HAKCSymbol.relation_dag):
                    edge_weight = self.get_edge_data(caller, callee, HAKCSymbol.relation_dag)['weight']
                    if edge_weight > 0:
                        valid_targets.add(self.get_symbol_compartment_id(callee))
        logger.debug(
            f"In get_valid_targets, compartment_id: {compartment_id}, valid targets: {sorted(list(valid_targets))}")
        return sorted(list(valid_targets))

    def get_unpersisted_nodes(self) -> dict[str, list[HAKCDBNode]]:
        result = dict()
        for node, is_persisted in self.nodes(data=HAKCCompartmentalization.persisted_attr):
            if not is_persisted:
                table_name = node.get_table_name()
                if table_name not in result:
                    result[table_name] = list()
                result[table_name].append(node)
        return result

    def get_unpersisted_edges(self) -> dict[str, list[tuple[HAKCDBNode, HAKCDBNode, dict]]]:
        # TODO: double check if this is working correctly after dag adjustment
        result = dict()
        for head, tail, table_name, attrs in self.edges(data=True, keys=True):
            if not attrs[HAKCCompartmentalization.persisted_attr]:
                edge_attributes = {}
                for key, val in attrs.items():
                    if key != HAKCCompartmentalization.persisted_attr:
                        edge_attributes[key] = val
                if table_name not in result:
                    result[table_name] = list()
                result[table_name].append((head, tail, edge_attributes))

        return result

    def _persist_nodes(self, conn: HAKCDatabase):
        unpersisted_nodes = self.get_unpersisted_nodes()
        for table_name, nodes in logger.progress_bar(iterable=unpersisted_nodes.items(), desc="Persisting to database"):
            data_to_persist = dict()
            for node in nodes:
                db_data = node.get_db_data()
                if 0 < len(data_to_persist) != len(db_data):
                    logger.error(
                        f'Node {node} does not have all the data needed. Data needed is {" ".join(sorted(data_to_persist.keys()))} and data provided is {" ".join(sorted([column.column_name for column in db_data.keys()]))}')
                for column, data in db_data.items():
                    if data is None:
                        logger.debug(f'Node {node} has None for column {column.column_name}')
                        data = column.column_type.default_value
                    if column.column_name not in data_to_persist:
                        data_to_persist[column.column_name] = list()
                    data_to_persist[column.column_name].append(data)

            df = pd.DataFrame(data_to_persist)
            logger.debug(f'Persisting {len(nodes)} Nodes to {table_name}')
            try:
                conn.insert_from_dataframe(table_name, df)
                del df
                for node in nodes:
                    self.nodes[node][HAKCCompartmentalization.persisted_attr] = True
            except Exception as e:
                del df
                logger.fatal(f'Failed to persist to {table_name}: {str(e)}')
                raise e

    def _persist_edges(self, conn: HAKCDatabase):
        unpersisted_edges = self.get_unpersisted_edges()
        for table_name, edge_data in logger.progress_bar(iterable=unpersisted_edges.items(), desc="Persisting edges"):
            head_primary_keys = list()
            tail_primary_keys = list()
            attr_list = dict()
            for head, tail, attrs in edge_data:
                head_primary_key = head.get_primary_key_data()
                tail_primary_key = tail.get_primary_key_data()
                head_primary_keys.append(head_primary_key)
                tail_primary_keys.append(tail_primary_key)
                if len(attrs) > 0:
                    for key, val in attrs.items():
                        if key not in attr_list:
                            attr_list[key] = list()
                        attr_list[key].append(val)
            if len(attr_list) == 0:
                df = pd.DataFrame({
                    'from': head_primary_keys,
                    'to': tail_primary_keys
                })
            else:
                df_data = {
                    'from': head_primary_keys,
                    'to': tail_primary_keys
                }
                for key, val in attr_list.items():
                    df_data[key] = val
                df = pd.DataFrame(df_data)
            logger.debug(f'Persisting {len(head_primary_keys)} edges to {table_name}')
            try:
                conn.insert_from_dataframe(table_name, df)
                del df
                for head, tail, _ in edge_data:
                    self.edges[head, tail, table_name][HAKCCompartmentalization.persisted_attr] = True
            except Exception as e:
                del df
                match = re.search('Runtime exception: Unable to find primary key value (-?[0-9]+)', str(e))
                if match:
                    missing_hash = int(match.group(1))
                    for head_primary_key in head_primary_keys:
                        if head_primary_key == missing_hash:
                            logger.error(f'{missing_hash} found in head_primary_keys')
                    for tail_primary_key in tail_primary_keys:
                        if tail_primary_key == missing_hash:
                            logger.error(f'{missing_hash} found in tail_primary_keys')
                else:
                    logger.error(f'Failed to persist to {table_name}: {str(e)}')
                raise e

    def persist_to_database(self, conn: HAKCDatabase, create_schema: bool = False) -> None:
        if create_schema:
            logger.info(f'Creating schema')
            self.create_schema(conn)
        logger.info('Persisting new nodes to database')
        self._persist_nodes(conn)
        logger.info('Persisting new edges to database')
        self._persist_edges(conn)
        logger.info(f"Finished persisting nodes and edges to database")

    @staticmethod
    def create_schema(conn: HAKCDatabase) -> None:
        node_tables = HAKCCompartmentalization.get_table_classes()
        for cls in logger.progress_bar(iterable=node_tables, desc='Creating data tables'):
            conn.create_node_table(node_type=cls)

        for cls in logger.progress_bar(iterable=node_tables, desc='Creating relationship tables'):
            db_relations = cls.get_db_relations()
            for db_relation in db_relations:
                conn.create_relationship_table(edge_type=db_relation)

    @staticmethod
    def delete_db(db_dir: str):
        logger.debug("Deleting database")
        if os.path.exists(db_dir) and os.path.isdir(db_dir):
            logger.info(f'Removing existing database at {db_dir}')
            shutil.rmtree(db_dir)

    def open_conn(self, db_dir: str, max_num_threads: int = 1):
        self.conn = HAKCDatabase(db_dir, max_num_threads=max_num_threads)

    def close_conn(self):
        if self.conn:
            self.conn.close()
        self.conn = None

    def create_new_db(self, db_dir: str):
        logger.debug("Creating new database")
        # Note: expecting users to close the connection manually
        self.open_conn(db_dir)
        logger.debug(f"Creating schema")
        self.persist_to_database(self.conn, create_schema=True)
        logger.info(f"Created new database with {len(self.conn.get_all_symbol_hashes())} symbols")

        # self.close_conn()

    def get_symbol_hashes(self) -> dict[int, HAKCSymbol]:
        symbol_hashes = dict()
        for symbol in self.get_symbols():
            symbol_hashes[hash(symbol)] = symbol
        return symbol_hashes

    def get_symbol_by_hash(self, symbol_hash: int) -> Optional[HAKCSymbol]:
        # logger.debug(f"Trying to find symbol for hash {symbol_hash} with symbols {self.get_symbols()}")
        for symbol in self.get_symbols():
            assert (isinstance(symbol, HAKCSymbol))
            if symbol.get_computed_hash().final_hash == symbol_hash:
                return symbol
        raise RuntimeError(f"Unable to find HAKCSymbol for symbol_hash = {symbol_hash}")

    @staticmethod
    def get_table_classes() -> list[Type[HAKCDBNode]]:
        return [
            HAKCType,
            HAKCScope,
            HAKCSymbol,
            HAKCCompartment,
            HAKCDivision,
            HAKCDefinitionLocation,
            HAKCFunction
        ]

    @staticmethod
    def add_yaml_constructors() -> None:
        for loader in [yaml.Loader, yaml.CLoader]:
            yaml.add_constructor(HAKCType.yaml_tag, HAKCType.from_yaml, Loader=loader)
            yaml.add_constructor(HAKCScope.yaml_tag, HAKCScope.from_yaml, Loader=loader)
            yaml.add_constructor(HAKCSymbol.yaml_tag, HAKCSymbol.from_yaml, Loader=loader)
            yaml.add_constructor(HAKCCompartment.yaml_tag, HAKCCompartment.from_yaml, Loader=loader)
            yaml.add_constructor(HAKCDivision.yaml_tag, HAKCDivision.from_yaml, Loader=loader)
            yaml.add_constructor(HAKCDefinitionLocation.yaml_tag, HAKCDefinitionLocation.from_yaml, Loader=loader)
            yaml.add_constructor(HAKCFunction.yaml_tag, HAKCFunction.from_yaml, Loader=loader)
            yaml.add_constructor(HAKCGlobalVariable.yaml_tag, HAKCGlobalVariable.from_yaml, Loader=loader)
            yaml.add_constructor(HAKCCompartmentalization.yaml_tag, HAKCCompartmentalization.from_yaml, Loader=loader)
            yaml.add_constructor(HAKCAdjustment.yaml_tag, HAKCAdjustment.from_yaml, Loader=loader)
            yaml.add_constructor(HAKCCompartmentalizationAdjustment.yaml_tag,
                                 HAKCCompartmentalizationAdjustment.from_yaml, Loader=loader)

    def save_as_yaml(self, filename: str):
        logger.debug(f'Saving compartmentalization to {filename}')
        # Note: python script will now create the file and directories, rather than the cpp script
        directory = os.path.dirname(filename)
        if not os.path.exists(directory):
            os.makedirs(directory)
        with open(filename, "w") as f:
            # Note: Don't use yaml.CDumper, it does something that breaks the loader
            yaml.dump(self, f, Dumper=yaml.Dumper)
        logger.info(f"Saved {self} to {filename}")
