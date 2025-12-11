import logging
import multiprocessing as mp
import threading
from typing import Type, Optional, Tuple, cast

import kuzu
import numpy
import pandas as pd

from .HAKCBase import HAKCDBNode, HAKCDBRelation
from .HAKCLogger import HAKCLogger
from .HAKCObjects import HAKCSymbol, HAKCFunction, HAKCScope, HAKCType, HAKCGlobalVariable, \
    HAKCDivision, HAKCCompartment, HAKCDefinitionLocation

logging.setLoggerClass(HAKCLogger)

logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc.database'))


class HAKCDatabase:
    def __init__(self, db_dir: str, read_only: bool = False, max_num_threads=int(mp.cpu_count() / 2)):
        self.db_dir = db_dir
        self.database = None
        self.conn = None
        self.open(read_only=read_only, max_num_threads=max_num_threads)
        # mutexs to share data among each server thread
        self.query_cache_mutex = threading.Lock()
        self.query_cache = {}

    def __del__(self):
        self.close()

    @staticmethod
    def __get_class_db_columns(cls):
        return [x.column_name for x in cls.get_data_columns()] + [cls.get_primary_key().column_name]

    @staticmethod
    def create_query_key(stmt: str, **kwargs):
        return stmt.replace(' ', '') + str(tuple(sorted(kwargs.items())))

    def create_object_from_df(self, cls, data, deep=False, AccessToken: int = 0, EntryToken: int = 0):
        if data.empty:
            return None
        if cls == HAKCSymbol and 'HAKCSymbol.IsFunction' in data:
            cls = HAKCFunction if data['HAKCSymbol.IsFunction'] else HAKCGlobalVariable
        if cls == HAKCScope:
            return HAKCScope(Scope=data['HAKCScope.Scope'],
                             LocalScopeName=data[
                                 'HAKCScope.LocalScopeName'] if 'HAKCScope.LocalScopeName' in data else None)
        elif cls == HAKCType:
            return HAKCType(LLVMType=data['HAKCType.LLVMType'],
                            DebugType=data['HAKCType.DebugType'] if 'HAKCType.DebugType' in data else None)
        elif cls == HAKCDefinitionLocation:
            return HAKCDefinitionLocation(DefiningFile=data['HAKCDefinitionLocation.DefiningFile'] if isinstance(
                data['HAKCDefinitionLocation.DefiningFile'], str) else None,
                                          DefiningLine=int(data[
                                                               'has_definition_location.DefiningLine']) if 'has_definition_location.DefiningLine' in data and not numpy.isnan(
                                              data['has_definition_location.DefiningLine']) else None)
        elif cls == HAKCFunction:
            function_hash = data['HAKCSymbol.symbol_hash']
            return HAKCFunction(Name=data['HAKCSymbol.Name'],
                                Type=HAKCType(
                                    LLVMType=data['HAKCType.LLVMType'],
                                    DebugType=data['HAKCType.DebugType'] if 'HAKCType.DebugType' in data else None),
                                Scope=HAKCScope(
                                    Scope=data['HAKCScope.Scope'],
                                    LocalScopeName=data[
                                        'HAKCScope.LocalScopeName'] if 'HAKCScope.LocalScopeName' in data else None),
                                DefinitionLocation=HAKCDefinitionLocation(
                                    DefiningFile=data['HAKCDefinitionLocation.DefiningFile'] if isinstance(data[
                                                                                                               'HAKCDefinitionLocation.DefiningFile'],
                                                                                                           str) else None,
                                    DefiningLine=int(data[
                                                         'has_definition_location.DefiningLine'])) if 'has_definition_location.DefiningLine' in data and not numpy.isnan(
                                    data['has_definition_location.DefiningLine']) else None,
                                UsedSymbols=self.get_used_symbols(function_hash) if deep else None,
                                DirectCalls=self.get_direct_calls(function_hash) if deep else None,
                                IndirectCalls=self.get_indirect_calls(function_hash) if deep else None)
        elif cls == HAKCGlobalVariable:
            return HAKCGlobalVariable(Name=data['HAKCSymbol.Name'],
                                      Type=HAKCType(
                                          LLVMType=data['HAKCType.LLVMType'],
                                          DebugType=data[
                                              'HAKCType.DebugType'] if 'HAKCType.DebugType' in data else None),
                                      Scope=HAKCScope(
                                          Scope=data['HAKCScope.Scope'],
                                          LocalScopeName=data[
                                              'HAKCScope.LocalScopeName'] if 'HAKCScope.LocalScopeName' in data else None),
                                      DefinitionLocation=HAKCDefinitionLocation(
                                          DefiningFile=data['HAKCDefinitionLocation.DefiningFile'],
                                          DefiningLine=int(data[
                                                               'has_definition_location.DefiningLine'])) if 'has_definition_location.DefiningLine' in data else None)
        elif cls == HAKCDivision:
            # Note: double check AccessToken, EntryToken
            return HAKCDivision(DivisionID=int(data[f'HAKCDivision.{HAKCDivision.DivisionIDColumnName()}']),
                                Salt=int(data['HAKCDivision.Salt']),
                                AccessToken=AccessToken)
        elif cls == HAKCCompartment:
            return HAKCCompartment(CompartmentID=int(data['HAKCCompartment.CompartmentID']),
                                   EntryToken=EntryToken)
        raise RuntimeError(f"Trying to create invalid HAKC class from response: {cls} with data {data}")

    def get_stats(self) -> dict[str, int]:
        cmd = f"""
        MATCH (HAKCSymbol:HAKCSymbol)
        RETURN COUNT(HAKCSymbol) as symbols;
        """
        symbols = self.execute(cmd, enable_cache=False)
        cmd = f"""
        MATCH (HAKCType:HAKCType)
        RETURN COUNT(HAKCType) as types;
        """
        types = self.execute(cmd, enable_cache=False)
        cmd = f"""
        MATCH (HAKCScope:HAKCScope)
        RETURN COUNT(HAKCScope) as scopes;
        """
        scopes = self.execute(cmd, enable_cache=False)
        cmd = f"""
        MATCH (HAKCDefinitionLocation:HAKCDefinitionLocation)
        RETURN COUNT(HAKCDefinitionLocation) as definition_locations;
        """
        definition_locations = self.execute(cmd, enable_cache=False)
        cmd = f"""
        MATCH (HAKCDivision:HAKCDivision)
        RETURN COUNT(HAKCDivision) as divisions;
        """
        divisions = self.execute(cmd, enable_cache=False)
        cmd = f"""
        MATCH (HAKCCompartment:HAKCCompartment)
        RETURN COUNT(HAKCCompartment) as compartments;
        """
        compartments = self.execute(cmd, enable_cache=False)

        return {'symbols': int(symbols['symbols'][0]),
                'types': int(types['types'][0]),
                'scopes': int(scopes['scopes'][0]),
                'definition_locations': int(definition_locations['definition_locations'][0]),
                'divisions': int(divisions['divisions'][0]),
                'compartments': int(compartments['compartments'][0])}

    def __str__(self):
        stats = self.get_stats()
        sorted_keys = sorted([k for k in stats.keys()])
        stats_strings = [f'{stats[k]} {k}' for k in sorted_keys]
        return f'hakc-db with {",".join(stats_strings)}'

    def get_query_cache(self, key):
        return self.query_cache[key] if key in self.query_cache else None

    def set_query_cache(self, key, value):
        with self.query_cache_mutex:
            if key not in self.query_cache:
                self.query_cache[key] = value

    def clear_query_cache(self):
        # clear cache every time there is a write operation
        logger.debug(f"Clearing query cache after write operation")
        with self.query_cache_mutex:
            self.query_cache = {}

    def close(self):
        if self.conn is not None:
            self.conn.close()
        if self.database is not None:
            self.database.close()

    def open(self, read_only: bool = False, max_num_threads=int(mp.cpu_count() / 2)):
        self.database = kuzu.Database(self.db_dir, read_only=read_only, max_num_threads=max_num_threads)
        self.conn = kuzu.Connection(self.database)  # main connection

    def execute_prepared_stmt(self, prepared_stmt: str, **kwargs):
        return self.conn.execute(prepared_stmt, parameters=kwargs)

    def execute(self, prepared_stmt: str, enable_cache=True, **kwargs):
        # adding query level caching (essentially a wrapper for conn.execute)
        if enable_cache:
            key = HAKCDatabase.create_query_key(prepared_stmt, **kwargs)
            found = self.get_query_cache(key)
            if found is not None:
                return found
            value = self.execute_prepared_stmt(prepared_stmt, **kwargs).get_as_df()
            self.set_query_cache(key, value)
            return value
        return self.execute_prepared_stmt(prepared_stmt, **kwargs).get_as_df()

    def get_compartment_entry_token_from_id(self, compartment_id: int) -> Optional[int]:
        cmd = f"""
        MATCH (comp:{HAKCCompartment.get_table_name()})<-[:{str(HAKCDivision.relation_compartment)}]-({HAKCDivision.get_table_name()}:{HAKCDivision.get_table_name()})<-[:{str(HAKCSymbol.relation_division)}]-(:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_dag}]->(:{HAKCSymbol.get_table_name()})
        WHERE comp.{str(HAKCCompartment.get_primary_key())} = $compartment_id
        RETURN DISTINCT {HAKCDivision.get_table_name()}.{HAKCDivision.DivisionIDColumnName()} as DivisionID;
        """
        return HAKCCompartment.compute_entry_token(compartment_id,
                                                   set(int(entry['DivisionID']) for _, entry in
                                                       self.execute(cmd, compartment_id=compartment_id).iterrows()))

    def get_division_access_token_from_id(self, division_id: int, compartment_id: int) -> Optional[int]:
        cmd = f"""
        MATCH (comp:{HAKCCompartment.get_table_name()} {{{str(HAKCCompartment.get_primary_key())}: $compartment_id}}) 
        MATCH (div1:{HAKCDivision.get_table_name()} {{{HAKCDivision.DivisionIDColumnName()}: $division_id}})-[:{HAKCDivision.relation_compartment}]->(comp) 
        MATCH (div2:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(comp) 
        WHERE EXISTS {{ 
            MATCH (div2)<-[:{HAKCSymbol.relation_division}]-(:{HAKCSymbol.get_table_name()})<-[:{HAKCSymbol.relation_dag}]-(:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_division}]->(div1) 
        }} 
        RETURN div2.{HAKCDivision.DivisionIDColumnName()} AS DivisionID
        """
        database_ids = self.execute(cmd, division_id=division_id, compartment_id=compartment_id)['DivisionID'].tolist()
        return HAKCDivision.compute_access_token(compartment_id, {int(entry) for entry in database_ids})

    def get_division(self, division_id: int, compartment_id: int) -> Optional[HAKCDivision]:
        cmd = f"""
        MATCH ({HAKCDivision.get_table_name()}:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->({HAKCCompartment.get_table_name()}:{HAKCCompartment.get_table_name()})
        WHERE {HAKCDivision.get_table_name()}.{HAKCDivision.DivisionIDColumnName()} = $division_id AND {HAKCCompartment.get_table_name()}.{HAKCCompartment.get_primary_key()} = $compartment_id
        RETURN DISTINCT {HAKCDivision.get_table_name()}.{HAKCDivision.DivisionIDColumnName()}, {HAKCDivision.get_table_name()}.Salt;
        """
        data = self.execute(cmd, division_id=int(division_id), compartment_id=int(compartment_id))
        return self.create_object_from_df(HAKCDivision, data.iloc[0], AccessToken=int(
            self.get_division_access_token_from_id(division_id, compartment_id))) if len(data) > 0 else None

    def get_division_id_compartment_id_from_symbol(self, symbol: HAKCSymbol) -> Optional[
        Tuple[HAKCDivision, HAKCCompartment]]:
        cmd = f"""        
        MATCH (scope:{HAKCScope.get_table_name()})<-[:{HAKCSymbol.relation_scope}]-(sym:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_division}]->({HAKCDivision.get_table_name()}:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->({HAKCCompartment.get_table_name()}:{HAKCCompartment.get_table_name()})
        WHERE sym.Name = $Name AND scope.Scope = $Scope
        RETURN DISTINCT {HAKCDivision.get_attrs()}, {HAKCCompartment.get_attrs()};
        """
        data = self.execute(cmd, Name=symbol.name, Scope=symbol.scope.scope)
        if data.empty:
            return None
        all_data = data.iloc[0]
        division_id = int(all_data[f'{HAKCDivision.get_table_name()}.{HAKCDivision.DivisionIDColumnName()}'])
        compartment_id = int(all_data[f'{HAKCCompartment.get_table_name()}.{str(HAKCCompartment.get_primary_key())}'])

        return (self.create_object_from_df(HAKCDivision, all_data,
                                           AccessToken=self.get_division_access_token_from_id(
                                               division_id, compartment_id)),
                self.create_object_from_df(HAKCCompartment, all_data,
                                           EntryToken=self.get_compartment_entry_token(
                                               compartment_id)))

    def get_compartment_entry_token(self, compartment_id: int) -> int:
        cmd = f"""
        MATCH (:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_symbol}]->(:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_division}]->(d:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(c:{HAKCCompartment.get_table_name()}),
        (:{HAKCSymbol.get_table_name()})-[:{HAKCFunction.relation_indirect_calls}]->(:{HAKCType.get_table_name()})<-[:{HAKCSymbol.relation_type}]-(:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_division}]->(d)-[:{HAKCDivision.relation_compartment}]->(c)
        WHERE c.{str(HAKCCompartment.get_primary_key())} = $compartment_id
        RETURN DISTINCT d.{HAKCDivision.DivisionIDColumnName()} AS DivisionID
        """
        division_ids = set(self.execute(cmd, compartment_id=compartment_id)['DivisionID'].tolist())
        return HAKCCompartment.compute_entry_token(compartment_id=compartment_id, entry_divisions=division_ids)

    def get_valid_targets_from_compartment_id(self, source_compartment_id: int) -> list[int]:
        # NOTE: comp1 is fixed to the caller's compartment
        # from the perspective of the caller, the compartment_id is their own and they are looking for compartment_ids of targets
        # Get valid target compartments given compartment id
        # comp1 <- div1 <- symbol1 -(Dag2)-> symbol2 -> div2 -> comp2
        cmd = f"""
        MATCH (comp1:{HAKCCompartment.get_table_name()})<-[:{HAKCDivision.relation_compartment}]-(div1:{HAKCDivision.get_table_name()})<-[:{HAKCSymbol.relation_division}]-(sym1:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_dag}]->(sym2:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_division}]->(div2:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(comp2:{HAKCCompartment.get_table_name()})
        WHERE comp1.{str(HAKCCompartment.get_primary_key())} = $source_compartment_id
        RETURN DISTINCT comp2.{str(HAKCCompartment.get_primary_key())} AS CompartmentID;
        """
        return [int(entry['CompartmentID']) for _, entry in
                self.execute(cmd, source_compartment_id=source_compartment_id).iterrows()]

    def persist_dag_edges(self, dag_edge_data):
        head_hashes = list()
        tail_hashes = list()
        edge_weights = list()

        for head_hash, edge_data in dag_edge_data.items():
            for tail_hash, edge_weight in edge_data.items():
                head_hashes.append(head_hash)
                tail_hashes.append(tail_hash)
                edge_weights.append(edge_weight)

        df = pd.DataFrame({
            "from": head_hashes,
            "to": tail_hashes,
            "weight": edge_weights
        })
        self.insert_from_dataframe(HAKCSymbol.relation_dag, df)
        self.clear_query_cache()

    def get_all_symbol_hashes(self) -> list[int]:
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()})
        RETURN DISTINCT sym.{str(HAKCSymbol.get_primary_key())} AS symbol_hash;
        """
        return [entry['symbol_hash'] for _, entry in self.execute(cmd).iterrows()]

    def get_symbol_by_hash(self, symbol_hashes: list[int], **kwargs) -> list[HAKCSymbol]:
        if isinstance(symbol_hashes, tuple):
            symbol_hashes = list(symbol_hashes)
        assert isinstance(symbol_hashes, list), f"symbol_hashes is not of type list[int]: {type(symbol_hashes)}"
        return self._get_symbols(where=f'HAKCSymbol.symbol_hash in [{", ".join([str(sh) for sh in symbol_hashes])}]',
                                 **kwargs)

    def get_single_symbol_by_hash(self, symbol_hash: int) -> HAKCSymbol:
        assert (isinstance(symbol_hash, int))
        return self._get_symbols(symbol_hash=symbol_hash)[0]

    def get_symbols_by_name(self, symbol_name: str) -> list[HAKCSymbol]:
        return self._get_symbols(symbol_name=symbol_name)

    def create_schema_for_object(self, cls) -> None:
        self.create_node_table(node_type=cls)
        for db_relation in cls.get_db_relations():
            self.create_relationship_table(edge_type=db_relation)

    def delete_all_compartments(self):
        logger.debug(f"About to delete all compartments and divisions!")
        logger.debug(self)
        # Note: Dropping tables is much faster than a detach delete
        # drop each relation and node table associated with divisions and compartments
        self.execute_prepared_stmt(f"DROP TABLE {HAKCSymbol.relation_division};")
        self.execute_prepared_stmt(f"DROP TABLE {HAKCDivision.relation_compartment};")
        self.execute_prepared_stmt(f"DROP TABLE {HAKCCompartment.get_table_name()};")
        self.execute_prepared_stmt(f"DROP TABLE {HAKCDivision.get_table_name()};")
        # now add back in the deleted tables
        self.create_schema_for_object(HAKCCompartment)
        self.create_schema_for_object(HAKCDivision)
        self.create_schema_for_object(HAKCSymbol)
        logger.debug(f"Deleted all compartments and divisions")
        logger.debug(self)
        self.clear_query_cache()

    def get_all_divisions(self) -> list[HAKCDivision]:
        cmd = f"""
        MATCH ({HAKCDivision.get_table_name()}:{HAKCDivision.get_table_name()})
        RETURN DISTINCT {HAKCDivision.get_attrs()};
        """
        return [self.create_object_from_df(HAKCDivision, entry) for _, entry in self.execute(cmd).iterrows()]

    def get_all_compartments(self) -> list[HAKCCompartment]:
        cmd = f"""
        MATCH ({HAKCCompartment.get_table_name()}:{HAKCCompartment.get_table_name()})
        RETURN DISTINCT {HAKCCompartment.get_attrs()};
        """
        return [self.create_object_from_df(HAKCCompartment, entry) for _, entry in self.execute(cmd).iterrows()]

    def get_symbol_definition_location(self, symbol_hash: int) -> Optional[HAKCDefinitionLocation]:
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()})-[e:{HAKCSymbol.relation_definition_location}]->(dl:{HAKCDefinitionLocation.get_table_name()})
        WHERE sym.{str(HAKCSymbol.get_primary_key())} = $symbol_hash
        RETURN DISTINCT {HAKCDefinitionLocation.get_attrs()};
        """
        return self.create_object_from_df(HAKCDefinitionLocation, self.execute(cmd, symbol_hash=symbol_hash))

    def get_dag_computation_edges(self, symbol_hash: int) -> dict[str, list[int]]:
        result = dict()
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()})-[:{HAKCFunction.relation_indirect_calls}]->(:{HAKCType.get_table_name()})<-[:{HAKCSymbol.relation_type}]-(indirect:{HAKCSymbol.get_table_name()})
        WHERE sym.{str(HAKCSymbol.get_primary_key())} = $symbol_hash
        RETURN DISTINCT indirect.{str(HAKCSymbol.get_primary_key())} AS {HAKCFunction.relation_indirect_calls}
        """
        result[f'{HAKCFunction.relation_indirect_calls}'] = list(
            self.execute(cmd, symbol_hash=symbol_hash)[f'{HAKCFunction.relation_indirect_calls}'])

        cmd = f"""
        MATCH (sym: {HAKCSymbol.get_table_name()})-[:{HAKCFunction.relation_direct_calls}]->(direct:{HAKCSymbol.get_table_name()})
        WHERE sym.{str(HAKCSymbol.get_primary_key())} = $symbol_hash
        RETURN DISTINCT direct.{str(HAKCSymbol.get_primary_key())} AS {HAKCFunction.relation_direct_calls}
        """
        result[f'{HAKCFunction.relation_direct_calls}'] = list(
            self.execute(cmd, symbol_hash=symbol_hash)[f'{HAKCFunction.relation_direct_calls}'])

        cmd = f"""
        MATCH (sym: {HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_symbol}]->(uses:{HAKCSymbol.get_table_name()})
        WHERE sym.{str(HAKCSymbol.get_primary_key())} = $symbol_hash
        RETURN DISTINCT uses.{str(HAKCSymbol.get_primary_key())} AS {HAKCSymbol.relation_symbol}
        """
        result[f'{HAKCSymbol.relation_symbol}'] = list(
            self.execute(cmd, symbol_hash=symbol_hash)[f'{HAKCSymbol.relation_symbol}'])
        return result

    def insert_from_dataframe(self, table_name: str, df: pd.DataFrame):
        self.conn.execute(f'COPY {table_name} FROM df')
        self.clear_query_cache()

    def _create_perm_edge_from_response(self, perm_prefix: str = "rwx.", **kwargs):
        return {key.removeprefix(perm_prefix): val for key, val in kwargs.items() if key.startswith(perm_prefix)}

    def get_indirect_calls(self, symbol_hash: int) -> list[HAKCType]:
        cmd = f"""
            MATCH (head: {HAKCSymbol.get_table_name()})-[:{HAKCFunction.relation_indirect_calls}]->({HAKCType.get_table_name()}: {HAKCType.get_table_name()})
            WHERE head.symbol_hash = $symbol_hash
            RETURN DISTINCT {HAKCType.get_table_name()}.DebugType, {HAKCType.get_table_name()}.LLVMType
            ORDER BY {HAKCType.get_table_name()}.DebugType, {HAKCType.get_table_name()}.LLVMType;
        """
        return [self.create_object_from_df(HAKCType, entry) for _, entry in
                self.execute(cmd, symbol_hash=symbol_hash).iterrows()]

    def get_direct_calls(self, symbol_hash: int) -> list[HAKCSymbol]:
        cmd = f"""
                MATCH (head: {HAKCSymbol.get_table_name()})-[:{HAKCFunction.relation_direct_calls}]->({HAKCSymbol.get_table_name()}: {HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_type}]->({HAKCType.get_table_name()}:{HAKCType.get_table_name()}),
                ({HAKCSymbol.get_table_name()})-[{HAKCSymbol.relation_scope}]->({HAKCScope.get_table_name()}:{HAKCScope.get_table_name()})
                WHERE head.symbol_hash = $symbol_hash AND head.symbol_hash <> {HAKCSymbol.get_table_name()}.symbol_hash AND head.IsFunction = TRUE AND {HAKCSymbol.get_table_name()}.IsFunction = TRUE
                RETURN DISTINCT {HAKCSymbol.get_attrs()}, {HAKCType.get_attrs()}, {HAKCScope.get_attrs()};
            """
        return [self.create_object_from_df(HAKCFunction, entry) for _, entry in
                self.execute(cmd, symbol_hash=symbol_hash).iterrows()]

    def get_used_symbols(self, symbol_hash: int) -> list[HAKCSymbol]:
        cmd = f"""
            MATCH (head:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_symbol}]->({HAKCSymbol.get_table_name()}:{HAKCSymbol.get_table_name()}),
            ({HAKCScope.get_table_name()}:{HAKCScope.get_table_name()})<-[:{HAKCSymbol.relation_scope}]-({HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_type}]->({HAKCType.get_table_name()}:{HAKCType.get_table_name()})
            WHERE head.symbol_hash=$symbol_hash
            RETURN DISTINCT {HAKCSymbol.get_attrs()}, {HAKCType.get_attrs()}, {HAKCScope.get_attrs()};
        """
        return [self.create_object_from_df(HAKCSymbol, entry) for _, entry in
                self.execute(cmd, symbol_hash=symbol_hash).iterrows()]

    def get_symbols(self):
        return self._get_symbols()

    def create_node_table(self, node_type: Type[HAKCDBNode]):
        self.execute(f'CREATE NODE TABLE IF NOT EXISTS {node_type.get_table_definition()}', enable_cache=False)

    def create_relationship_table(self, edge_type: HAKCDBRelation):
        self.execute(f'CREATE REL TABLE IF NOT EXISTS {edge_type.get_definition()}', enable_cache=False)

    def get_dag_edges(self, symbol_hash: int) -> list[tuple['HAKCSymbol', int]]:
        # want to reconstruct the output from the yaml exactly, so the compartmentalization can be rebuilt from the database
        # Note: Only need to get enough data to match the node in the networkx graph
        cmd = f"""
        MATCH ({HAKCType.get_table_name()}:{HAKCType.get_table_name()})<-[{HAKCSymbol.relation_type}:{HAKCSymbol.relation_type}]-({HAKCSymbol.get_table_name()}:{HAKCSymbol.get_table_name()})-[{HAKCSymbol.relation_scope}:{HAKCSymbol.relation_scope}]->({HAKCScope.get_table_name()}:{HAKCScope.get_table_name()}), 
        (sym:{HAKCSymbol.get_table_name()})<-[{HAKCSymbol.relation_dag}:{HAKCSymbol.relation_dag}]-({HAKCSymbol.get_table_name()})
        WHERE sym.{HAKCSymbol.get_primary_key()} = $symbol_hash  
        RETURN DISTINCT {HAKCType.get_attrs()}, {HAKCScope.get_attrs()}, {HAKCSymbol.get_attrs()}, {HAKCSymbol.relation_dag}.weight as weight;
        """
        return [(self.create_object_from_df(HAKCSymbol, entry), entry['weight']) for _, entry in
                self.execute(cmd, symbol_hash=symbol_hash).iterrows()]

    def get_division_compartment(self, symbol_hash: int) -> Optional[tuple[HAKCDivision, HAKCCompartment]]:
        cmd = f"""
        MATCH ({HAKCScope.get_table_name()})<-[:{HAKCSymbol.relation_scope}]-({HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_division}]->({HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->({HAKCCompartment.get_table_name()})
        WHERE {HAKCSymbol.get_table_name()}.{HAKCSymbol.get_primary_key()} = $symbol_hash  
        RETURN DISTINCT {HAKCScope.get_attrs()}, {HAKCDivision.get_attrs()}, {HAKCCompartment.get_attrs()}, {HAKCSymbol.get_attrs()};
        """
        data = self.execute(cmd, symbol_hash=symbol_hash)
        if data.empty:
            return None
        return (self.create_object_from_df(HAKCDivision, data.iloc[0]),
                self.create_object_from_df(HAKCCompartment, data.iloc[0]))

    def _get_symbols(self, symbol_name: str = None, symbol_hash: int = None, where: str = None, deep=False) -> list[
        HAKCSymbol]:
        # want to reconstruct the output from the yaml exactly, so the compartmentalization can be rebuilt from the database
        cmd = f"""
        MATCH ({HAKCType.get_table_name()}:{HAKCType.get_table_name()})<-[{HAKCSymbol.relation_type}:{HAKCSymbol.relation_type}]-({HAKCSymbol.get_table_name()}:{HAKCSymbol.get_table_name()})-[{HAKCSymbol.relation_scope}:{HAKCSymbol.relation_scope}]->({HAKCScope.get_table_name()}:{HAKCScope.get_table_name()})
        WHERE TRUE {f' AND {HAKCSymbol.get_table_name()}.Name=$symbol_name' if symbol_name else ''} {f' AND {HAKCSymbol.get_table_name()}.{HAKCSymbol.get_primary_key()}=$symbol_hash' if symbol_hash else ''} {f'AND {where}' if where else ''}
        OPTIONAL MATCH ({HAKCSymbol.get_table_name()})-[{HAKCSymbol.relation_definition_location}:{HAKCSymbol.relation_definition_location}]->({HAKCDefinitionLocation.get_table_name()}:{HAKCDefinitionLocation.get_table_name()})
        RETURN DISTINCT {HAKCType.get_attrs()}, {HAKCScope.get_attrs()}, {HAKCSymbol.get_attrs()}, {HAKCDefinitionLocation.get_attrs()}, {HAKCSymbol.relation_definition_location}.DefiningLine;
        """
        cmdargs = dict()
        if symbol_name:
            cmdargs['symbol_name'] = symbol_name
        if symbol_hash:
            cmdargs['symbol_hash'] = symbol_hash
        # the 'base' HAKCSymbol is now created, now look for all symbols used, direct calls, indirect calls, types used
        # Note: Speeding up performance by only doing a 'shallow' query of the direct calls, since we only need to know enough to create the symbol hash
        return [self.create_object_from_df(HAKCSymbol, entry, deep=deep) for _, entry in
                self.execute(cmd, **cmdargs).iterrows()]

    def get_types(self, type_hashes: Optional[list[int]] = None) -> list[HAKCType]:
        assert isinstance(type_hashes, list) or type_hashes is None, f"types_hash is not of type list[int]"
        cmd = f"""
        MATCH ({HAKCType.get_table_name()}:{HAKCType.get_table_name()})
        {f'WHERE {HAKCType.get_table_name()}.{HAKCType.get_primary_key()} IN ($type_hashes)' if type_hashes else ''}
        RETURN DISTINCT {HAKCType.get_attrs()};
        """
        cmdargs = dict()
        if type_hashes:
            cmdargs['type_hashes'] = type_hashes
        return [self.create_object_from_df(HAKCType, entry) for _, entry in self.execute(cmd, **cmdargs).iterrows()]

    def get_scopes(self) -> list[HAKCScope]:
        cmd = f"""
        MATCH ({HAKCScope.get_table_name()}:{HAKCScope.get_table_name()})
        RETURN DISTINCT {HAKCScope.get_attrs()};
        """
        return [self.create_object_from_df(HAKCScope, entry) for _, entry in self.execute(cmd).iterrows()]

    def get_definition_locations(self) -> list[HAKCDefinitionLocation]:
        cmd = f"""
        MATCH ({HAKCDefinitionLocation.get_table_name()}:{HAKCDefinitionLocation.get_table_name()})<-[{HAKCSymbol.relation_definition_location}:{HAKCSymbol.relation_definition_location}]-({HAKCSymbol.get_table_name()}:{HAKCSymbol.get_table_name()})
        RETURN DISTINCT {HAKCDefinitionLocation.get_attrs()};
        """
        return [self.create_object_from_df(HAKCDefinitionLocation, entry) for _, entry in self.execute(cmd).iterrows()]

    def get_compartments(self) -> list[HAKCCompartment]:
        cmd = f"""
        MATCH ({HAKCCompartment.get_table_name()}:{HAKCCompartment.get_table_name()})
        RETURN DISTINCT {HAKCCompartment.get_attrs()};
        """
        return [self.create_object_from_df(HAKCCompartment, entry) for _, entry in self.execute(cmd).iterrows()]

    def get_symbols_using_type(self, type_hash: int) -> list[HAKCSymbol]:
        cmd = f"""
        MATCH ({HAKCSymbol.get_table_name()}:{HAKCSymbol.get_table_name()})-[:{HAKCFunction.relation_types_used}]->({HAKCType.get_table_name()}:{HAKCType.get_table_name()})
        WHERE {HAKCType.get_table_name()}.{HAKCType.get_primary_key()} = $type_hash
        RETURN DISTINCT {HAKCSymbol.get_table_name()}.{HAKCSymbol.get_primary_key()};
        """
        return [self.create_object_from_df(HAKCSymbol, entry) for _, entry in
                self.execute(cmd, type_hash=type_hash, enable_cache=False).iterrows()]

    def get_symbol_hashes_in_definition_location(self, DefiningFile: str) -> set[int]:
        cmd = f"""
            MATCH ({HAKCSymbol.get_table_name()}:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_definition_location}]->({HAKCDefinitionLocation.get_table_name()}:{HAKCDefinitionLocation.get_table_name()})
            WHERE {HAKCDefinitionLocation.get_table_name()}.DefiningFile =~ $DefiningFile
            RETURN DISTINCT {HAKCSymbol.get_table_name()}.{HAKCSymbol.get_primary_key()};
        """
        # Note: need to use '.*' to allow for any prepended path
        return set(self.execute(cmd, DefiningFile=f'.*{DefiningFile}')[
                       f'{HAKCSymbol.get_table_name()}.{HAKCSymbol.get_primary_key()}'])

    def get_symbol_hash(self, Name: str, DefiningFile: str, DefiningLine: int) -> int:
        cmd = f"""
            MATCH (sym:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_definition_location}]->(dl:{HAKCDefinitionLocation.get_table_name()})
            WHERE sym.Name=$Name AND dl.DefiningFile=$DefiningFile AND dl.DefiningLine=$DefiningLine
            RETURN DISTINCT sym.{HAKCSymbol.get_primary_key()} as symbol_hash;
        """
        return self.execute(cmd, Name=Name, DefiningFile=DefiningFile, DefiningLine=DefiningLine)['symbol_hash'][0]

    def add_all_symbols_to_nec(self, nec_division: HAKCDivision, nec_compartment: HAKCCompartment):
        logger.debug(f"!!!Adding all symbols to NEC!!!")
        logger.debug(self)
        self.delete_all_compartments()

        cmd = f"CREATE (div:{HAKCDivision.get_table_name()} {{division_hash: $division_hash, {HAKCDivision.DivisionIDColumnName()}: $division_id, Salt: $salt}});"
        self.execute(cmd, division_hash=hash(nec_division), division_id=nec_division.division_id,
                     salt=nec_division.salt, enable_cache=False)
        cmd = f"CREATE (comp:{HAKCCompartment.get_table_name()} {{CompartmentID: $compartment_id}});"
        self.execute(cmd, compartment_id=nec_compartment.compartment_id, enable_cache=False)
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()}), (div:{HAKCDivision.get_table_name()}), (comp:{HAKCCompartment.get_table_name()})
        WHERE div.{HAKCDivision.get_primary_key()} = $division_hash AND comp.{HAKCCompartment.get_primary_key()} = $compartment_id
        CREATE (sym)-[:{HAKCSymbol.relation_division}]->(div)-[:{HAKCDivision.relation_compartment}]->(comp);
        """
        self.execute(cmd, division_hash=hash(nec_division), compartment_id=nec_compartment.compartment_id,
                     enable_cache=False)
        logger.debug(f"!!!Added all symbols to NEC!!!")
        logger.debug(self)
        self.clear_query_cache()

    def set_division_compartment_id_by_symbol(self, symbol_hashes: list[int], new_division_id: int,
                                              new_compartment_id: int):
        # delete old relationship between symbol, division, and compartment
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()})-[old_div_edge:{HAKCSymbol.relation_division}]->(old_div:{HAKCDivision.get_table_name()})-[old_comp_edge:{HAKCDivision.relation_compartment}]->(old_comp:{HAKCSymbol.get_table_name()})
        WHERE sym.{HAKCSymbol.get_primary_key()} IN ($symbol_hashes)
        DELETE old_div_edge, old_comp_edge;
        """
        self.execute_prepared_stmt(cmd, symbol_hashes=symbol_hashes)
        # create new division, compartment if they don't exist
        cmd = f"""
        MERGE (comp:{HAKCCompartment.get_table_name()} {{CompartmentID: $compartment_id}}),
        (div:{HAKCDivision.get_table_name()} {{division_hash: $division_hash, {HAKCDivision.DivisionIDColumnName()}: $division_id, Salt: $salt}})-[:{HAKCDivision.relation_compartment}]->(comp);
        """
        new_div = HAKCDivision(int(new_division_id))
        self.execute_prepared_stmt(cmd, division_hash=hash(new_div), division_id=new_division_id, salt=new_div.salt,
                                   compartment_id=new_compartment_id, )
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()}), (div:{HAKCDivision.get_table_name()}), (comp:{HAKCCompartment.get_table_name()})
        WHERE sym.{HAKCSymbol.get_primary_key()} IN ($symbol_hashes) AND div.{HAKCDivision.get_primary_key()} = $division_hash AND comp.CompartmentID = $compartment_id
        CREATE (sym)-[:{HAKCSymbol.relation_division}]->(div)-[:{HAKCDivision.relation_compartment}]->(comp);
        """
        self.execute(cmd, symbol_hashes=symbol_hashes, division_hash=hash(new_div), compartment_id=new_compartment_id,
                     enable_cache=False)

    def get_all_symbol_hashes_in_compartment(self, compartment_id: int) -> list[int]:
        cmd = f"""
        MATCH (comp1:{HAKCCompartment.get_table_name()})<-[:{HAKCDivision.relation_compartment}]-(div1:{HAKCDivision.get_table_name()})<-[:{HAKCSymbol.relation_division}]-(sym1:{HAKCSymbol.get_table_name()})
        WHERE comp1.CompartmentID = $source_compartment_id
        return sym1.{str(HAKCSymbol.get_primary_key())} AS symbol_hash;
        """
        response = self.execute(cmd, source_compartment_id=int(compartment_id))
        ret = response['symbol_hash'].to_list()
        return ret

    def get_compartment_symbol_count(self) -> dict[int, int]:
        cmd = f"""
        MATCH
        (comp1:{HAKCCompartment.get_table_name()})
        RETURN comp1.{str(HAKCCompartment.get_primary_key())} AS CompartmentID,  COUNT {{ MATCH (comp1)<-[:{HAKCDivision.relation_compartment}]-(:{HAKCDivision.get_table_name()})<-[:{HAKCSymbol.relation_division}]-(:{HAKCSymbol.get_table_name()}) }} AS Count
        """
        response = self.execute_prepared_stmt(cmd)
        result = dict()
        for _, row in response.get_as_df().iterrows():
            compartment_id = int(row['CompartmentID'].item())
            count = int(row['Count'].item())
            result[compartment_id] = count

        return result

    def get_all_divisions_in_compartment(self, compartment_ids: set[int]) -> dict[int, list[HAKCDivision]]:
        division_attrs = HAKCDivision.get_attrs()
        compartment_attrs = HAKCCompartment.get_attrs()
        cmd = f"""
        MATCH ({HAKCCompartment.get_table_name()}:{HAKCCompartment.get_table_name()})<-[:{HAKCDivision.relation_compartment}]-({HAKCDivision.get_table_name()}:{HAKCDivision.get_table_name()})
        WHERE {HAKCCompartment.get_table_name()}.{str(HAKCCompartment.get_primary_key())} IN [{','.join([str(i) for i in compartment_ids])}]
        RETURN DISTINCT {division_attrs}, {compartment_attrs}
        """
        divisions = dict()
        response = self.execute(cmd)
        for _, data in response.iterrows():
            div, comp = (HAKCDatabase.create_object_from_df(HAKCDivision, data),
                         HAKCDatabase.create_object_from_df(HAKCCompartment, data))
            if comp.compartment_id not in divisions:
                divisions[comp.compartment_id] = []
            divisions[comp.compartment_id].append(div)

        return divisions

    def merge_compartments(self, compartments_to_merge: dict[int, int]):
        target_compartments = set(compartments_to_merge.values())
        compartments_to_remove = set(compartments_to_merge.keys())
        existing_divisions = self.get_all_divisions_in_compartment(target_compartments)

        cmd = f"""
            MATCH (sym:{HAKCSymbol.get_table_name()})-[e:{HAKCSymbol.relation_division}]->(div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(c:{HAKCCompartment.get_table_name()})
            WHERE c.{str(HAKCCompartment.get_primary_key())} IN [{','.join([str(i) for i in compartments_to_remove])}]
            DELETE e
            RETURN sym.{str(HAKCSymbol.get_primary_key())} AS SymbolHash, div.{HAKCDivision.DivisionIDColumnName()} AS DivisionID, c.{str(HAKCCompartment.get_primary_key())} AS CompartmentID;
        """
        result = self.execute_prepared_stmt(cmd)
        data = result.get_as_df()
        symbol_hashes = list()
        division_hashes = list()

        for _, row in data.iterrows():
            division_id = row['DivisionID'].item()
            symbol_hash = row['SymbolHash'].item()
            old_compartment_id = row['CompartmentID'].item()

            division_list = existing_divisions[compartments_to_merge[old_compartment_id]]
            symbol_hashes.append(symbol_hash)

            division_hash = hash(division_list[0])

            for existing_division in division_list:
                if existing_division.division_id == division_id:
                    division_hash = hash(existing_division)
            division_hashes.append(division_hash)

        df = pd.DataFrame({
            "from": symbol_hashes,
            "to": division_hashes,
        })
        self.insert_from_dataframe(HAKCSymbol.relation_division, df)

        cmd = f"""
            MATCH (div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(c:{HAKCCompartment.get_table_name()})
            WHERE c.CompartmentID IN [{','.join([str(i) for i in compartments_to_remove])}]
            DETACH DELETE div;
        """
        self.execute_prepared_stmt(cmd)
        cmd = f"""
            MATCH (c:{HAKCCompartment.get_table_name()})
            WHERE c.CompartmentID IN [{','.join([str(i) for i in compartments_to_remove])}]
            DETACH DELETE c;
        """
        self.execute_prepared_stmt(cmd)
