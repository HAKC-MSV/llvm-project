import logging
import multiprocessing as mp
from typing import Type, Optional, Tuple, cast

import pandas as pd
import copy
from .HAKCBase import HAKCDBNode, HAKCDBRelation
from .HAKCLogger import HAKCLogger
from .HAKCObjects import HAKCSymbol, HAKCFunction, HAKCScope, HAKCType, HAKCGlobalVariable, HAKCDivision, \
    HAKCCompartment, HAKCDefinitionLocation
import time
logging.setLoggerClass(HAKCLogger)

logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc-database'))

# creating thread lock for shared resource (caching system)

class HAKCDatabase:
    def __init__(self, db_dir: str, read_only: bool = False, max_num_threads=int(mp.cpu_count() / 2)):
        self.db_dir = db_dir
        self.database = None
        self.conn = None
        self.open(read_only=read_only, max_num_threads=max_num_threads)
        self.cache_hit_mutex = mp.Lock()
        self.cache_miss_mutex = mp.Lock()
        self.total_query_time_mutex = mp.Lock()
        self.query_cache_mutex = mp.Lock()
        self.cache_hit = 0
        self.cache_miss = 0
        self.total_query_time = 0
        self.query_cache = {}

    def increment_cache_hit(self):
        self.cache_hit_mutex.acquire()
        try:
            # logger.debug(f"Incrementing cache_hit {self.cache_hit}")
            self.cache_hit += 1
        finally:
            self.cache_hit_mutex.release()

    def increment_cache_miss(self):
        self.cache_miss_mutex.acquire()
        try:
