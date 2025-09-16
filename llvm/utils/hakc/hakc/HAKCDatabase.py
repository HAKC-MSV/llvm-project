import logging
import multiprocessing as mp
import threading
from typing import Type, Optional, Tuple, cast

import pandas as pd
import kuzu

from .HAKCBase import HAKCDBNode, HAKCDBRelation
from .HAKCLogger import HAKCLogger
from .HAKCObjects import HAKCSymbol, HAKCFunction, HAKCScope, HAKCType, HAKCGlobalVariable, HAKCDivision, \
    HAKCCompartment, HAKCDefinitionLocation

logging.setLoggerClass(HAKCLogger)

logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc-database'))


# creating thread lock for shared resource (caching system)


class HAKCDatabase:
    def __init__(self, db_dir: str, read_only: bool = False, max_num_threads=int(mp.cpu_count() / 2), profile = False):
        self.db_dir = db_dir
        self.database = None
        self.conn = None
        self.open(read_only=read_only, max_num_threads=max_num_threads)
        self.profile = profile
        # mutexs to share data among each server thread
        if profile:
            self.cache_hit_mutex = threading.Lock()
            self.cache_miss_mutex = threading.Lock()
            self.total_query_time_mutex = threading.Lock()
            self.cache_hit = 0
            self.cache_miss = 0
            self.total_query_time = 0

        self.query_cache_mutex = threading.Lock()
        self.query_cache = {}

    def get_stats(self):
        cmd = f"""
        MATCH (HAKCSymbol:HAKCSymbol)
        RETURN COUNT(HAKCSymbol) as symbols;
        """
        symbols = self.execute(cmd)
        cmd = f"""
        MATCH (HAKCType:HAKCType)
        RETURN COUNT(HAKCType) as types;
        """
        types = self.execute(cmd)
        cmd = f"""
        MATCH (HAKCScope:HAKCScope)
        RETURN COUNT(HAKCScope) as scopes;
        """
        scopes = self.execute(cmd)
        cmd = f"""
        MATCH (HAKCDefinitionLocation:HAKCDefinitionLocation)
        RETURN COUNT(HAKCDefinitionLocation) as definition_locations;
        """
        definition_locations = self.execute(cmd)
        cmd = f"""
        MATCH (HAKCDivision:HAKCDivision)
        RETURN COUNT(HAKCDivision) as divisions;
        """
        divisions = self.execute(cmd)
        cmd = f"""
        MATCH (HAKCCompartment:HAKCCompartment)
        RETURN COUNT(HAKCCompartment) as compartments;
        """
        compartments = self.execute(cmd)

        return {'symbols': symbols['symbols'][0],
                'types': types['types'][0],
                'scopes': scopes['scopes'][0],
                'definition_locations': definition_locations['definition_locations'][0],
                'divisions': divisions['divisions'][0],
                'compartments': compartments['compartments'][0]}

    def print_stats(self):
        stats = self.get_stats()
        logger.info(f"hakc-db with {stats['symbols']} Symbols, {stats['types']} Types, {stats['scopes']} Scopes, {stats['definition_locations']} DefinitionLocations, {stats['divisions']} Divisions, {stats['compartments']} Compartments")

    def increment_cache_hit(self):
        with self.cache_hit_mutex:
            self.cache_hit += 1

    def increment_cache_miss(self):
        with self.cache_miss_mutex:
            self.cache_miss += 1

    def increment_total_query_time(self, delta):
        with self.total_query_time_mutex:
            self.total_query_time += delta

    def get_cache_hits(self):
        with self.cache_hit_mutex:
            return self.cache_hit

    def get_cache_misses(self):
        with self.cache_miss_mutex:
            return self.cache_miss

    def get_query_cache(self, key):
        if key in self.query_cache:
            if self.profile:
                self.increment_cache_hit()
            return self.query_cache[key]
        return None

    def set_query_cache(self, key, value):
        with self.query_cache_mutex:
            if key not in self.query_cache:
                if self.profile:
                    self.increment_cache_miss()
                self.query_cache[key] = value

    def get_hit_rate(self) -> Tuple[int, int, float]:
        cache_hits = self.get_cache_hits()
        cache_misses = self.get_cache_misses()
        total = cache_hits + cache_misses
        ratio = float(float(cache_hits) / float(total)) if total > 0 else 0
        return cache_hits, cache_misses, round(100 * ratio)

    def get_cache_stats(self):
        total_queries = self.get_cache_misses()
        total_hits = self.get_cache_hits()
        # time saved = average query time * number of cache hits
        cache_hits, cache_misses, hit_rate = self.get_hit_rate()
        with self.total_query_time_mutex:
            average_miss_penalty = (float(self.total_query_time) / float(total_queries)) if total_queries > 0 else 0
            time_saved = round(total_hits * average_miss_penalty)
            return f"\t{total_queries} database queries took {round(self.total_query_time, 2)} seconds with an average of {round(average_miss_penalty, 2)} seconds, hit rate={round(hit_rate, 2)}%; {total_hits} cache hits saved {time_saved} seconds of cpu time."

    def __del__(self):
        self.close()

    def close(self):
        if self.conn is not None:
            self.conn.close()
        if self.database is not None:
            self.database.close()

    def open(self, read_only: bool = False, max_num_threads=int(mp.cpu_count() / 2)):
        self.database = kuzu.Database(self.db_dir, read_only=read_only, max_num_threads=max_num_threads)
        self.conn = kuzu.Connection(self.database)  # main connection

    def new_conn(self, read_only: bool = False):
        self.conn = kuzu.Connection(self.database)  # thread i connection

    def execute_prepared_stmt(self, prepared_stmt: str, **kwargs):
        start = None
        if self.profile:
            start = time.time()
        response =  self.conn.execute(prepared_stmt, parameters=kwargs)
        if self.profile:
            end = time.time()
            self.increment_total_query_time(end - start)
            logger.info(self.get_cache_stats())
        return response

    @staticmethod
    def create_query_key(stmt: str, **kwargs):
        return stmt.replace(' ', '') + str(tuple(sorted(kwargs.items())))

    def execute(self, prepared_stmt: str, enable_cache=True, **kwargs):
        # adding query level caching (essentially a wrapper for conn.execute)
        if enable_cache:
            key = HAKCDatabase.create_query_key(prepared_stmt, **kwargs)
            found = self.get_query_cache(key)
            if found is not None:
                # logger.info(f"Cache hit  with hit rate {hit_rate}% ({cache_hits}/{cache_hits+cache_misses})\t[Database level cache]")
                return found
            value = self.execute_prepared_stmt(prepared_stmt, **kwargs).get_as_df()
            self.set_query_cache(key, value)
            # logger.info(f"Cache miss with hit rate {hit_rate}% ({cache_hits}/{cache_hits+cache_misses})\t[Database level cache]")
            return value
        return self.execute_prepared_stmt(prepared_stmt, **kwargs).get_as_df()

    def get_compartment_entry_token_from_id(self, compartment_id: int) -> Optional[int]:
        cmd = f"""
        MATCH
        (comp:{HAKCCompartment.get_table_name()})<-[:{str(HAKCDivision.relation_compartment)}]-({HAKCDivision.get_table_name()}:{HAKCDivision.get_table_name()})<-[:{str(HAKCSymbol.relation_division)}]-(:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_dag}]->(:{HAKCSymbol.get_table_name()})
        WHERE comp.{str(HAKCCompartment.get_primary_key())} = $compartment_id
        RETURN DISTINCT {HAKCDivision.get_table_name()}.DivisionID;
        """
        target_divisions = set()
        for _, entry in self.execute(cmd, compartment_id=compartment_id).iterrows():
            target_divisions.add(int(entry[f'{HAKCDivision.get_table_name()}.DivisionID']))
        return HAKCCompartment.compute_entry_token(compartment_id, target_divisions)

    def get_division_access_token_from_id(self, division_id: int, compartment_id: int) -> Optional[int]:
        cmd = f"""
        MATCH
        (comp:{HAKCCompartment.get_table_name()})<-[:{str(HAKCDivision.relation_compartment)}]-(div1:{HAKCDivision.get_table_name()})<-[:{str(HAKCSymbol.relation_division)}]-(:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_dag}]->(:{HAKCSymbol.get_table_name()})-[:{str(HAKCSymbol.relation_division)}]->(div2:{HAKCDivision.get_table_name()})-[:{str(HAKCDivision.relation_compartment)}]->(comp)
        WHERE comp.{str(HAKCCompartment.get_primary_key())} = $compartment_id AND div1.DivisionID = $division_id
        RETURN DISTINCT div2.DivisionID AS DivisionID;
        """
        division_ids = {division_id}
        for _, entry in self.execute(cmd, division_id=division_id, compartment_id=compartment_id).iterrows():
            division_ids.add(entry['DivisionID'])
        return HAKCDivision.compute_access_token(compartment_id, division_ids)

    def get_division(self, division_id: int, compartment_id: int) -> Optional[HAKCDivision]:
        access_token = self.get_division_access_token_from_id(division_id, compartment_id)
        cmd = f"""
        MATCH ({HAKCDivision.get_table_name()}:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(c:{HAKCCompartment.get_table_name()})
        WHERE {HAKCDivision.get_table_name()}.DivisionID = $division_id AND c.CompartmentID = $compartment_id
        RETURN DISTINCT {HAKCDivision.get_table_name()}.DivisionID, {HAKCDivision.get_table_name()}.Salt
        """
        data = self.execute(cmd, division_id=division_id, compartment_id=compartment_id)
        if len(data) != 1:
            raise RuntimeError(f"Get division with parameters division_id: {division_id}, compartment_id: {compartment_id} did not return a response of length one: {data}")
        data['HAKCDivision.AccessToken'] = access_token
        return HAKCDatabase.__create_object_from_df(HAKCDivision, data.iloc[0])

    def get_division_id_compartment_id_from_symbol(self, symbol: HAKCSymbol) -> Optional[
        Tuple[HAKCDivision, HAKCCompartment]]:
        cmd = f"""        
        MATCH (scope:{HAKCScope.get_table_name()})<-[:{HAKCSymbol.relation_scope}]-(sym:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_division}]->({HAKCDivision.get_table_name()}:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->({HAKCCompartment.get_table_name()}:{HAKCCompartment.get_table_name()})
        WHERE sym.Name = $Name AND scope.Scope = $Scope
        RETURN DISTINCT {HAKCDivision.get_table_name()}.DivisionID, {HAKCDivision.get_table_name()}.Salt, {HAKCCompartment.get_table_name()}.CompartmentID;
        """
        data = self.execute(cmd, Name=symbol.name, Scope=symbol.scope.scope)
        if len(data) != 1:
            raise RuntimeError(f"Get division id compartment id with parameters {symbol} did not return a response of length one: {data}")
        return HAKCDatabase.__create_object_from_df(HAKCDivision, data.iloc[0]), HAKCDatabase.__create_object_from_df(HAKCCompartment, data.iloc[0])


    def get_valid_targets_from_compartment_id(self, source_compartment_id: int) -> list[int]:
        # NOTE: comp1 is fixed to the caller's compartment
        # from the perspective of the caller, the compartment_id is their own and they are looking for compartment_ids of targets
        # Get valid target compartments given compartment id
        # comp1 <- div1 <- symbol1 -(Dag2)-> symbol2 -> div2 -> comp2
        # WITH  *
        cmd = f"""
        MATCH (comp1:{HAKCCompartment.get_table_name()})<-[:{HAKCDivision.relation_compartment}]-(div1:{HAKCDivision.get_table_name()})<-[:{HAKCSymbol.relation_division}]-(sym1:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_dag}]->(sym2:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_division}]->(div2:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(comp2:{HAKCCompartment.get_table_name()})
        WHERE comp1.CompartmentID = $source_compartment_id
        RETURN DISTINCT comp2.CompartmentID AS CompartmentID;
        """
        targets = set()
        for _, entry in self.execute(cmd, source_compartment_id=source_compartment_id).iterrows():
            targets.add(int(entry['CompartmentID']))
        return list(targets)

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

    def get_all_symbol_hashes(self) -> list[int]:
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()})
        RETURN DISTINCT sym.{str(HAKCSymbol.get_primary_key())} AS symbol_hash;
        """
        return list(self.execute(cmd)['symbol_hash'])

    def get_symbol_by_hash(self, symbol_hashes: list[int], **kwargs) -> set[HAKCSymbol]:
        assert (isinstance(symbol_hashes, list))
        return self._get_symbols(where=f'HAKCSymbol.symbol_hash in [{", ".join([str(sh) for sh in symbol_hashes])}]',
                                 **kwargs)

    def get_single_symbol_by_hash(self, symbol_hash: int) -> HAKCSymbol:
        assert (isinstance(symbol_hash, int))
        return list(self._get_symbols(symbol_hash=symbol_hash))[0]

    def get_symbols_by_name(self, symbol_name: str) -> list[HAKCSymbol]:
        return list(self._get_symbols(symbol_name=symbol_name))

    def get_symbol_hashes_to_symbols(self):
        symbol_hashes = self.get_all_symbol_hashes()
        symbols = self.get_symbol_by_hash(symbol_hashes)
        assert (len(symbol_hashes) == len(symbols))
        return dict(zip(symbol_hashes, symbols))

    def delete_all_compartments(self):
        cmd = f"""
        MATCH (div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(c:{HAKCCompartment.get_table_name()})
        DETACH DELETE div, c;
        """
        self.execute(cmd, enable_cache=False)

    def get_all_divisions(self):
        cmd = f"""
        MATCH ({HAKCDivision.get_table_name()}:{HAKCDivision.get_table_name()})
        RETURN DISTINCT {HAKCDivision.get_table_name()}.DivisionID, {HAKCDivision.get_table_name()}.Salt;
        """
        # use list instead of set because we allow for duplicates
        divisions = set()
        for _, entry in self.execute(cmd).iterrows():
            divisions.add(HAKCDatabase.__create_object_from_df(HAKCDivision, entry))
        return divisions

    def get_all_compartments(self):
        cmd = f"""
        MATCH ({HAKCCompartment.get_table_name()}:{HAKCCompartment.get_table_name()})
        RETURN DISTINCT {HAKCCompartment.get_table_name()}.CompartmentID as CompartmentID;
        """
        compartments = set()
        for _, entry in self.execute(cmd).iterrows():
            compartments.add(HAKCCompartment(CompartmentID=entry['CompartmentID']))
        return compartments

    def get_symbol_definition_location(self, symbol: HAKCSymbol) -> Optional[HAKCDefinitionLocation]:
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()})-[e:{HAKCSymbol.relation_definition_location}]->(dl:{HAKCDefinitionLocation.get_table_name()})
        WHERE sym.{str(HAKCSymbol.get_primary_key())} = $symbol_hash
        RETURN DISTINCT dl.DefiningFile as DefiningFile, e.DefiningLine as DefiningLine;
        """
        data = self.execute(cmd, symbol_hash=hash(symbol))
        return HAKCDefinitionLocation(DefiningFile=data['DefiningFile'][0], DefiningLine=data['DefiningLine'][0]) if len(data) == 1 else None

    def get_dag_computation_edges(self, symbol_hash: int) -> dict[str, list[int]]:
        result = dict()
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()})-[:{HAKCFunction.relation_indirect_calls}]->(:{HAKCType.get_table_name()})<-[:{HAKCSymbol.relation_type}]-(indirect:{HAKCSymbol.get_table_name()})
        WHERE sym.{str(HAKCSymbol.get_primary_key())} = $symbol_hash
        RETURN DISTINCT indirect.{str(HAKCSymbol.get_primary_key())} AS {HAKCFunction.relation_indirect_calls}
        """
        result[f'{HAKCFunction.relation_indirect_calls}'] = list(self.execute(cmd, symbol_hash=symbol_hash)[f'{HAKCFunction.relation_indirect_calls}'])

        cmd = f"""
        MATCH (sym: {HAKCSymbol.get_table_name()})-[:{HAKCFunction.relation_direct_calls}]->(direct:{HAKCSymbol.get_table_name()})
        WHERE sym.{str(HAKCSymbol.get_primary_key())} = $symbol_hash
        RETURN DISTINCT direct.{str(HAKCSymbol.get_primary_key())} AS {HAKCFunction.relation_direct_calls}
        """
        result[f'{HAKCFunction.relation_direct_calls}'] = list(self.execute(cmd, symbol_hash=symbol_hash)[f'{HAKCFunction.relation_direct_calls}'])

        cmd = f"""
        MATCH (sym: {HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_symbol}]->(uses:{HAKCSymbol.get_table_name()})
        WHERE sym.{str(HAKCSymbol.get_primary_key())} = $symbol_hash
        RETURN DISTINCT uses.{str(HAKCSymbol.get_primary_key())} AS {HAKCSymbol.relation_symbol}
        """
        result[f'{HAKCSymbol.relation_symbol}'] = list(self.execute(cmd, symbol_hash=symbol_hash)[f'{HAKCSymbol.relation_symbol}'])
        return result

    def insert_from_dataframe(self, table_name: str, df: pd.DataFrame):
        self.conn.execute(f'COPY {table_name} FROM df')

    def _create_perm_edge_from_response(self, perm_prefix: str = "rwx.", **kwargs):
        perm_data = {key.removeprefix(perm_prefix): val for key, val in kwargs.items() if key.startswith(perm_prefix)}
        if len(perm_data) == 0:
            raise RuntimeError('No type data provided')
        return perm_data

    def _create_symbol_from_response(self, data: pd.DataFrame, is_function: bool, symbol_prefix: Optional[str] = None) -> HAKCSymbol:
        if symbol_prefix:
            data.rename(columns={f'{symbol_prefix}.Name': 'HAKCSymbol.Name',
                                 f'{symbol_prefix}.Type': 'HAKCSymbol.Type',
                                 f'{symbol_prefix}.Scope': 'HAKCSymbol.Scope',
                                 f'{symbol_prefix}.DefinitionLocation': 'HAKCSymbol.DefinitionLocation'}, inplace=True)
        if is_function:
            return HAKCDatabase.__create_object_from_df(HAKCFunction, data)
        else:
            return HAKCDatabase.__create_object_from_df(HAKCGlobalVariable, data)

    def get_indirect_calls(self, symbol: HAKCSymbol) -> list[HAKCType]:
        cmd = f"""
            MATCH (head: {HAKCSymbol.get_table_name()})-[:{HAKCFunction.relation_indirect_calls}]->({HAKCType.get_table_name()}: {HAKCType.get_table_name()})
            WHERE head.symbol_hash = $symbol_hash
            RETURN DISTINCT {HAKCType.get_table_name()}.DebugType, {HAKCType.get_table_name()}.LLVMType
            ORDER BY {HAKCType.get_table_name()}.DebugType, {HAKCType.get_table_name()}.LLVMType;
        """
        try:
            types = []
            for _, entry in self.execute(cmd, symbol_hash=hash(symbol)).iterrows():
                types.append(HAKCDatabase.__create_object_from_df(HAKCType, entry))
        except Exception as e:
            logger.error(f'get_indirect_calls failed')
            raise e
        return types

    def get_direct_calls(self, symbol: HAKCSymbol) -> list[HAKCType]:
        cmd = f"""
                MATCH (head: {HAKCSymbol.get_table_name()})-[:{HAKCFunction.relation_direct_calls}]->(tail: {HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_type}]->(ty:{HAKCType.get_table_name()}),
                (tail)-[{HAKCSymbol.relation_scope}]->(scope:{HAKCScope.get_table_name()})
                WHERE head.symbol_hash = $symbol_hash AND head.symbol_hash <> tail.symbol_hash AND head.IsFunction = TRUE AND tail.IsFunction = TRUE
                RETURN DISTINCT tail.*, ty.*, scope.*;
            """
        try:
            direct_calls = set()
            for _, entry in self.execute(cmd, symbol_hash=hash(symbol)).iterrows():
                direct_calls.add(self._create_symbol_from_response(entry, is_function=True, symbol_prefix='tail.'))
        except Exception as e:
            logger.error(f'get_direct_calls failed')
            raise e
        return list(direct_calls)

    def get_used_symbols(self, symbol: HAKCSymbol):
        cmd = f"""
            MATCH (head:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_symbol}]->(tail:{HAKCSymbol.get_table_name()}),
            (sc:{HAKCScope.get_table_name()})<-[:{HAKCSymbol.relation_scope}]-(tail)-[:{HAKCSymbol.relation_type}]->(ty:{HAKCType.get_table_name()})
            WHERE head.symbol_hash=$symbol_hash
            RETURN DISTINCT tail.Name, tail.IsFunction AS is_function, sc.Scope,
            sc.LocalScopeName, ty.DebugType, ty.LLVMType;
        """
        try:
            used_symbols = set()
            for _, entry in self.execute(cmd, symbol_hash=hash(symbol)).iterrows():
                used_symbols.add(self._create_symbol_from_response(entry, is_function=entry['is_function'], symbol_prefix='tail.'))
        except Exception as e:
            logger.error(f'get_used_symbols failed')
            raise e
        return list(used_symbols)

    @staticmethod
    def __get_class_db_columns(cls):
        return [x.column_name for x in cls.get_data_columns()] + [cls.get_primary_key().column_name]

    @staticmethod
    def __create_object_from_df(cls, data):
        if cls == HAKCScope:
            return HAKCScope(Scope=data['HAKCScope.Scope'], LocalScopeName=data['HAKCScope.LocalScopeName'] if 'HAKCScope.LocalScopeName' in data else None)
        elif cls == HAKCType:
            return HAKCType(LLVMType=data['HAKCType.LLVMType'], DebugType=data['HAKCType.DebugType'] if 'HAKCType.DebugType' in data else None)
        elif cls == HAKCDefinitionLocation:
            return HAKCDefinitionLocation(DefiningFile=data['DefinitionLocation.DefinitionLocation'], DefiningLine = int(data['HAKCDefinitionLocation.DefiningLine']))
        elif cls == HAKCFunction or cls == HAKCGlobalVariable:
            data['HAKCSymbol.Scope'] = HAKCScope(Scope=data['HAKCScope.Scope'], LocalScopeName=data['HAKCScope.LocalScopeName'] if 'HAKCScope.LocalScopeName' in data else None)
            data['HAKCSymbol.Type'] = HAKCType(LLVMType=data['HAKCType.LLVMType'], DebugType=data['HAKCType.DebugType'] if 'HAKCType.DebugType' in data else None)
            if 'HAKCDefinitionLocation.DefiningLine' in data:
                data['HAKCSymbol.DefinitionLocation'] = HAKCDefinitionLocation(DefiningFile=data['DefinitionLocation.DefinitionLocation'], DefiningLine = int(data['HAKCDefinitionLocation.DefiningLine']))
            if cls == HAKCFunction:
                return HAKCFunction(Name=data['HAKCSymbol.Name'], Type=data['HAKCSymbol.Type'], Scope=data['HAKCSymbol.Scope'], DefinitionLocation=data['HAKCSymbol.DefinitionLocation'] if 'HAKCSymbol.DefinitionLocation' in data else None)
            if cls == HAKCGlobalVariable:
                return HAKCGlobalVariable(Name=data['HAKCSymbol.Name'], Type=data['HAKCSymbol.Type'], Scope=data['HAKCSymbol.Scope'], DefinitionLocation=data['HAKCSymbol.DefinitionLocation'] if 'HAKCSymbol.DefinitionLocation' in data else None)
        # Note: double check AccessToken, EntryToken
        elif cls == HAKCDivision:
            return HAKCDivision(DivisionID=int(data['HAKCDivision.DivisionID']), Salt=int(data['HAKCDivision.Salt']), AccessToken=int(data['HAKCDivision.AccessToken'] if 'HAKCDivision.AccessToken' in data else 0))
        elif cls == HAKCCompartment:
            return HAKCCompartment(CompartmentID=int(data['HAKCCompartment.CompartmentID']), EntryToken=int(data['HAKCCompartment.EntryToken'] if 'HAKCCompartment.EntryToken' in data else 0))
        raise RuntimeError(f"Trying to create invalid HAKC class from response: {cls} with data {data}")

    def get_symbols(self):
        return self._get_symbols()

    def create_node_table(self, node_type: Type[HAKCDBNode]):
        self.execute(f'CREATE NODE TABLE IF NOT EXISTS {node_type.get_table_definition()}', enable_cache=False)

    def create_relationship_table(self, edge_type: HAKCDBRelation):
        self.execute(f'CREATE REL TABLE IF NOT EXISTS {edge_type.get_definition()}', enable_cache=False)

    def get_dag_edges(self, _symbol: HAKCSymbol) -> set[tuple['HAKCSymbol', int]]:
        # want to reconstruct the output from the yaml exactly, so the compartmentalization can be rebuilt from the database
        # Note: Only need to get enough data to match the node in the networkx graph

        type_attrs = HAKCDatabase.get_object_attributes(HAKCType)
        scope_attrs = HAKCDatabase.get_object_attributes(HAKCScope)
        symbol_attrs = HAKCDatabase.get_object_attributes(HAKCSymbol)
        cmd = f"""
        MATCH ({HAKCType.get_table_name()}:{HAKCType.get_table_name()})<-[{HAKCSymbol.relation_type}:{HAKCSymbol.relation_type}]-({HAKCSymbol.get_table_name()}:{HAKCSymbol.get_table_name()})-[{HAKCSymbol.relation_scope}:{HAKCSymbol.relation_scope}]->({HAKCScope.get_table_name()}:{HAKCScope.get_table_name()}), 
        (sym:{HAKCSymbol.get_table_name()})<-[{HAKCSymbol.relation_dag}:{HAKCSymbol.relation_dag}]-({HAKCSymbol.get_table_name()})
        WHERE sym.{HAKCSymbol.get_primary_key()} = $symbol_hash  
        RETURN DISTINCT {type_attrs}, {scope_attrs}, {symbol_attrs}, {HAKCSymbol.relation_dag}.weight;
        """
        dag_edges = set()
        for _, entry in self.execute(cmd, symbol_hash=hash(_symbol)).iterrows():
            if entry["HAKCSymbol.IsFunction"]:
                dag_edges.add((HAKCDatabase.__create_object_from_df(HAKCFunction, entry), entry[f"{HAKCSymbol.relation_symbol}.weight"]))
            else:
                dag_edges.add((HAKCDatabase.__create_object_from_df(HAKCGlobalVariable, entry), entry[f"{HAKCSymbol.relation_symbol}.weight"]))
        return dag_edges

    def get_division_compartment(self, _symbol: HAKCSymbol) -> tuple[HAKCDivision, HAKCCompartment]:
        assert (isinstance(_symbol, HAKCSymbol))

        symbol_attrs = HAKCDatabase.get_object_attributes(HAKCSymbol)
        division_attrs = HAKCDatabase.get_object_attributes(HAKCDivision)
        compartment_attrs = HAKCDatabase.get_object_attributes(HAKCCompartment)
        scope_attrs = HAKCDatabase.get_object_attributes(HAKCScope)
        cmd = f"""
        MATCH ({HAKCScope.get_table_name()})<-[:{HAKCSymbol.relation_scope}]-({HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_division}]->({HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->({HAKCCompartment.get_table_name()})
        WHERE {HAKCSymbol.get_table_name()}.{HAKCSymbol.get_primary_key()} = $symbol_hash  
        RETURN DISTINCT {scope_attrs}, {symbol_attrs}, {division_attrs}, {compartment_attrs};
        """
        data = self.execute(cmd, symbol_hash=hash(_symbol))
        div, comp = None, None
        if len(data) == 1:
            div, comp = HAKCDatabase.__create_object_from_df(HAKCDivision, data.iloc[0]), HAKCDatabase.__create_object_from_df(HAKCCompartment, data.iloc[0])
        return div, comp

    @staticmethod
    def get_object_attributes(cls):
        assert (cls in {HAKCType, HAKCScope, HAKCSymbol, HAKCFunction, HAKCDefinitionLocation, HAKCDivision,
                        HAKCCompartment})
        return ", ".join([f"{cls.get_table_name()}.{x.column_name}" for x in cls.get_data_columns()] + [
            f"{cls.get_table_name()}.{cls.get_primary_key()}"])

    def _get_symbols(self, symbol_name: str = None, symbol_hash: int = None, where: str = None, deep = False ) -> set[HAKCSymbol]:
        # want to reconstruct the output from the yaml exactly, so the compartmentalization can be rebuilt from the database
        type_attrs = HAKCDatabase.get_object_attributes(HAKCType)
        scope_attrs = HAKCDatabase.get_object_attributes(HAKCScope)
        symbol_attrs = HAKCDatabase.get_object_attributes(HAKCSymbol)
        dl_attrs = HAKCDatabase.get_object_attributes(HAKCDefinitionLocation)
        cmd = f"""
        MATCH ({HAKCType.get_table_name()}:{HAKCType.get_table_name()})<-[{HAKCSymbol.relation_type}:{HAKCSymbol.relation_type}]-({HAKCSymbol.get_table_name()}:{HAKCSymbol.get_table_name()})-[{HAKCSymbol.relation_scope}:{HAKCSymbol.relation_scope}]->({HAKCScope.get_table_name()}:{HAKCScope.get_table_name()})
        WHERE TRUE {f' AND {HAKCSymbol.get_table_name()}.Name=$symbol_name' if symbol_name else ''} {f' AND {HAKCSymbol.get_table_name()}.{HAKCSymbol.get_primary_key()}=$symbol_hash' if symbol_hash else ''} {f'AND {where}' if where else ''}
        OPTIONAL MATCH ({HAKCSymbol.get_table_name()})-[{HAKCSymbol.relation_definition_location}:{HAKCSymbol.relation_definition_location}]->({HAKCDefinitionLocation.get_table_name()}:{HAKCDefinitionLocation.get_table_name()})
        RETURN DISTINCT {type_attrs}, {scope_attrs}, {symbol_attrs}, {dl_attrs}, {HAKCSymbol.relation_definition_location}.DefiningLine;
        """

        cmdargs = dict()
        if symbol_name:
            cmdargs['symbol_name'] = symbol_name
        if symbol_hash:
            cmdargs['symbol_hash'] = symbol_hash
        functions = set()
        gvs = set()
        for _, entry in self.execute(cmd, **cmdargs).iterrows():
            if entry["HAKCSymbol.IsFunction"] is True:
                functions.add(HAKCDatabase.__create_object_from_df(HAKCFunction, entry))
            else:
                gvs.add(HAKCDatabase.__create_object_from_df(HAKCGlobalVariable, entry))

        # the 'base' HAKCSymbol is now created, now look for all symbols used, direct calls, indirect calls, types used
        # Note: Speeding up performance by only doing a 'shallow' query of the direct calls, since we only need to know enough to create the symbol hash
        if deep:
            for func in functions:
                for used_symbol in self.get_used_symbols(func):
                    func.used_symbols.append(used_symbol)

                for direct_call in self.get_direct_calls(func):
                    func.direct_calls.append(direct_call)

                for indirect_call in self.get_indirect_calls(func):
                    func.indirect_calls.append(indirect_call)

        symbols = functions.union(gvs)
        return symbols

    def get_symbol_hash(self, Name, DefiningFile, DefiningLine):
        cmd = f"""
            MATCH (sym:{HAKCSymbol.get_table_name()})-[{HAKCSymbol.relation_definition_location}]->(dl:{HAKCDefinitionLocation.get_table_name()})
            WHERE sym.Name=$Name AND dl.DefiningFile=$DefiningFile AND dl.DefiningLine=$DefiningLine
            RETURN DISTINCT sym.{HAKCSymbol.get_primary_key()}
        """
        data = self.execute(cmd, Name=Name, DefiningFile=DefiningFile, DefiningLine=DefiningLine)
        if len(data) != 1:
            raise RuntimeError(f"Queried symbol hash from (Name, DefiningFile, DefiningLine) = ({Name}, {DefiningFile}, {DefiningLine}), but could not find symbol hash!")
        return data[f"sym.{HAKCSymbol.get_primary_key()}"][0]

    def set_division_compartment_id_by_symbol(self, _symbol: HAKCSymbol, new_division_id: int, new_compartment_id: int):

        # delete old relationship between symbol, division, and compartment
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()})-[old_div_edge:{HAKCSymbol.relation_division}]->(old_div:{HAKCDivision.get_table_name()})-[old_comp_edge:{HAKCDivision.relation_compartment}]->(old_comp:{HAKCCompartment.get_table_name()})
        WHERE sym.{HAKCSymbol.get_primary_key()} = $symbol_hash
        DELETE old_div_edge, old_comp_edge;
        """
        self.execute(cmd, symbol_hash=hash(_symbol), enable_cache=False)

        # create new compartment if it does not exist in the database
        # MERGE -> If MATCH <pattern> then RETURN <pattern> ELSE CREATE <pattern>
        cmd = f"""
        MERGE (comp:{HAKCCompartment.get_table_name()} {{CompartmentID: $compartment_id}});
        """
        self.execute(cmd, compartment_id=new_compartment_id, enable_cache=False)

        # check if new division is connected to the new compartment
        cmd = f"""
        MATCH (div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(comp:{HAKCCompartment.get_table_name()})
        WHERE comp.CompartmentID = $compartment_id
        RETURN DISTINCT div.*; 
        """
        response = self.execute(cmd, compartment_id=new_compartment_id, enable_cache=False)
        data = response.get_as_df()
        create_division = len(data) == 0

        # create new division, and connect new division to compartment
        if create_division:
            new_div = HAKCDivision(int(new_division_id))
            cmd = f"""
            MATCH (comp:{HAKCCompartment.get_table_name()})
            WHERE comp.CompartmentID = $compartment_id 
            CREATE (div:{HAKCDivision.get_table_name()} {{division_hash: $division_hash, DivisionID: $division_id, Salt: $salt}})-[:{HAKCDivision.relation_compartment}]->(comp:{HAKCCompartment.get_table_name()}); 
            """
            self.execute(cmd, compartment_id=new_compartment_id, division_hash=hash(new_div),
                         division_id=new_division_id, salt=new_div.salt, enable_cache=False)

        cmd = f"""
        MATCH (div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(comp:{HAKCCompartment.get_table_name()}), 
              (sym:{HAKCSymbol.get_table_name()})
        WHERE sym.{HAKCSymbol.get_primary_key()} = $symbol_hash AND comp.CompartmentID = $compartment_id AND div.DivisionID = $division_id
        CREATE (sym)-[new_div_edge]->(div);
        """
        self.execute(cmd, symbol_hash=int(_symbol.get_computed_hash()), compartment_id=new_compartment_id,
                     division_id=new_division_id, enable_cache=False)
