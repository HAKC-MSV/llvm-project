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
from .HAKCObjects import HAKCSymbol, HAKCCompilationUnit, HAKCFunction, HAKCType, HAKCCompartment, HAKCDivision, \
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

    def __init__(self, division_count: Optional[int] = 16, nxgraph: Optional[nx.MultiDiGraph] = None):
        yaml.YAMLObject.__init__(self)
        nx.MultiDiGraph.__init__(self)
        self.division_count = division_count
        if nxgraph is not None:
            for division in self.get_filtered_nodes(nxgraph, node_filter=lambda n: isinstance(n, HAKCDivision))():
                for nbr in nxgraph.neighbors(division):
                    for edge_data in nxgraph.get_edge_data(division, nbr):
                        if edge_data == HAKCDivision.InCompartmentTable:
                            division.compartment_id = nbr.compartment_id
                            self.add_division(division, nbr)

            symbol_mapping = dict()
            for symbol in self.get_filtered_nodes(nxgraph,
                                                  node_filter=lambda n: isinstance(n, HAKCFunction) or isinstance(n,
                                                                                                                  HAKCGlobalVariable)):
                compilation_unit = None
                division: Optional[HAKCDivision] = None
                if isinstance(symbol, HAKCFunction):
                    new_symbol = HAKCFunction(Name=symbol.name)
                else:
                    new_symbol = HAKCGlobalVariable(Name=symbol.name)
                for nbr in nxgraph.neighbors(symbol):
                    for edge_data in nxgraph.get_edge_data(symbol, nbr):
                        if edge_data == HAKCSymbol.IsTypeTable:
                            new_symbol.type = nbr
                        elif edge_data == HAKCSymbol.HasScopeTable:
                            new_symbol.scope = nbr
                        elif edge_data == HAKCSymbol.SymbolCompilationUnitTable:
                            compilation_unit = nbr
                        elif edge_data == HAKCSymbol.InDivisionTable:
                            division = nbr
                        elif edge_data == HAKCSymbol.DefinedInTable:
                            new_symbol.defining_file = nbr.filename
                            new_symbol.defining_line = nxgraph.get_edge_data(symbol, nbr, key=edge_data)['line']

                new_symbol.computed_hash = None
                self.add_symbol(new_symbol, compilation_unit)
                self.set_division(new_symbol, division.division_id, division.compartment_id)
                symbol_mapping[symbol] = new_symbol

            for head, tail, edge_name, edge_data in nxgraph.edges.data(keys=True):
                if edge_name in {HAKCSymbol.UsesSymbolTable, HAKCSymbol.DagEdgeTable}:
                    new_head = symbol_mapping[head]
                    new_tail = symbol_mapping[tail]
                    if edge_name == HAKCSymbol.UsesSymbolTable:
                        self.add_symbol_use(new_head, new_tail)
                    elif edge_name == HAKCSymbol.DagEdgeTable:
                        self.add_dag_edge(new_head, new_tail, edge_data['weight'])

    @classmethod
    def from_yaml(cls, loader: yaml.Loader, node):
        graph_data = loader.construct_mapping(node, deep=True)
        graph = json_graph.node_link_graph(graph_data, edges='edges')
        compartmentalization = cls(division_count=graph_data.get('division_count', 16), nxgraph=graph)

        return compartmentalization

    @classmethod
    def to_yaml(cls, dumper: yaml.Dumper, data):
        compartmentalization_data = json_graph.node_link_data(data, edges='edges')
        compartmentalization_data['division_count'] = data.division_count
        return dumper.represent_mapping(cls.yaml_tag, compartmentalization_data)

    def add_dag_edge(self, head: HAKCSymbol, tail: HAKCSymbol, dag_edge_weight: int, add_nodes: bool = True):
        if dag_edge_weight > 0:
            self.add_persistent_edge(head, tail, key=HAKCSymbol.DagEdgeTable, add_nodes=add_nodes,
                                     weight=dag_edge_weight)

    def add_persistent_node(self, node: HAKCDBNode, already_persisted: bool = False):
        if not self.has_node(node):
            attrs = {HAKCCompartmentalization.persisted_attr: already_persisted}
            self.add_node(node, **attrs)

    def add_persistent_edge(self, u_for_edge: HAKCDBNode, v_for_edge: HAKCDBNode, key, add_nodes: bool = True, **attr):
        if add_nodes:
            self.add_persistent_node(u_for_edge)
            self.add_persistent_node(v_for_edge)
        if not self.has_edge(u_for_edge, v_for_edge, key):
            attr[HAKCCompartmentalization.persisted_attr] = False
            self.add_edge(u_for_edge, v_for_edge, key, **attr)

    def add_symbol_use(self, symbol: HAKCSymbol, used_symbol: HAKCSymbol, key=HAKCSymbol.UsesSymbolTable):
        self.add_persistent_edge(symbol, used_symbol, key=key)

    def add_symbol_type_use(self, symbol: HAKCSymbol, used_type: HAKCType, key=HAKCSymbol.IsTypeTable):
        self.add_persistent_edge(symbol, used_type, key=key)

    def add_symbol(self, symbol: HAKCSymbol, compilation_unit: HAKCCompilationUnit):
        self.add_symbol_type_use(symbol, symbol.type)
        self.add_persistent_edge(symbol, symbol.scope, key=HAKCSymbol.HasScopeTable)
        if compilation_unit is not None:
            self.add_persistent_edge(symbol, compilation_unit, key=HAKCSymbol.SymbolCompilationUnitTable)
        if symbol.defining_file is not None:
            self.add_persistent_edge(symbol, HAKCCompilationUnit(filename=symbol.defining_file),
                                     key=HAKCSymbol.DefinedInTable, line=symbol.defining_line)

        for used_symbol in symbol.used_symbols:
            self.add_symbol_use(symbol, used_symbol)

        if isinstance(symbol, HAKCFunction):
            for indirect_call in symbol.indirect_calls:
                self.add_symbol_type_use(symbol, indirect_call.type, key=HAKCFunction.IndirectCallTable)
            for direct_call in symbol.direct_calls:
                self.add_symbol_use(symbol, direct_call, key=HAKCFunction.DirectCallTable)

    def _get_neighbors(self, symbol: HAKCSymbol, edge_key: str) -> list:
        nbrs = list()
        if symbol not in self.nodes:
            for existing_symbol in self.get_symbols():
                if existing_symbol.name == symbol.name:
                    symbol_hash_inputs = symbol.get_hash_inputs()
                    existing_hash_inputs = symbol.get_hash_inputs()
                    for symbol_hash_input, existing_hash_input in zip(symbol_hash_inputs, existing_hash_inputs):
                        print(f'{hash(symbol_hash_input)} ? {hash(existing_hash_input)}')
                    symbol.computed_hash = None
                    existing_symbol.computed_hash = None
                    print(f'{hash(symbol)} ? {hash(existing_symbol)}')
                    break
            raise RuntimeError(f'Symbol {symbol} could not be found')
        for nbr, edges in self.adj[symbol].items():
            if edge_key in edges:
                nbrs.append(nbr)
        return nbrs

    def get_indirect_calls(self, symbol: HAKCSymbol) -> list[HAKCType]:
        return self._get_neighbors(symbol, HAKCFunction.IndirectCallTable)

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

    def set_division(self, symbol: HAKCSymbol, division_id: int, compartment_id: int):
        division = self.get_division_node(division_id, compartment_id)
        compartment = self.get_compartment_node(compartment_id)
        if compartment is None:
            compartment = HAKCCompartment(compartment_id, division_count=self.division_count)
        if division is None:
            division = HAKCDivision(division_id, compartment_id, division_count=compartment.division_count)
        self.set_symbol_division(symbol, division, compartment)

    def add_division(self, division: HAKCDivision, compartment: HAKCCompartment):
        compartment.add_division(division)
        self.add_persistent_edge(division, compartment, key=HAKCDivision.InCompartmentTable)

    def set_symbol_division(self, symbol: HAKCSymbol, division: HAKCDivision, compartment: HAKCCompartment):
        self.add_division(division, compartment)
        self.add_persistent_edge(symbol, division, key=HAKCSymbol.InDivisionTable)

    def get_division(self, symbol: HAKCSymbol) -> HAKCDivision | None:
        nbrs = self._get_neighbors(symbol, HAKCSymbol.InDivisionTable)
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
        return self.get_filtered_nodes(self, node_filter=lambda n: isinstance(n, HAKCFunction) or isinstance(n,
                                                                                                             HAKCGlobalVariable))

    def get_scopes(self):
        return self.get_filtered_nodes(self, node_filter=lambda n: isinstance(n, HAKCScope))

    def get_compilation_units(self):
        return self.get_filtered_nodes(self, node_filter=lambda n: isinstance(n, HAKCCompilationUnit))

    def get_divisions(self):
        return self.get_filtered_nodes(self, node_filter=lambda n: isinstance(n, HAKCDivision))

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

    def create_node_table(self, conn: HAKCDatabase, node_class: Type[HAKCDBNode]):
        logger.debug(f'Creating node table {node_class.get_table_name()}')
        conn.create_node_table(node_class)

    def create_rel_table(self, conn: HAKCDatabase, database_relation: HAKCDBRelation):
        logger.debug(f'Creating relation table {database_relation}')
        conn.create_relationship_table(database_relation)

    def create_schema(self, conn: HAKCDatabase):
        node_tables = HAKCCompartmentalization.get_table_classes()
        for cls in logger.progress_bar(iterable=node_tables, desc='Creating data tables'):
            self.create_node_table(conn, node_class=cls)

        for cls in logger.progress_bar(iterable=node_tables, desc='Creating relationship tables'):
            db_relations = cls.get_db_relations()
            for db_relation in db_relations:
                self.create_rel_table(conn, db_relation)

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