#             logger.debug(f"Incrementing cache_miss {self.cache_miss}")
            self.cache_miss += 1
        finally:
            self.cache_miss_mutex.release()

    def increment_total_query_time(self, delta):
        self.total_query_time_mutex.acquire()
        try:
            self.total_query_time += delta
        finally:
            self.total_query_time_mutex.release()

    def get_cache_hits(self):
        self.cache_hit_mutex.acquire()
        out = None
        try:
            out = self.cache_hit
        finally:
            self.cache_hit_mutex.release()
            return out

    def get_cache_misses(self):
        self.cache_miss_mutex.acquire()
        out = None
        try:
            out = self.cache_miss
        finally:
            self.cache_miss_mutex.release()
            return out

    def get_total_query_time(self):
        self.total_query_time_mutex.acquire()
        out = None
        try:
            out = self.total_query_time
        finally:
            self.total_query_time_mutex.release()
            return out

    def get_query_cache(self, key):
        # TODO: skipping getting lock on read only operation, which should be atomic
        # print(f"Attempting to acquire lock...")
        # start_wait_time = time.time()
        # self.query_cache_mutex.acquire()
        # end_wait_time = time.time()
        # wait_duration = end_wait_time - start_wait_time
        # print(f"Acquired lock after waiting for {wait_duration:.4f} seconds.")
        #
        # value = None
        # try:
        #     if key in self.query_cache:
        #         self.increment_cache_hit()
        #         value = self.query_cache[key]
        # finally:
        #     self.query_cache_mutex.release()
        #     return value
        if key in self.query_cache:
            self.increment_cache_hit()
            return self.query_cache[key]
        return None

    def set_query_cache(self, key, value):
        self.query_cache_mutex.acquire()
        try:
            if key not in self.query_cache:
                self.increment_cache_miss()
                self.query_cache[key] = value
        finally:
            self.query_cache_mutex.release()

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
        stats = ""
        cache_hits, cache_misses, hit_rate = self.get_hit_rate()
        self.total_query_time_mutex.acquire()
        try:
            average_miss_penalty = (float(self.total_query_time) / float(total_queries))
            time_saved = round(total_hits * average_miss_penalty)
            stats += f"\t{total_queries} database queries took {round(self.total_query_time, 2)} seconds with an average of {round(average_miss_penalty, 2)} seconds, hit rate={round(hit_rate, 2)}%; {total_hits} cache hits saved {time_saved} seconds of cpu time."
        finally:
            self.total_query_time_mutex.release()
            return stats

    def __del__(self):
        self.close()

    def close(self):
        if self.conn is not None:
            self.conn.close()
        if self.database is not None:
            self.database.close()

    def open(self, read_only: bool = False, max_num_threads=int(mp.cpu_count() / 2)):
        import kuzu
        self.database = kuzu.Database(self.db_dir, read_only=read_only, max_num_threads=max_num_threads)
        self.conn = kuzu.Connection(self.database)  # main connection

    def new_conn(self, read_only: bool = False):
        import kuzu
        self.conn = kuzu.Connection(self.database)  # thread i connection

    def execute_prepared_stmt(self, prepared_stmt: str, **kwargs):
        start = time.time()
        response =  self.conn.execute(prepared_stmt, parameters=kwargs)
        end = time.time()
        self.increment_total_query_time(end - start)
        logger.info(self.get_cache_stats())
        return response

    @staticmethod
    def create_query_key(stmt: str, **kwargs):
        return stmt.replace(' ', '') + str(tuple(sorted(kwargs.items())))

    def execute(self, prepared_stmt: str, enable_cache = True, **kwargs):
        # adding query level caching (essentially a wrapper for conn.execute)
        if enable_cache:
            # _statement = copy.deepcopy(str(prepared_stmt))
            # _kwargs = copy.deepcopy(dict(kwargs))
            key = HAKCDatabase.create_query_key(prepared_stmt, **kwargs)
            # cache_hits, cache_misses, hit_rate = self.get_hit_rate()
            found = self.get_query_cache(key)
            if found is not None:
                # logger.info(f"Cache hit  with hit rate {hit_rate}% ({cache_hits}/{cache_hits+cache_misses})\t[Database level cache]")
                return found
            value = self.execute_prepared_stmt(prepared_stmt, **kwargs).get_as_df()
            self.set_query_cache(key, value)
            # logger.info(f"Cache miss with hit rate {hit_rate}% ({cache_hits}/{cache_hits+cache_misses})\t[Database level cache]")
            return value
        # if enable_cache:
        #     _statement = copy.deepcopy(str(prepared_stmt))
        #     _kwargs = copy.deepcopy(dict(kwargs))
        #     key = _statement.replace(' ', '') + str(tuple(sorted(_kwargs.items())))
        #     hit_rate = round(100*(float(self.cache_hit)/float(self.cache_miss))) if self.cache_miss > 0 else 0
        #
        #     if key in self.query_cache:
        #         logger.info(f"Cache hit {self.cache_miss} ({hit_rate}%)\t[Database level cache]")
        #         self.cache_miss += 1
        #         return self.query_cache[key]
        #     logger.info(f"Cache miss {self.cache_hit} ({hit_rate}%)\t[Database level cache]")
        #     self.query_cache[key] = self.execute_prepared_stmt(prepared_stmt, **kwargs).get_as_df()
        #     return self.query_cache[key]
        return self.execute_prepared_stmt(prepared_stmt, **kwargs).get_as_df()

    def get_compartment_entry_token_from_id(self, compartment_id: int) -> Optional[int]:
        cmd = f"""
        MATCH
        (comp:{HAKCCompartment.get_table_name()})<-[:{str(HAKCDivision.relation_compartment)}]-(div1:{HAKCDivision.get_table_name()})<-[:{str(HAKCSymbol.relation_division)}]-(:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_dag}]->(:{HAKCSymbol.get_table_name()})
        WHERE comp.{str(HAKCCompartment.get_primary_key())} = $compartment_id
        RETURN DISTINCT div1.DivisionID AS DivisionID
        """
        data = self.execute(cmd, compartment_id=compartment_id).to_dict(orient='records')
        target_divisions = set()
        for division_id_dict in data:
            division_id = division_id_dict['DivisionID'] if 'DivisionID' in division_id_dict else None
            if division_id:
                target_divisions.add(int(division_id))
        return HAKCCompartment.compute_entry_token(compartment_id, target_divisions)

    def get_division_access_token_from_id(self, division_id: int, compartment_id: int) -> Optional[int]:
        cmd = f"""
        MATCH
        (comp:{HAKCCompartment.get_table_name()})<-[:{str(HAKCDivision.relation_compartment)}]-(div1:{HAKCDivision.get_table_name()})<-[:{str(HAKCSymbol.relation_division)}]-(:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_dag}]->(:{HAKCSymbol.get_table_name()})-[:{str(HAKCSymbol.relation_division)}]->(div2:{HAKCDivision.get_table_name()})-[:{str(HAKCDivision.relation_compartment)}]->(comp)
        WHERE comp.{str(HAKCCompartment.get_primary_key())} = $compartment_id AND div1.DivisionID = $division_id
        RETURN DISTINCT div2.DivisionID AS DivisionID
        """
        data = self.execute(cmd, division_id=division_id, compartment_id=compartment_id).to_dict(orient='records')
        division_ids = {division_id}
        for division_id_dict in data:
            division_id = division_id_dict['DivisionID'] if 'DivisionID' in division_id_dict else None
            if division_id:
                division_ids.add(int(division_id))
        return HAKCDivision.compute_access_token(compartment_id, division_ids)

    def get_division(self, division_id: int, compartment_id: int) -> Optional[HAKCDivision]:
        access_token = self.get_division_access_token_from_id(division_id, compartment_id)
        cmd = f"""
        MATCH (div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(c:{HAKCCompartment.get_table_name()})
        WHERE div.DivisionID = $division_id AND c.CompartmentID = $compartment_id
        RETURN DISTINCT div.DivisionID AS DivisionID, div.Salt AS Salt, div.{str(HAKCDivision.get_primary_key())} AS division_hash
        """
        data = self.execute(cmd, division_id=division_id, compartment_id=compartment_id).to_dict(orient='records')
        # list of dictionaries
        if len(data) == 1:
            division = HAKCDivision(AccessToken=access_token, **data[0])
            return division
        return None

    def get_division_id_compartment_id_from_symbol(self, symbol: HAKCSymbol) -> Optional[
        Tuple[HAKCDivision, HAKCCompartment]]:
        cmd = f"""        
        MATCH (scope:{HAKCScope.get_table_name()})<-[:{HAKCSymbol.relation_scope}]-(sym:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_division}]->(div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(comp:{HAKCCompartment.get_table_name()})
        WITH sym.Name as Name, scope.Scope as Scope, div.DivisionID as DivisionID, div.Salt as Salt, comp.CompartmentID as CompartmentID
        WHERE Name = $Name AND Scope = $Scope
        RETURN DISTINCT DivisionID, Salt, CompartmentID;
        """
        data = self.execute(cmd, Name=symbol.name, Scope=symbol.scope.scope).to_dict(orient='records')
        if len(data) == 1:
            division = HAKCDivision(**data[0])
            compartment = HAKCCompartment(**data[0])
            logger.debug(f"Found ({division}, {compartment}) for symbol: {symbol}")
            return division, compartment
        logger.debug(f'Command: {cmd} returned None\n')
        logger.debug(f'Searched with Name: {symbol.name}, Scope: {str(symbol.scope)}')
        return None

    def get_valid_targets_from_compartment_id(self, source_compartment_id: int) -> list[int]:
        # TODO: double check this
        # NOTE: comp1 is fixed to the caller's compartment
        # from the perspective of the caller, the compartment_id is their own and they are looking for compartment_ids of targets
        # Get valid target compartments given compartment id
        # comp1 <- div1 <- symbol1 -(Dag2)-> symbol2 -> div2 -> comp2
        cmd = f"""
        MATCH (comp1:{HAKCCompartment.get_table_name()})<-[:{HAKCDivision.relation_compartment}]-(div1:{HAKCDivision.get_table_name()})<-[:{HAKCSymbol.relation_division}]-(sym1:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_dag}]->(sym2:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_division}]->(div2:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(comp2:{HAKCCompartment.get_table_name()})
        WITH  *
        WHERE comp1.CompartmentID = $source_compartment_id
        RETURN DISTINCT comp2.CompartmentID;
        """
        data = self.execute(cmd, source_compartment_id=source_compartment_id).to_dict(orient='records')
        targets = set()
        for entry in data:
            target_id = entry["comp2.CompartmentID"] if "comp2.CompartmentID" in entry else None
            if target_id:
                logger.debug(f"Found valid_targets from {entry['comp1.CompartmentID']} to {target_id}")
                targets.add(int(target_id))
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
        RETURN sym.{str(HAKCSymbol.get_primary_key())} AS symbol_hash;
        """
        data = self.execute(cmd).to_dict(orient='records')
        all_symbol_hashes = set()
        for entry in data:
            symbol_hash = entry["symbol_hash"] if "symbol_hash" in entry else None
            if symbol_hash:
                all_symbol_hashes.add(int(symbol_hash))
        return list(all_symbol_hashes)

    def get_symbol_by_hash(self, symbol_hashes: list[int], **kwargs) -> set[HAKCSymbol]:
        assert(isinstance(symbol_hashes, list))
        return self._get_symbols(where=f'HAKCSymbol.symbol_hash in [{", ".join([str(sh) for sh in symbol_hashes])}]', **kwargs)

    def get_single_symbol_by_hash(self, symbol_hash: int) -> HAKCSymbol:
        assert(isinstance(symbol_hash, int))
        return list(self._get_symbols(symbol_hash=symbol_hash))[0]

    def get_symbols_by_name(self, symbol_name: str) -> list[HAKCSymbol]:
        result = self._get_symbols(symbol_name=symbol_name)
        return list(result)

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
        MATCH (div:{HAKCDivision.get_table_name()})
        RETURN div.DivisionID as DivisionID, div.Salt as Salt, div.{str(HAKCDivision.get_primary_key())} as division_hash
        """
        data = self.execute(cmd).to_dict(orient='records')
        # divisions = set()
        # use list instead of set becasue we allow for duplicates
        divisions = list()
        # if response.has_next():
        for entry in data:
            # divisions.add(HAKCDivision(**entry))
            divisions.append(HAKCDivision(**entry))
        return divisions

    def get_all_compartments(self):
        cmd = f"""
        MATCH ({HAKCCompartment.get_table_name()}:{HAKCCompartment.get_table_name()})
        RETURN {HAKCCompartment.get_table_name()}.CompartmentID as CompartmentID;
        """
        compartments = set()
        data = self.execute(cmd).to_dict(orient='records')
        for entry in data:
            compartments.add(HAKCCompartment(**entry))
        return compartments

    def get_symbol_definition_location(self, symbol: HAKCSymbol) -> Optional[HAKCDefinitionLocation]:
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()})-[e:{HAKCSymbol.relation_definition_location}]->(dl:{HAKCDefinitionLocation.get_table_name()})
        WHERE sym.{str(HAKCSymbol.get_primary_key())} = $symbol_hash
        RETURN dl.DefiningFile as DefiningFile, e.DefiningLine as DefiningLine;
        """
        data = self.execute(cmd, symbol_hash=hash(symbol)).to_dict(orient='records')
        if len(data) == 1:
            return HAKCDefinitionLocation(**data[0])
        return None

    def get_dag_computation_edges(self, symbol_hash: int) -> dict[str, list[int]]:
        # TODO: update this to use df for consistency?
        result = dict()
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()})-[:{HAKCFunction.relation_indirect_calls}]->(:{HAKCType.get_table_name()})<-[:{HAKCSymbol.relation_type}]-(indirect:{HAKCSymbol.get_table_name()})
        WHERE sym.{str(HAKCSymbol.get_primary_key())} = $symbol_hash
        RETURN DISTINCT indirect.{str(HAKCSymbol.get_primary_key())} AS {HAKCFunction.relation_direct_calls}
        """
        response = self.execute_prepared_stmt(cmd, symbol_hash=symbol_hash)
        df = response.get_as_pl()
        for table_name, entries in df.to_dict(as_series=False).items():
            # logger.error(f"Got response: {table_name} -> {entries}")
            if len(entries) > 0:
                result[table_name] = entries
        cmd = f"""
        MATCH (sym: {HAKCSymbol.get_table_name()})-[:{HAKCFunction.relation_direct_calls}]->(direct:{HAKCSymbol.get_table_name()})
        WHERE sym.{str(HAKCSymbol.get_primary_key())} = $symbol_hash
        RETURN DISTINCT direct.{str(HAKCSymbol.get_primary_key())} AS {HAKCFunction.relation_direct_calls}
        """
        response = self.execute_prepared_stmt(cmd, symbol_hash=symbol_hash)
        df = response.get_as_pl()
        for table_name, entries in df.to_dict(as_series=False).items():
            if len(entries) > 0:
                result[table_name] = entries
        cmd = f"""
        MATCH (sym: {HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_symbol}]->(uses:{HAKCSymbol.get_table_name()})
        WHERE sym.{str(HAKCSymbol.get_primary_key())} = $symbol_hash
        RETURN DISTINCT uses.{str(HAKCSymbol.get_primary_key())} AS {HAKCSymbol.relation_symbol}
        """
        response = self.execute_prepared_stmt(cmd, symbol_hash=symbol_hash)
        df = response.get_as_pl()
        for table_name, entries in df.to_dict(as_series=False).items():
            if len(entries) > 0:
                result[table_name] = entries
        return result

    def insert_from_dataframe(self, table_name: str, df: pd.DataFrame):
        self.conn.execute(f'COPY {table_name} FROM df')

    def _create_perm_edge_from_response(self, perm_prefix: str = "rwx.", **kwargs):
        perm_data = {key.removeprefix(perm_prefix): val for key, val in kwargs.items() if key.startswith(perm_prefix)}
        if len(perm_data) == 0:
            raise RuntimeError('No type data provided')
        return perm_data

    def _create_type_from_response(self, type_prefix: str = "ty.", **kwargs) -> HAKCType:
        type_data = {key.removeprefix(type_prefix): val for key, val in kwargs.items()}
        if len(type_data) == 0:
            raise RuntimeError('No type data provided')
        ty = HAKCType(**type_data)
        return ty

    def _create_symbol_from_response(self, is_function: bool, type_prefix: str = "ty.", scope_prefix: str = "scope.",
                                     symbol_prefix: str = "sym.", **kwargs) -> HAKCSymbol:
        ty = self._create_type_from_response(type_prefix=type_prefix, **kwargs)
        scope_data = {key.removeprefix(scope_prefix): val for key, val in kwargs.items()}
        if len(scope_data) == 0:
            raise RuntimeError('No scope data provided')
        scope = HAKCScope(**scope_data)
        symbol_data = {key.removeprefix(symbol_prefix): val for key, val in kwargs.items()}
        if len(symbol_data) == 0:
            raise RuntimeError('No symbol data provided')
        symbol_data['Type'] = ty
        symbol_data['Scope'] = scope

        if is_function:
            symbol = HAKCFunction(**symbol_data)
        else:
            symbol = HAKCGlobalVariable(**symbol_data)

        return symbol

    def get_indirect_calls(self, symbol: HAKCSymbol) -> list[HAKCType]:
        cmd = f"""
            MATCH (head: {HAKCSymbol.get_table_name()})-[:{HAKCFunction.relation_indirect_calls}]->(ty: {HAKCType.get_table_name()})
            WHERE head.symbol_hash = $symbol_hash
            RETURN ty.DebugType, ty.LLVMType
            ORDER BY ty.DebugType, ty.LLVMType;
        """
        try:
            data = self.execute(cmd, symbol_hash=hash(symbol)).to_dict(orient='records')
            types = []
            for entry in data:
                ty = self._create_type_from_response(**entry)
                types.append(ty)
        except Exception as e:
            logger.error(f'get_indirect_calls failed')
            raise e
        return types

    def get_direct_calls(self, symbol: HAKCSymbol) -> list[HAKCType]:
        cmd = f"""
                MATCH (head: {HAKCSymbol.get_table_name()})-[:{HAKCFunction.relation_direct_calls}]->(tail: {HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_type}]->(ty:{HAKCType.get_table_name()}),
                (tail)-[{HAKCSymbol.relation_scope}]->(scope:{HAKCScope.get_table_name()})
                WHERE head.symbol_hash = $symbol_hash and head.symbol_hash <> tail.symbol_hash
                RETURN DISTINCT head.*, tail.*, ty.*, scope.*;
            """
        try:
            data = self.execute(cmd, symbol_hash=hash(symbol)).to_dict(orient='records')
            direct_calls = set()
            for entry in data:
                if entry["head.IsFunction"] and entry["tail.IsFunction"]:
                    call = self._create_symbol_from_response(is_function=True, symbol_prefix='tail.', **entry)
                    direct_calls.add(call)
        except Exception as e:
            logger.error(f'get_direct_calls failed')
            raise e
        return list(direct_calls)

    def get_used_symbols(self, symbol: HAKCSymbol):
        cmd = f"""
            MATCH (head:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_symbol}]->(tail:{HAKCSymbol.get_table_name()}),
            (sc:{HAKCScope.get_table_name()})<-[:{HAKCSymbol.relation_scope}]-(tail)-[:{HAKCSymbol.relation_type}]->(ty:{HAKCType.get_table_name()})
            WHERE head.symbol_hash=$symbol_hash
            RETURN tail.Name, tail.IsFunction AS is_function, sc.Scope,
            sc.LocalScopeName, ty.DebugType, ty.LLVMType;
        """
        try:
            data = self.execute(cmd, symbol_hash=hash(symbol)).to_dict(orient='records')
            used_symbols = set()
            for entry in data:
                symbol = self._create_symbol_from_response(symbol_prefix='tail.', scope_prefix='sc.', type_prefix='ty.', **entry)
                used_symbols.add(symbol)
        except Exception as e:
            logger.error(f'get_used_symbols failed')
            raise e
        return list(used_symbols)

    @staticmethod
    def __get_class_db_columns(cls):
        return [x.column_name for x in cls.get_data_columns()] + [cls.get_primary_key().column_name]

    @staticmethod
    def __create_object_from_response(cls, **data):
        if cls == HAKCDefinitionLocation:
            cls_data = {key.removeprefix(f"{cls.get_table_name()}."): val for key, val in data.items()}
            return HAKCDefinitionLocation(**cls_data)
        elif cls == HAKCFunction or cls == HAKCGlobalVariable:
            data["HAKCSymbol.Scope"] = HAKCDatabase.__create_object_from_response(HAKCScope, **data)
            data["HAKCSymbol.Type"] = HAKCDatabase.__create_object_from_response(HAKCType, **data)
            if "HAKCDefinitionLocation.DefiningLine" in data:
                data["HAKCSymbol.DefinitionLocation"] = HAKCDatabase.__create_object_from_response(HAKCDefinitionLocation, **data)
            cls_data = {key.removeprefix(f"{cls.get_table_name()}."): val for key, val in data.items()}
            if cls == HAKCFunction:
                return HAKCFunction(**cls_data)
            if cls == HAKCGlobalVariable:
                return HAKCGlobalVariable(**cls_data)
        elif cls == HAKCDivision:
            compartment = HAKCDatabase.__create_object_from_response(HAKCCompartment, **data)
            div_data = {key.removeprefix(f"{cls.get_table_name()}."): val for key, val in data.items() if key.startswith(cls.get_table_name())}
            return HAKCDivision(**div_data), compartment
        elif cls == HAKCCompartment:
            cls_data = {key.removeprefix(f"{cls.get_table_name()}."): val for key, val in data.items() if key.startswith(cls.get_table_name())}
            return HAKCCompartment(**cls_data)

        cls_data = {key.removeprefix(f"{cls.get_table_name()}."): val for key, val in data.items() if key.startswith(cls.get_table_name())}
        if len(cls_data) == 0:
            raise RuntimeError('No type data provided')
        return cls(**cls_data)


    def get_symbols(self):
        return self._get_symbols()

    def create_node_table(self, node_type: Type[HAKCDBNode]):
        create_cmd = f'CREATE NODE TABLE IF NOT EXISTS {node_type.get_table_definition()}'
        self.execute(create_cmd, enable_cache=False)

    def create_relationship_table(self, edge_type: HAKCDBRelation):
        create_cmd = f'CREATE REL TABLE IF NOT EXISTS {edge_type.get_definition()}'
        self.execute(create_cmd, enable_cache=False)

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

        # print(cmd)
        data = self.execute(cmd, symbol_hash=hash(_symbol)).to_dict(orient='records')
        dag_edges = set()
        for entry in data:
            if entry["HAKCSymbol.IsFunction"]:
                func = HAKCDatabase.__create_object_from_response(HAKCFunction, **entry)
                dag_edge = (func, entry[f"{HAKCSymbol.relation_symbol}.weight"])
                dag_edges.add(dag_edge)
                # print(f"Found DAG: {dag_edge}")
            else:
                gv = HAKCDatabase.__create_object_from_response(HAKCGlobalVariable, **entry)
                dag_edge = (gv, entry[f"{HAKCSymbol.relation_symbol}.weight"])
                dag_edges.add(dag_edge)
                # print(f"Found DAG: {dag_edge}")
        return dag_edges


    def get_division_compartment(self, _symbol: HAKCSymbol) -> tuple[HAKCDivision, HAKCCompartment]:
        assert(isinstance(_symbol, HAKCSymbol))
        logger.debug(_symbol)

        symbol_attrs = HAKCDatabase.get_object_attributes(HAKCSymbol)
        division_attrs = HAKCDatabase.get_object_attributes(HAKCDivision)
        compartment_attrs = HAKCDatabase.get_object_attributes(HAKCCompartment)
        scope_attrs = HAKCDatabase.get_object_attributes(HAKCScope)
        cmd = f"""
        MATCH ({HAKCScope.get_table_name()})<-[:{HAKCSymbol.relation_scope}]-({HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_division}]->({HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->({HAKCCompartment.get_table_name()})
        WHERE {HAKCSymbol.get_table_name()}.{HAKCSymbol.get_primary_key()} = $symbol_hash  
        RETURN DISTINCT {scope_attrs}, {symbol_attrs}, {division_attrs}, {compartment_attrs};
        """
        data = self.execute(cmd, symbol_hash=hash(_symbol)).to_dict(orient='records')
        div, comp = None, None
        if len(data) == 1:
            div, comp = HAKCDatabase.__create_object_from_response(HAKCDivision, **data[0])
        return div, comp

    @staticmethod
    def get_object_attributes(cls):
        assert(cls in {HAKCType, HAKCScope, HAKCSymbol, HAKCFunction, HAKCDefinitionLocation, HAKCDivision, HAKCCompartment})
        return ", ".join([f"{cls.get_table_name()}.{x.column_name}" for x in cls.get_data_columns()] + [f"{cls.get_table_name()}.{cls.get_primary_key()}"])

    def _get_symbols(self, symbol_name: str = None, symbol_hash: int = None, where: str = None, deep = False ) -> set[HAKCSymbol]:
        # TODO: add function level caching
        # want to reconstruct the output from the yaml exactly, so the compartmentalization can be rebuilt from the database
        type_attrs = HAKCDatabase.get_object_attributes(HAKCType)
        scope_attrs = HAKCDatabase.get_object_attributes(HAKCScope)
        symbol_attrs = HAKCDatabase.get_object_attributes(HAKCSymbol)
        dl_attrs = HAKCDatabase.get_object_attributes(HAKCDefinitionLocation)
        cmd = f"""
        MATCH ({HAKCType.get_table_name()}:{HAKCType.get_table_name()})<-[{HAKCSymbol.relation_type}:{HAKCSymbol.relation_type}]-({HAKCSymbol.get_table_name()}:{HAKCSymbol.get_table_name()})-[{HAKCSymbol.relation_scope}:{HAKCSymbol.relation_scope}]->({HAKCScope.get_table_name()}:{HAKCScope.get_table_name()})
        WHERE TRUE { f' AND {HAKCSymbol.get_table_name()}.Name=$symbol_name' if symbol_name else '' } { f' AND {HAKCSymbol.get_table_name()}.{HAKCSymbol.get_primary_key()}=$symbol_hash' if symbol_hash else '' } { f'AND {where}' if where else ''}
        OPTIONAL MATCH ({HAKCSymbol.get_table_name()})-[{HAKCSymbol.relation_definition_location}:{HAKCSymbol.relation_definition_location}]->({HAKCDefinitionLocation.get_table_name()}:{HAKCDefinitionLocation.get_table_name()})
        RETURN DISTINCT {type_attrs}, {scope_attrs}, {symbol_attrs}, {dl_attrs}, {HAKCSymbol.relation_definition_location}.DefiningLine AS DefiningLine;
        """
        # logger.error(f"running command: {cmd}")

        cmdargs = dict()
        if symbol_name:
            cmdargs['symbol_name'] = symbol_name
        if symbol_hash:
            cmdargs['symbol_hash'] = symbol_hash
        data = self.execute(cmd, **cmdargs).to_dict(orient='records')
        # logger.fatal(response)
        functions = set()
        gvs = set()
        for entry in data:
            entry["HAKCDefinitionLocation.DefiningLine"] = entry["DefiningLine"]
            del entry["DefiningLine"]

            # if no definition found, then remove empty keys
            if 'HAKCDefinitionLocation.DefiningFile' in entry:
                del entry['HAKCDefinitionLocation.DefiningFile']
            if 'HAKCDefinitionLocation.DefiningLine' in entry:
                del entry['HAKCDefinitionLocation.DefiningLine']

            if entry["HAKCSymbol.IsFunction"] is True:
                func = HAKCDatabase.__create_object_from_response(HAKCFunction, **entry)
                functions.add(func)
            else:
                gv = HAKCDatabase.__create_object_from_response(HAKCGlobalVariable, **entry)
                gvs.add(gv)

        # the 'base' HAKCSymbol is now created, now look for all symbols used, direct calls, indirect calls, types used
        # Note: Speeding up performance by only doing a 'shallow' query of the direct calls, since we only need to know enough to create the symbol hash
        if deep:
            for func in functions:
                # print(func)
                used_symbols = self.get_used_symbols(func)
                # print(f"used_symbols: {used_symbols}")
                for used_symbol in used_symbols:
                    func.used_symbols.append(used_symbol)

                direct_calls = self.get_direct_calls(func)
                # print(f"direct_calls: {direct_calls}")
                for direct_call in direct_calls:
                    func.direct_calls.append(direct_call)

                indirect_calls = self.get_indirect_calls(func)
                # print(f"indirect_calls: {indirect_calls}")
                for indirect_call in indirect_calls:
                    func.indirect_calls.append(indirect_call)

        symbols = functions.union(gvs)
        # logger.fatal(f"Returning symbols {symbols}")
        return symbols

    def get_symbol_hash(self, Name, DefiningFile, DefiningLine):
        cmd = f"""
            MATCH (sym:{HAKCSymbol.get_table_name()})-[{HAKCSymbol.relation_definition_location}]->(dl:{HAKCDefinitionLocation.get_table_name()})
            WHERE sym.Name=$Name AND dl.DefiningFile=$DefiningFile AND dl.DefiningLine=$DefiningLine
            RETURN DISTINCT sym.{HAKCSymbol.get_primary_key()}
        """
        data = self.execute(cmd, Name=Name, DefiningFile=DefiningFile, DefiningLine=DefiningLine).to_dict(orient='records')
        if len(data) == 1:
            return data[0][f"sym.{HAKCSymbol.get_primary_key()}"]
        logger.fatal(f"Queried symbol hash from (Name, DefiningFile, DefiningLine) = ({Name}, {DefiningFile}, {DefiningLine}), but could not find symbol hash!")
        return None

    def set_division_compartment_id_by_symbol(self, _symbol : HAKCSymbol, new_division_id: int, new_compartment_id: int):

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
        RETURN div.*; 
        """
        response = self.execute(cmd, compartment_id=new_compartment_id, enable_cache=False)
        data = response.get_as_df().to_dict(orient='records')
        create_division = len(data) == 0

        # create new division, and connect new division to compartment
        if create_division:
            new_div = HAKCDivision(new_division_id)
            cmd = f"""
            MATCH (comp:{HAKCCompartment.get_table_name()})
            WHERE comp.CompartmentID = $compartment_id 
            CREATE (div:{HAKCDivision.get_table_name()} {{division_hash: $division_hash, DivisionID: $division_id, Salt: $salt}})-[:{HAKCDivision.relation_compartment}]->(comp:{HAKCCompartment.get_table_name()}); 
            """
            self.execute(cmd, compartment_id=new_compartment_id, division_hash=hash(new_div), division_id=new_division_id, salt=new_div.salt, enable_cache=False)

        cmd = f"""
        MATCH (div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(comp:{HAKCCompartment.get_table_name()}), 
              (sym:{HAKCSymbol.get_table_name()})
        WHERE sym.{HAKCSymbol.get_primary_key()} = $symbol_hash AND comp.CompartmentID = $compartment_id AND div.DivisionID = $division_id
        CREATE (sym)-[new_div_edge]->(div);
        """
        self.execute(cmd, symbol_hash=int(_symbol.get_computed_hash()), compartment_id=new_compartment_id, division_id=new_division_id, enable_cache=False)
