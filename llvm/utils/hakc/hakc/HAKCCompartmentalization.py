import logging
import re
from typing import Type, Optional

import networkx as nx
import pandas as pd
import yaml
from networkx.readwrite import json_graph

from .HAKCBase import HAKCDivisionEnum, HAKCDBNode, HAKCDBRelation
from .HAKCDatabase import HAKCDatabase
from .HAKCLogger import HAKCLogger
from .HAKCObjects import HAKCSymbol, HAKCCompilationUnit, HAKCFunction, HAKCType, HAKCTypePerm, HAKCCompartment, HAKCDivision, \
    HAKCScope, HAKCGlobalVariable

logging.setLoggerClass(HAKCLogger)

logger: HAKCLogger = logging.getLogger('hakc-compartmentalization')


class HAKCCompartmentalization(yaml.YAMLObject, nx.MultiDiGraph):
    kernel_compartment_id = 0
    kernel_division: int = HAKCDivisionEnum.NO_DIVISION.value
    default_division: int = HAKCDivisionEnum.TEAL_DIVISION.value
    DefaultDivisionCount = max(1, len(HAKCDivisionEnum) - 1)
    persisted_attr = 'persisted'
    yaml_tag = "!HAKCCompartmentalization"

    def __init__(self, max_division_count: Optional[int] = 16, nxgraph: Optional[nx.MultiDiGraph] = None):
        yaml.YAMLObject.__init__(self)
        nx.MultiDiGraph.__init__(self)
        self.max_division_count = max_division_count
        # if nxgraph is not None:
        #     for division in self.get_filtered_nodes(nxgraph, node_filter=lambda n: isinstance(n, HAKCDivision))():
        #         for nbr in nxgraph.neighbors(division):
        #             for edge_data in nxgraph.get_edge_data(division, nbr):
        #                 if edge_data == HAKCDivision.InCompartmentTable:
        #                     division.compartment_id = nbr.compartment_id
        #                     self.add_division(division, nbr)
        #
        #     symbol_mapping = dict()
        #     for symbol in self.get_filtered_nodes(nxgraph, node_filter=lambda n: isinstance(n, HAKCFunction) or isinstance(n, HAKCGlobalVariable)):
        #         compilation_unit = None
        #         division: Optional[HAKCDivision] = None
        #         if isinstance(symbol, HAKCFunction):
        #             new_symbol = HAKCFunction(Name=symbol.name)
        #         else:
        #             new_symbol = HAKCGlobalVariable(Name=symbol.name)
        #         for nbr in nxgraph.neighbors(symbol):
        #             for edge_data in nxgraph.get_edge_data(symbol, nbr):
        #                 if edge_data == HAKCSymbol.IsTypeTable:
        #                     new_symbol.type = nbr
        #                 elif edge_data == HAKCSymbol.HasScopeTable:
        #                     new_symbol.scope = nbr
        #                 elif edge_data == HAKCSymbol.SymbolCompilationUnitTable:
        #                     compilation_unit = nbr
        #                 elif edge_data == HAKCSymbol.InDivisionTable:
        #                     division = nbr
        #                 elif edge_data == HAKCSymbol.DefinedInTable:
        #                     new_symbol.defining_file = nbr.filename
        #                     new_symbol.defining_line = nxgraph.get_edge_data(symbol, nbr, key=edge_data)['line']
        #
        #         new_symbol.computed_hash = None
        #         self.add_symbol(new_symbol, compilation_unit)
        #         self.set_division(new_symbol, division.division_id, division.compartment_id)
        #         symbol_mapping[symbol] = new_symbol
        #
        #     for head, tail, edge_name, edge_data in nxgraph.edges.data(keys=True):
        #         if edge_name in {HAKCSymbol.UsesSymbolTable, HAKCSymbol.DagEdgeTable, HAKCFunction.TypesUsedTable}:
        #             new_head = symbol_mapping[head]
        #             new_tail = symbol_mapping[tail]
        #             if edge_name == HAKCSymbol.UsesSymbolTable:
        #                 self.add_symbol_use(new_head, new_tail)
        #             elif edge_name == HAKCSymbol.DagEdgeTable:
        #                 self.add_dag_edge(new_head, new_tail, edge_data['weight'])
        #             elif edge_name == HAKCFunction.TypesUsedTable:
        #                 self.add_type_perm_type_edge(new_head, new_tail)


    def load_from_db(self, conn: HAKCDatabase):
        assert(isinstance(conn, HAKCDatabase))
        functions = conn.get_functions()
        for function in functions:
            self.__add_function(function)
            dag_edges = conn.get_dag_edges(function)
            for dag_edge in dag_edges:
                self.add_dag_edge(dag_edge[0], function, dag_edge[1])
                # print(f"Adding dag_edge: ({function})-[{dag_edge[1]}]->({dag_edge[0]})")
            division, compartment, = conn.get_division_compartment(function)
            self.__add_division_compartment(function, division, compartment)

        self.save_graph("loaded_from_db.png")

        print(self)

    @classmethod
    def to_yaml(cls, dumper: yaml.Dumper, data):
        compartmentalization_data = json_graph.node_link_data(data, edges='edges')
        compartmentalization_data['max_division_count'] = data.max_division_count
        return dumper.represent_mapping(cls.yaml_tag, compartmentalization_data)

    def add_symbols(self, functions: list[HAKCFunction], global_variables: list[HAKCGlobalVariable]):

        for global_variable in global_variables:
            self.__add_global_variable(global_variable)

        for function in functions:
            self.__add_function(function)

    def __add_function(self, function: HAKCFunction):
        assert(isinstance(function, HAKCFunction))

        # add symbol with all its attributes
        self.__add_symbol(function)
        # print(self)

        # add function specific attributes,
        for direct_call in function.direct_calls:
            assert(isinstance(direct_call, HAKCFunction))
            self.__add_symbol(direct_call)
            self.__add_persistent_edge(function, direct_call, key=HAKCFunction.relation_direct_calls)

        for indirect_call in function.indirect_calls:
            assert(isinstance(indirect_call, HAKCType))
            self.__add_type(indirect_call)
            self.__add_persistent_edge(function, indirect_call, key=HAKCFunction.relation_indirect_calls)

        for type_used in function.types_used:
            assert(isinstance(type_used, HAKCTypePerm))
            self.__add_persistent_edge(function, type_used.perm_type, key=HAKCFunction.relation_types_used, R = (type_used.RWX & 0b100) >> 2, W = (type_used.RWX & 0b010) >> 1, X = (type_used.RWX & 0b001))

    def __add_type(self, _type: HAKCType):
        assert(isinstance(_type, HAKCType))
        self.__add_persistent_node(_type)

    def add_symbol(self, symbol):
        # function to allow for external access
        self.__add_symbol(symbol)

    def __add_symbol(self, symbol: HAKCSymbol):
        # add 'sanitized' version of the HAKCSymbol (e.g., don't include HAKCTypePerm as a node, since it should be an edge?)
        assert(isinstance(symbol, HAKCSymbol))

        # add the symbol node
        self.__add_persistent_node(symbol)

        # now add all the edges from the symbol node to node (and add the nodes at the same time!)
        self.__add_persistent_edge(symbol, symbol.type, key=HAKCSymbol.relation_type)
        self.__add_persistent_edge(symbol, symbol.scope, key=HAKCSymbol.relation_scope)
        if symbol.compilation_unit:
            # print(f"Adding CompilationUnit! {symbol}")
            self.__add_persistent_edge(symbol, symbol.compilation_unit, key=HAKCSymbol.relation_compilation_unit, DefiningLine=symbol.compilation_unit.defining_line)
        for used_symbol in symbol.used_symbols:
            self.__add_persistent_edge(symbol, used_symbol, key=HAKCSymbol.relation_symbol)


    def __add_global_variable(self, global_variable: HAKCGlobalVariable):
        assert(isinstance(global_variable, HAKCGlobalVariable))
        self.__add_symbol(global_variable)


    def add_dag_edge(self, head: HAKCSymbol, tail: HAKCSymbol, dag_edge_weight: int):
        if dag_edge_weight > 0:
            self.__add_persistent_edge(head, tail, key=HAKCSymbol.relation_dag, weight=dag_edge_weight)

    def __add_persistent_node(self, node: HAKCDBNode, already_persisted: bool = False):
        # Note: networkx determines if a node is already in the graph if the id(node) exists, meaning the memory address, not the actual hash
        # need to maintain internal map of object hashes to the actual networkx node (prevent duplicates, ensure data normalcy)
        assert(isinstance(node, HAKCDBNode))
        if node not in self:
            attrs = {HAKCCompartmentalization.persisted_attr: already_persisted}
            self.add_node(node, **attrs)
            # self.node_map[node_hash] = node
        # assert self.node_map[node_hash] == node, f"{self.node_map[node_hash]} =?= {node}"
        # print(f"FOUND {self.node_map[node_hash]} =?= {node}")
        # return self.node_map[node_hash]
        return node

    def get_compartment_from_division(self, division: HAKCDivision):
        assert(isinstance(division, HAKCDivision))
        for edge in self.edges(division):
            compartment = edge[1]
            if isinstance(compartment, HAKCCompartment):
                return compartment
        assert False, f"Division {division} should always return a HAKCCompartment!"

    def get_divisions_compartments(self):
        nodes = set()
        for division in self.get_filtered_nodes(self, node_filter=lambda n: isinstance(n, HAKCDivision)):
            compartment = self.get_compartment_from_division(division)
            nodes.add((division, compartment))
        return nodes

    def __compute_tokens(self):
        pass
        for division, compartment in self.get_divisions_compartments():
            compartment.entry_token = self.compute_entry_token(compartment.compartment_id)
            division.access_token = self.compute_access_token(division.division_id, compartment.compartment_id)
            print(f"Computed access token and entry token for {division}, and {compartment}")


    def __add_division_compartment(self, symbol: HAKCSymbol, division: HAKCDivision, compartment: HAKCCompartment):
        assert(isinstance(symbol, HAKCSymbol))
        assert(isinstance(division, HAKCDivision))
        assert(isinstance(compartment, HAKCCompartment))

        self.__add_persistent_edge(symbol, division, key=HAKCSymbol.relation_division)
        self.__add_persistent_edge(division, compartment, key=HAKCDivision.relation_compartment)

    def add_default_compartmentalization(self, conn: HAKCDatabase, create_schema: bool = False):
        compartment_id = HAKCCompartmentalization.kernel_compartment_id + 1
        division_id = HAKCCompartmentalization.default_division
        symbols = list(self.get_symbols())
        # first construct all the compartments and divisions
        for symbol in logger.progress_bar(iterable=symbols, desc='Adding default compartmentalization'):
            compartment = HAKCCompartment(compartment_id)
            # setting access_token to compartment_id temporarily so multiple HAKCDivision objects can be created (otherwise the hashes would be the same, and only one would be created)
            division = HAKCDivision(division_id, compartment_id)
            self.__add_division_compartment(symbol, division, compartment)
            compartment_id += 1
        # then compute all the tokens
        self.__compute_tokens()
        self.persist_to_database(conn, create_schema=create_schema)

    def compute_entry_token(self,compartment_id: int) -> int:
        token = compartment_id << self.max_division_count
        for division in self.get_divisions():
            token |= (1 << division.division_id)
        return token

    def compute_access_token(self, division_id: int, compartment_id: int, allowable_accesses: list['HAKCDivision'] = []) -> int:
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

    def __add_persistent_edge(self, u_for_edge: HAKCDBNode, v_for_edge: HAKCDBNode, key, **attr):
        if not(self.has_edge(u_for_edge, v_for_edge, key)):
            attr[HAKCCompartmentalization.persisted_attr] = False
            self.add_edge(u_for_edge, v_for_edge, key, **attr)

    def _get_neighbors(self, symbol: HAKCSymbol, edge_key: str) -> list:
        nbrs = list()
        if symbol not in self.nodes:
            for existing_symbol in self.get_symbols():
                if existing_symbol.name == symbol.name:
                    symbol_hash_inputs = symbol.get_hash_inputs()
                    existing_hash_inputs = symbol.get_hash_inputs()
                    # for symbol_hash_input, existing_hash_input in zip(symbol_hash_inputs, existing_hash_inputs):
                        # print(f'{hash(symbol_hash_input)} ? {hash(existing_hash_input)}')
                    symbol.computed_hash = None
                    existing_symbol.computed_hash = None
                    # print(f'{hash(symbol)} ? {hash(existing_symbol)}')
                    break
            raise RuntimeError(f'Symbol {symbol} could not be found')
        for nbr, edges in self.adj[symbol].items():
            if edge_key in edges:
                nbrs.append(nbr)
        return nbrs

    def get_indirect_calls(self, symbol: HAKCSymbol) -> list[HAKCType]:
        return self._get_neighbors(symbol, HAKCFunction.relation_indirect_calls)

    def get_compartment_node(self, compartment_id: int):
        for compartment in self.get_filtered_nodes(self, node_filter=lambda
                n: isinstance(n, HAKCCompartment)):
            if compartment.compartment_id == compartment_id:
                return compartment
        return None

    def get_division_node(self, division_id: int, compartment_id: int):
        for division in self.get_filtered_nodes(self, node_filter=lambda n: isinstance(n, HAKCDivision)):
            if division_id == division.division_id and division.compartment_id == compartment_id:
                return division
        return None



    # def set_division(self, symbol: HAKCSymbol, division_id: int, compartment_id: int):
    #     division = self.get_division_node(division_id, compartment_id)
    #     compartment = self.get_compartment_node(compartment_id)
    #     if compartment is None:
    #         compartment = HAKCCompartment(compartment_id, max_division_count=self.max_division_count)
    #     if division is None:
    #         division = HAKCDivision(division_id, compartment_id, max_division_count=compartment.max_division_count)
    #     self.set_symbol_division(symbol, division, compartment)

    # def add_division(self, division: HAKCDivision, compartment: HAKCCompartment):
    #     compartment.add_division(division)
    #     self.__add_persistent_edge(division, compartment, key=HAKCDivision.relation_compartment)
    #
    # def set_symbol_division(self, symbol: HAKCSymbol, division: HAKCDivision, compartment: HAKCCompartment):
    #     # print(f"Adding symbol division for {symbol} in division {division} with compartment {compartment}")
    #     self.add_division(division, compartment)
    #     self.__add_persistent_edge(symbol, division, key=HAKCSymbol.relation_division)

    def get_division(self, symbol: HAKCSymbol) -> HAKCDivision | None:
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
    def get_filtered_nodes(G: nx.MultiDiGraph, node_filter):
        return nx.subgraph_view(G, filter_node=node_filter).nodes

    def get_types(self):
        return self.get_filtered_nodes(self, node_filter=lambda n: isinstance(n, HAKCType))

    def get_functions(self):
        return self.get_filtered_nodes(self, node_filter=lambda n: isinstance(n, HAKCFunction))

    def get_global_variables(self):
        return self.get_filtered_nodes(self, node_filter=lambda n: isinstance(n, HAKCGlobalVariable))

    def get_symbols(self):
        return self.get_filtered_nodes(self, node_filter=lambda n: isinstance(n, HAKCFunction) or isinstance(n, HAKCGlobalVariable))

    def get_scopes(self):
        return self.get_filtered_nodes(self, node_filter=lambda n: isinstance(n, HAKCScope))

    def get_compilation_units(self):
        return self.get_filtered_nodes(self, node_filter=lambda n: isinstance(n, HAKCCompilationUnit))

    def get_divisions(self):
        return self.get_filtered_nodes(self, node_filter=lambda n: isinstance(n, HAKCDivision))

    def get_symbol_by_name(self, symbol_name):
        return self.get_filtered_nodes(self, node_filter=lambda n: (isinstance(n, HAKCFunction) or isinstance(n, HAKCGlobalVariable)) and n.name == symbol_name)

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

        # now search all valid neighbors
        for caller in all_symbols_by_compartment_id:
            for callee in self.neighbors(caller):
                if self.has_edge(caller, callee, HAKCSymbol.DagEdgeTable):
                    edge_weight = self.get_edge_data(caller, callee, HAKCSymbol.DagEdgeTable)['weight']
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
                if len(data_to_persist) > 0 and len(db_data) != len(data_to_persist):
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
                logger.error(f'Failed to persist to {table_name}: {str(e)}')
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

    def persist_to_database(self, conn: HAKCDatabase, create_schema: bool = False):
        if create_schema:
            logger.info(f'Creating schema')
            self.create_schema(conn)
        logger.info('Persisting new nodes to database')
        self._persist_nodes(conn)
        logger.info('Persisting new edges to database')
        self._persist_edges(conn)

    def create_schema(self, conn: HAKCDatabase):
        node_tables = HAKCCompartmentalization.get_table_classes()
        for cls in logger.progress_bar(iterable=node_tables, desc='Creating data tables'):
            conn.create_node_table(node_type=cls)

        for cls in logger.progress_bar(iterable=node_tables, desc='Creating relationship tables'):
            db_relations = cls.get_db_relations()
            for db_relation in db_relations:
                conn.create_relationship_table(edge_type=db_relation)

    def get_symbol_hashes(self) -> dict[int, HAKCSymbol]:
        symbol_hashes = dict()
        for symbol in self.get_symbols():
            symbol_hashes[hash(symbol)] = symbol
        return symbol_hashes

    def get_symbol_by_hash(self, symbol_hash: int) -> HAKCSymbol | None:
        for symbol in self.get_symbols():
            if hash(symbol) == symbol_hash:
                return symbol

        return None

    @staticmethod
    def get_table_classes() -> list[Type[HAKCDBNode]]:
        return [
            HAKCType,
            HAKCScope,
            HAKCSymbol,
            HAKCCompartment,
            HAKCDivision,
            HAKCCompilationUnit,
            HAKCFunction
        ]

    @staticmethod
    def show_top_n_degrees(G, n):
        """
        Returns a list of the top n nodes with the highest degrees of freedom.

        Args:
        graph: A NetworkX graph object.
        n: The number of top nodes to return.

        Returns:
        A list of node IDs sorted by degree in descending order.
        """
        degrees = dict(nx.degree(G))

        # Sort the degrees in descending order
        sorted_degrees = sorted(degrees.items(), key=lambda item: item[1], reverse=True)

        return [node for node, degree in sorted_degrees[:n]]


    def save_graph(self, fname, symbol_name = None, prune = False, N = 50):
        from networkx.drawing.nx_pydot import to_pydot
        print(f"Trying to save {self} to {fname}")
        G = self.subgraph(self.nodes)
        # node_filter = lambda n: (isinstance(n, HAKCFunction) or isinstance(n, HAKCGlobalVariable)) and n.name == symbol_name
        # _symbol = nx.subgraph_view(G, filter_node=node_filter).nodes()
        functions = self.get_functions()
        print(functions)
        symbol = [n for n in functions if n.name == symbol_name]
        print(f"Found {symbol} of type {type(symbol)}")
        # symbol = self.nodes[_symbol]

        # symbol = self.get_symbol_by_name(symbol_name)
        # print(f"Found symbol {symbol} in {len(symbol)}")
        symbol_descendents = nx.descendants(G, symbol[0])
        # print(f"{symbol} has {symbol_descendents} descendents!")
        G = G.subgraph(symbol_descendents)

        if prune:
            top_n_node_ids = HAKCCompartmentalization.show_top_n_degrees(G, N)
            subgraph = G.subgraph(top_n_node_ids)
            # print(G.nodes)
            G = subgraph

        G = nx.relabel_nodes(G, lambda x: x.pretty_print())

        dot = to_pydot(G)
        dot.set_splines("true")
        dot.set("overlap", "false")
        dot.set("rankdir","LR")
        for node in dot.get_nodes():
            node.set("shape", "box")
        # set edge labels
        for edge in dot.get_edges():
            # print(edge.obj_dict)
            # ignore all 'persisted' attributes
            if "persisted" in edge.obj_dict["attributes"]:
                del edge.obj_dict["attributes"]["persisted"]

            edge.set_label(str(edge.obj_dict["attributes"]))

        # dot.write_png(fname)  # Requires Graphviz installed
        dot.write_svg(fname)

        """
        node_degrees = sorted(G_ddg.degree, key=lambda x: x[1], reverse=True)
        # prune graph to only have this graph
        print(f"Nodes in DDG: {G_ddg.nodes()}")
        MaxNodeDescendants = nx.descendants(G_ddg, MaxNodeName)
        print(f"MaxNode Descendents {MaxNodeDescendants}")
        G_ddg = G_ddg.subgraph(MaxNodeDescendants)
        print(f"Max node {MaxNodeName} -> {MaxNodeAttrs} with found hash {MaxNodeHash}")
        """


        print(f"Saved {self} to {fname}")

    def save_graph_alt(self, fname):
        import matplotlib.pyplot as plt
        from networkx.drawing.nx_agraph import graphviz_layout

        G = self.subgraph(self.nodes)
        G = nx.relabel_nodes(G, lambda x: x.pretty_print())

        pos = graphviz_layout(G, prog='dot', args='-Grankdir="LR" -Gminlen="1"')

        plt.figure(figsize=(16,9))
        nx.draw(G, pos, with_labels=True, node_shape="o", node_color='none', edgecolors='black')
        nx.draw_networkx_edge_labels(G, pos)
        # print(G.edges)
        plt.savefig(fname)
        plt.close()
        print(f"Saved {self} to {fname}")
