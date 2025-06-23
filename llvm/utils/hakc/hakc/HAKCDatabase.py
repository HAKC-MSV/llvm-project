import logging
import multiprocessing as mp
from typing import Type, Optional, Tuple

import pandas as pd

from .HAKCBase import HAKCDBNode, HAKCDBRelation
from .HAKCObjects import HAKCSymbol, HAKCFunction, HAKCScope, HAKCTypePerm, HAKCType, HAKCGlobalVariable, HAKCDivision, \
    HAKCCompartment, HAKCCompilationUnit

logger = logging.getLogger('hakc-dag')


class HAKCDatabase:
    def __init__(self, db_dir: str, read_only: bool = False, max_num_threads=int(mp.cpu_count() / 2)):
        self.db_dir = db_dir
        self.database = None
        self.conn = None
        self.open(read_only=read_only, max_num_threads=max_num_threads)
        self.count_executed_statements = 0
        self.query_history = []

    def close(self):
        print("closing connection")
        if self.conn is not None:
            self.conn.close()
        print("closing database")
        if self.database is not None:
            self.database.close()

    def open(self, read_only: bool = False, max_num_threads=int(mp.cpu_count() / 2)):
        import kuzu
        self.database = kuzu.Database(self.db_dir, read_only=read_only, max_num_threads=max_num_threads)
        self.conn = kuzu.Connection(self.database)  # main connection

    def new_conn(self, read_only: bool = False):
        import kuzu
        self.conn = kuzu.Connection(self.database)  # thread i connection

    def get_compartment_entry_token_from_id(self, compartment_id: int) -> Optional[int]:
        cmd = f"""
        MATCH (comp:{HAKCCompartment.get_table_name()})
        WITH comp.CompartmentID as compartment_id, comp.EntryToken as entry_token
        WHERE compartment_id = $compartment_id
        RETURN entry_token;
        """
        response = self.execute_prepared_stmt(cmd, compartment_id=compartment_id)
        ret = response.get_as_df()
        if ret.empty:
            logger.error(f'Command: {cmd} returned None')
            logger.error(f'Searched with compartment_id: {compartment_id}')
            return None
        else:
            entry_token = ret["entry_token"][0]
            logger.error(f"Found entry_token: {entry_token} for compartment_id: {compartment_id}")
            return int(entry_token)

    def get_division_access_token_from_id(self, division_id: int, compartment_id: int) -> Optional[int]:
        cmd = f"""
        MATCH (div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.InCompartmentTable}]->(comp:{HAKCCompartment.get_table_name()})
        WITH div.DivisionID as division_id, div.AccessToken as access_token, comp.CompartmentID AS compartment_id, comp.EntryToken AS entry_token
        WHERE division_id = $division_id AND compartment_id = $compartment_id 
        RETURN access_token, entry_token;
        """
        response = self.execute_prepared_stmt(cmd, division_id=division_id, compartment_id=compartment_id)
        ret = response.get_as_df()
        if ret.empty:
            logger.error(f'Command: {cmd} returned None')
            logger.error(f'Searched with division_id: {division_id}, compartment_id: {compartment_id}')
            return None
        else:
            access_token = ret["access_token"][0]
            logger.error(f"Found access_token: {access_token} for division_id: {division_id}")
            return int(access_token)

    def get_division_id_compartment_id_from_symbol(self, symbol: HAKCSymbol) -> Optional[Tuple[int, int, int, int]]:
        cmd = f"""        
        MATCH (scope:{HAKCScope.get_table_name()})<-[:{HAKCSymbol.HasScopeTable}]-(sym:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.InDivisionTable}]->(div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.InCompartmentTable}]->(comp:{HAKCCompartment.get_table_name()})
        WITH sym.Name as Name, scope.Scope as Scope, div.DivisionID as division_id, div.AccessToken as access_token, comp.CompartmentID as compartment_id, comp.EntryToken as entry_token
        WHERE Name = $Name AND Scope = $Scope
        RETURN division_id, access_token, compartment_id, entry_token
        """
        response = self.execute_prepared_stmt(cmd, Name=symbol.name, Scope=symbol.scope.scope)
        # TODO: double check that this only returns one row
        ret = response.get_as_df()
        if ret.empty:
            logger.error(f'Command: {cmd} returned None\n')
            logger.error(f'Searched with Name: {symbol.name}, Scope: {str(symbol.scope)}')
            return None
        else:
            # TODO: check that this is correct when this is eventually called
            division_id = ret["division_id"][0]
            access_token = ret["access_token"][0]
            compartment_id = ret["compartment_id"][0]
            entry_token = ret["entry_token"][0]
            logger.error(
                f"Found division_id, access_token, compartment_id, entry_token: ({division_id}, {access_token}, {compartment_id}, {entry_token}) for symbol: {symbol}")
            # need to cast to int because json cant parse numpy.uint64s apparently
            return int(division_id), int(access_token), int(compartment_id), int(entry_token)

    def get_valid_targets_from_compartment_id(self, source_compartment_id: int) -> list[int]:
        # TODO: double check this
        # NOTE: comp1 is fixed to the caller's compartment
        # from the perspective of the caller, the compartment_id is their own and they are looking for compartment_ids of targets
        # Get valid target compartments given compartment id
        # comp1 <- div1 <- symbol1 -(Dag2)-> symbol2 -> div2 -> comp2
        cmd = f"""
        MATCH (comp1:{HAKCCompartment.get_table_name()})<-[:{HAKCDivision.InCompartmentTable}]-(div1:{HAKCDivision.get_table_name()})<-[:{HAKCSymbol.InDivisionTable}]-(sym1:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.DagEdgeTable}]->(sym2:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.InDivisionTable}]->(div2:{HAKCDivision.get_table_name()})-[:{HAKCDivision.InCompartmentTable}]->(comp2:{HAKCCompartment.get_table_name()})
        WITH  *
        WHERE comp1.CompartmentID = $source_compartment_id
        RETURN comp1.CompartmentID, comp2.CompartmentID, comp2.EntryToken;
        """
        response = self.execute_prepared_stmt(cmd, source_compartment_id=source_compartment_id)
        data = response.get_as_df()
        if data.empty:
            logger.debug(f'Command: {cmd} returned None\n')
            logger.debug(f'Searched with CompartmentID: {source_compartment_id}')
            return list()
        else:
            # using dictionary to remove duplicates
            targets = list()
            for index, row in data.iterrows():
                target_id = row["comp2.CompartmentID"]
                # entry_token = row["comp2.EntryToken"] // probably don't need entry token here
                if target_id not in targets:
                    logger.debug(f"Found valid_targets from {row['comp1.CompartmentID']} to {target_id}")
                    targets.append(int(target_id))

            return targets

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
        symbol = HAKCSymbol.get_table_name()
        symbol_hash = HAKCSymbol.get_primary_key()
        cmd = f"""
        MATCH ({symbol})
        WHERE {symbol}.{symbol_hash} IS NOT NULL
        RETURN DISTINCT {symbol}.{symbol_hash};
        """
        response = self.execute_prepared_stmt(cmd)
        ret = response.get_as_df()[f"{symbol}.{symbol_hash}"].to_list()
        return ret

    def get_symbol_by_hash(self, symbol_hashes: list[int], **kwargs) -> list[HAKCSymbol]:
        symbol = HAKCSymbol.get_table_name()
        symbol_hash = HAKCSymbol.get_primary_key()
        result = self._get_symbols(
            where_clause=f'WHERE {symbol}.{symbol_hash} in [{", ".join([str(sh) for sh in symbol_hashes])}]', **kwargs)
        return result

    def get_symbol_hashes_to_symbols(self):
        symbol_hashes = self.get_all_symbol_hashes()
        symbols = self.get_symbol_by_hash(symbol_hashes, assume_defined=False)
        try:
            assert(len(symbol_hashes) == len(symbols))
        except AssertionError as e:
            print(f"Asserting len(symbol_hashes) == len(symbols); {len(symbol_hashes)} =?= {len(symbols)}")
            print(e)
        return dict(zip(symbol_hashes, symbols))


    def delete_all_compartments(self):
        div = HAKCDivision.get_table_name()
        comp = HAKCCompartment.get_table_name()
        div_comp_edge = HAKCDivision.relation_compartment
        cmd = f"""
        MATCH ({div})-[:{div_comp_edge}]->({comp})
        DETACH DELETE {div};
        """
        self.execute_prepared_stmt(cmd)
        cmd = f"""
        MATCH ({comp})
        DETACH DELETE {comp};
        """
        self.execute_prepared_stmt(cmd)

    def get_all_divisions(self):
        div = HAKCDivision.get_table_name()
        comp = HAKCCompartment.get_table_name()
        div_comp_edge = HAKCDivision.relation_compartment
        cmd = f"""
        MATCH ({div})-[:{div_comp_edge}]->({comp})
        RETURN {div}.DivisionID, {comp}.CompartmentID;
        """
        # RETURN div.DivisionID as DivisionID, c.CompartmentID AS compartment_id
        divisions = set()
        response = self.execute_prepared_stmt(cmd)
        if response.has_next():
            info = response.get_as_df()
            for data in info.to_dict(orient='records'):
                divisions.add(HAKCDivision(**data))
        return divisions

    def get_symbol_definition_location(self, symbol: HAKCSymbol) -> tuple[HAKCCompilationUnit, int] | None:
        symbol = HAKCSymbol.get_table_name()
        symbol_hash = HAKCSymbol.get_primary_key()
        # symbol_hash = HAKCSymbol.get_primary_key().column_name
        symbol_cu_edge = HAKCSymbol.relation_compilation_unit
        cu = HAKCCompilationUnit.get_table_name()
        cmd = f"""
        MATCH ({symbol})-[{symbol_cu_edge}]->({cu})
        WHERE {symbol}.{symbol_hash} = $symbol_hash
        RETURN {cu}.filename, {symbol_cu_edge}.line;
        """
        response = self.execute_prepared_stmt(cmd, symbol_hash=hash(symbol))
        if response.has_next():
            resp_dict = response.get_as_df().to_dict(orient='records')
            logger.info(f'resp_dict df: {resp_dict}')
            return HAKCCompilationUnit(filename=resp_dict['filename'][0]), resp_dict['line'][0]
        return None

    def get_dag_computation_edges(self, symbol_hash: int) -> dict[str, list[int]]:
        symbol = HAKCSymbol.get_table_name()
        _symbol_hash = HAKCSymbol.get_primary_key()
        symbol_cu_edge = HAKCSymbol.relation_compilation_unit
        symbol_type_indirect_call_edge = HAKCFunction.relation_indirect_calls
        symbol_symbol_direct_call_edge = HAKCFunction.relation_direct_calls
        symbol_type_edge = HAKCSymbol.relation_type
        symbol_symbol_edge = HAKCSymbol.relation_symbol
        cu = HAKCCompilationUnit.get_table_name()
        _type = HAKCType.get_table_name()
        result = dict()
        # TODO: double check this derrick
        cmd = f"""
        MATCH ({symbol})-[:{symbol_type_indirect_call_edge}]->(:{_type})<-[:{symbol_type_edge}]-(indirect:{symbol})
        WHERE {symbol}.{_symbol_hash} = $symbol_hash
        RETURN DISTINCT indirect.{_symbol_hash} AS {symbol_type_indirect_call_edge}
        """
        response = self.execute_prepared_stmt(cmd, symbol_hash=symbol_hash)
        df = response.get_as_pl()
        for table_name, entries in df.to_dict(as_series=False).items():
            if len(entries) > 0:
                result[table_name] = entries
        cmd = f"""
        MATCH (sym: {symbol})-[:{symbol_symbol_direct_call_edge}]->(direct:{symbol})
        WHERE sym.{_symbol_hash} = $symbol_hash
        RETURN DISTINCT direct.{_symbol_hash} AS {symbol_symbol_direct_call_edge}
        """
        response = self.execute_prepared_stmt(cmd, symbol_hash=symbol_hash)
        df = response.get_as_pl()
        for table_name, entries in df.to_dict(as_series=False).items():
            if len(entries) > 0:
                result[table_name] = entries
        cmd = f"""
        MATCH (sym: {symbol})-[:{symbol_symbol_edge}]->(uses:{symbol})
        WHERE sym.{_symbol_hash} = $symbol_hash
        RETURN DISTINCT uses.{_symbol_hash} AS {symbol_symbol_edge}
        """
        response = self.execute_prepared_stmt(cmd, symbol_hash=symbol_hash)
        df = response.get_as_pl()
        for table_name, entries in df.to_dict(as_series=False).items():
            if len(entries) > 0:
                result[table_name] = entries
        return result

    def execute_prepared_stmt(self, prepared_stmt: str, **kwargs):
        self.count_executed_statements += 1
        # print(f"Executing statement: {prepared_stmt} with {kwargs}")
        response = self.conn.execute(prepared_stmt, parameters=kwargs)
        self.query_history.append((prepared_stmt, response))
        return response

    def insert_from_dataframe(self, table_name: str, df: pd.DataFrame):
        self.conn.execute(f'COPY {table_name} FROM df')

    def _get_symbols(self, where_clause: None | str = None, limit: int = 0, assume_defined: bool = True) -> list[HAKCSymbol] | int:
        # assumes all symobls are defined, but this may not be a valid assumption
        # print(f"assume_defined is {assume_defined}")
        symbol = HAKCSymbol.get_table_name()
        symbol_hash = HAKCSymbol.get_primary_key()
        # symbol_hash = HAKCSymbol.get_primary_key().column_name
        symbol_cu_edge = HAKCSymbol.relation_compilation_unit
        symbol_type_indirect_call_edge = HAKCFunction.relation_indirect_calls
        symbol_symbol_direct_call_edge = HAKCFunction.relation_direct_calls
        symbol_type_edge = HAKCSymbol.relation_type
        symbol_symbol_edge = HAKCSymbol.relation_symbol
        cu = HAKCCompilationUnit.get_table_name()
        _type = HAKCType.get_table_name()
        scope = HAKCScope.get_table_name()
        symbol_scope_edge = HAKCSymbol.relation_scope

        type_attrs = HAKCDatabase.get_object_attributes(HAKCType)
        scope_attrs = HAKCDatabase.get_object_attributes(HAKCScope)
        symbol_attrs = HAKCDatabase.get_object_attributes(HAKCSymbol)
        cu_attrs = HAKCDatabase.get_object_attributes(HAKCCompilationUnit)

        cmd = [f"""
        MATCH ({scope})<-[:{symbol_scope_edge}]-({symbol})-[:{symbol_type_edge}]->({_type})
        """]
        if assume_defined:
            cmd.append(f", MATCH ({symbol})-[{symbol_cu_edge}]->({cu})")
        if where_clause is not None:
            cmd.append(where_clause)

        cmd.append("RETURN")
        return_str = f"""
        {symbol_attrs}, {scope_attrs}, {type_attrs}
        """
        cmd.append(f'{return_str}')
        if assume_defined:
            cmd.append(f", {cu_attrs}, {symbol_cu_edge}.DefiningLine")
        cmd.append(f"""
            ORDER BY {symbol}.Name, {_type}.DebugType, {scope}.Scope, {scope}.LocalScopeName
        """)
        if limit > 0:
            cmd.append(f'LIMIT {limit}')
        symbols = list()
        response = self.execute_prepared_stmt(prepared_stmt=" ".join(cmd))
        if response.has_next():
            info = response.get_as_df()
            for data in info.to_dict(orient='records'):
                symbol = self.__create_object_from_response(HAKCFunction, **data)
                symbols.append(symbol)

        return symbols


    def get_indirect_calls(self, _symbol: HAKCSymbol) -> set[HAKCFunction]:
        _type = HAKCType.get_table_name()
        symbol = HAKCSymbol.get_table_name()
        symbol_hash = HAKCSymbol.get_primary_key()
        scope = HAKCScope.get_table_name()
        compilation_unit = HAKCCompilationUnit.get_table_name()
        _type = HAKCType.get_table_name()
        symbol_scope_edge = HAKCSymbol.relation_scope
        symbol_compilation_unit_edge = HAKCSymbol.relation_compilation_unit
        function_indirect_calls = HAKCFunction.relation_indirect_calls

        type_attrs = HAKCDatabase.get_object_attributes(HAKCType)
        scope_attrs = HAKCDatabase.get_object_attributes(HAKCScope)
        symbol_attrs = HAKCDatabase.get_object_attributes(HAKCSymbol)
        cu_attrs = HAKCDatabase.get_object_attributes(HAKCCompilationUnit)

        cmd = f"""
        MATCH (_:{symbol})-[:{function_indirect_calls}]->({_type}), 
        ({symbol})-[{symbol_scope_edge}]->({scope})
        WHERE _.{symbol_hash} = $symbol_hash AND _.{symbol_hash} <> {symbol}.{symbol_hash} AND  
                {_type}.{HAKCType.get_primary_key()} IS NOT NULL AND {symbol}.{symbol_hash} IS NOT NULL AND {scope}.{HAKCScope.get_primary_key()} IS NOT NULL
        OPTIONAL MATCH ({symbol})-[{symbol_compilation_unit_edge}]-({compilation_unit})
        RETURN DISTINCT {type_attrs}, {scope_attrs}, {symbol_attrs}, {cu_attrs};
        """
        try:
            response = self.execute_prepared_stmt(cmd, symbol_hash=int(_symbol.get_computed_hash().final_hash))
            indirect_calls = set()
            if response.has_next():
                info = response.get_as_df()
                # print(info)
                for data in info.to_dict(orient='records'):
                    ty = HAKCDatabase.__create_object_from_response(HAKCType, **data)
                    indirect_calls.add(ty)
        except Exception as e:
            logger.error(f'get_indirect_calls failed')
            raise e
        return indirect_calls


    def get_direct_calls(self, _symbol: HAKCSymbol) -> set[HAKCFunction]:
        # TODO: should compilation unit be added to the direct call output in dag.yaml?
        symbol_symbol_direct_call_edge = HAKCFunction.relation_direct_calls
        _type = HAKCType.get_table_name()
        symbol = HAKCSymbol.get_table_name()
        symbol_hash = HAKCSymbol.get_primary_key()
        scope = HAKCScope.get_table_name()
        compilation_unit = HAKCCompilationUnit.get_table_name()
        _type = HAKCType.get_table_name()
        symbol_type_edge = HAKCSymbol.relation_type
        symbol_scope_edge = HAKCSymbol.relation_scope
        symbol_compilation_unit_edge = HAKCSymbol.relation_compilation_unit

        type_attrs = HAKCDatabase.get_object_attributes(HAKCType)
        scope_attrs = HAKCDatabase.get_object_attributes(HAKCScope)
        symbol_attrs = HAKCDatabase.get_object_attributes(HAKCSymbol)
        cu_attrs = HAKCDatabase.get_object_attributes(HAKCCompilationUnit)

        cmd = f"""
        MATCH (_:{symbol})-[:{symbol_symbol_direct_call_edge}]->({symbol})-[:{symbol_type_edge}]->({_type}), 
        ({symbol})-[{symbol_scope_edge}]->({scope})
        WHERE _.{symbol_hash} = $symbol_hash AND _.{symbol_hash} <> {symbol}.{symbol_hash} AND {symbol}.IsFunction AND  
                {_type}.{HAKCType.get_primary_key()} IS NOT NULL AND {symbol}.{symbol_hash} IS NOT NULL AND {scope}.{HAKCScope.get_primary_key()} IS NOT NULL
        OPTIONAL MATCH ({symbol})-[{symbol_compilation_unit_edge}]-({compilation_unit})
        RETURN DISTINCT {type_attrs}, {scope_attrs}, {symbol_attrs}, {cu_attrs};
        """
        try:
            response = self.execute_prepared_stmt(cmd, symbol_hash=int(_symbol.get_computed_hash().final_hash))
            direct_calls = set()
            if response.has_next():
                info = response.get_as_df()
                # print(info)
                for data in info.to_dict(orient='records'):
                    func = HAKCDatabase.__create_object_from_response(HAKCFunction, **data)
                    direct_calls.add(func)
        except Exception as e:
            logger.error(f'get_direct_calls failed')
            raise e
        return direct_calls


    def get_used_symbols(self, _symbol: HAKCSymbol) -> set[HAKCSymbol]:
        # NOTE: this seems to not properly return HAKCFunction with defining line and file information
        # seems to be duplicate defining line showing up as DefiningLine, and head.DefiningLine, which is causing issues...
        # so, I guess, don't try to manually rename things in the return statement or it might not be processed correctly
        symbol_symbol_edge = HAKCSymbol.relation_symbol
        _type = HAKCType.get_table_name()
        symbol = HAKCSymbol.get_table_name()
        symbol_hash = HAKCSymbol.get_primary_key()
        scope = HAKCScope.get_table_name()
        compilation_unit = HAKCCompilationUnit.get_table_name()
        _type = HAKCType.get_table_name()
        symbol_type_edge = HAKCSymbol.relation_type
        symbol_scope_edge = HAKCSymbol.relation_scope
        symbol_compilation_unit_edge = HAKCSymbol.relation_compilation_unit

        type_attrs = HAKCDatabase.get_object_attributes(HAKCType)
        scope_attrs = HAKCDatabase.get_object_attributes(HAKCScope)
        symbol_attrs = HAKCDatabase.get_object_attributes(HAKCSymbol)
        cu_attrs = HAKCDatabase.get_object_attributes(HAKCCompilationUnit)

        cmd = f"""
        MATCH (_:{symbol})-[:{symbol_symbol_edge}]->({symbol})-[:{symbol_type_edge}]->({_type}), 
        ({symbol})-[{symbol_scope_edge}]->({scope})
        WHERE _.{symbol_hash} = $symbol_hash AND _.{symbol_hash} <> {symbol}.{symbol_hash} AND {symbol}.IsFunction AND  
                {_type}.{HAKCType.get_primary_key()} IS NOT NULL AND {symbol}.{symbol_hash} IS NOT NULL AND {scope}.{HAKCScope.get_primary_key()} IS NOT NULL
        OPTIONAL MATCH ({symbol})-[{symbol_compilation_unit_edge}]->({compilation_unit})
        RETURN DISTINCT {type_attrs}, {scope_attrs}, {symbol_attrs}, {cu_attrs};
        """
        try:
            response = self.execute_prepared_stmt(cmd, symbol_hash=int(_symbol.get_computed_hash().final_hash))
            used_symbols = set()
            if response.has_next():
                info = response.get_as_df()
                # print(info)
                for data in info.to_dict(orient='records'):
                    func = HAKCDatabase.__create_object_from_response(HAKCFunction, **data)
                    used_symbols.add(func)
        except Exception as e:
            logger.error(f'get_used_symbols_calls failed')
            raise e
        return used_symbols

    def get_types_used(self, _symbol: HAKCSymbol) -> set[HAKCTypePerm]:
        # NOTE: this seems to not properly return HAKCFunction with defining line and file information
        # seems to be duplicate defining line showing up as DefiningLine, and head.DefiningLine, which is causing issues...
        # so, I guess, don't try to manually rename things in the return statement or it might not be processed correctly
        symbol = HAKCSymbol.get_table_name()
        symbol_hash = HAKCSymbol.get_primary_key()
        _type = HAKCType.get_table_name()
        function_types_used = HAKCFunction.relation_types_used

        type_attrs = HAKCDatabase.get_object_attributes(HAKCType)

        cmd = f"""
        MATCH (_:{symbol})-[RWX:{function_types_used}]->({_type}) 
        WHERE _.{symbol_hash} = $symbol_hash AND {_type}.{HAKCType.get_primary_key()} IS NOT NULL  
        RETURN DISTINCT {type_attrs}, RWX.*;
        """
        try:
            response = self.execute_prepared_stmt(cmd, symbol_hash=int(_symbol.get_computed_hash().final_hash))
            types_used = set()
            if response.has_next():
                info = response.get_as_df()
                # print(info)
                for data in info.to_dict(orient='records'):
                    # print(data)
                    ty = HAKCDatabase.__create_object_from_response(HAKCType, **data)
                    ty_perm = HAKCTypePerm(Type=ty, RWX = (data["RWX.R"] << 2) + (data["RWX.W"] << 1) + data["RWX.X"])
                    types_used.add(ty_perm)
        except Exception as e:
            logger.error(f'get_types_used failed')
            raise e
        return types_used


    def get_symbols(self, limit: int = 0):
        return self._get_symbols(limit=limit)

    @staticmethod
    def __get_class_db_columns(cls):
        return [x.column_name for x in cls.get_data_columns()] + [cls.get_primary_key().column_name]

    @staticmethod
    def __create_object_from_response(cls, **data):
        # print(cls)
        if cls == HAKCFunction:
            ty = HAKCDatabase.__create_object_from_response(HAKCType, **data)
            # print(ty)
            sc = HAKCDatabase.__create_object_from_response(HAKCScope, **data)
            # print(sc)
            data["HAKCSymbol.Scope"] = sc
            data["HAKCSymbol.Type"] = ty

            # print(data.keys())
            # if HAKCCompilationUnit.get_primary_key().column_name in data:
            if "HAKCCompilationUnit.DefiningLine" in data:
                # print(f"Found CompilationUnit")
                cu = HAKCDatabase.__create_object_from_response(HAKCCompilationUnit, **data)
                data["HAKCSymbol.CompilationUnit"] = cu
            cls_data = {key.removeprefix(f"{cls.get_table_name()}."): val for key, val in data.items()}
            return HAKCFunction(**cls_data)
        elif cls == HAKCDivision:
            compartment = HAKCDatabase.__create_object_from_response(HAKCCompartment, **data)
            data["HAKCDivision.CompartmentID"] = data["HAKCCompartment.CompartmentID"]
            div_data = {key.removeprefix(f"{cls.get_table_name()}."): val for key, val in data.items() if key.startswith(cls.get_table_name())}
            return HAKCDivision(**div_data), compartment
        elif cls == HAKCCompartment:
            cls_data = {key.removeprefix(f"{cls.get_table_name()}."): val for key, val in data.items() if key.startswith(cls.get_table_name())}
            return HAKCCompartment(**cls_data)

        cls_data = {key.removeprefix(f"{cls.get_table_name()}."): val for key, val in data.items() if key.startswith(cls.get_table_name())}
        if len(cls_data) == 0:
            raise RuntimeError('No type data provided')
        return cls(**cls_data)


    def get_dag_edges(self, _symbol: HAKCSymbol) -> set[(HAKCSymbol, int)]:
        # want to reconstruct the output from the yaml exactly, so the compartmentalization can be rebuilt from the database
        # Note: Only need to get enough data to match the node in the networkx graph

        symbol = HAKCSymbol.get_table_name()
        symbol_hash = HAKCSymbol.get_primary_key()
        scope = HAKCScope.get_table_name()
        compilation_unit = HAKCCompilationUnit.get_table_name()
        division = HAKCDivision.get_table_name()
        _type = HAKCType.get_table_name()
        symbol_type_edge = HAKCSymbol.relation_type
        symbol_scope_edge = HAKCSymbol.relation_scope
        symbol_compilation_unit_edge = HAKCSymbol.relation_compilation_unit
        symbol_symbol_dag_edge = HAKCSymbol.relation_dag

        type_attrs = HAKCDatabase.get_object_attributes(HAKCType)
        scope_attrs = HAKCDatabase.get_object_attributes(HAKCScope)
        symbol_attrs = HAKCDatabase.get_object_attributes(HAKCSymbol)
        cu_attrs = HAKCDatabase.get_object_attributes(HAKCCompilationUnit)

        cmd = f"""
        MATCH (_:{symbol})<-[{symbol_symbol_dag_edge}]-({symbol})-[:{symbol_type_edge}]->({_type}),
              ({symbol})-[{symbol_scope_edge}]->({scope})
        WHERE _.{symbol_hash} = $symbol_hash AND {_type}.{HAKCType.get_primary_key()} IS NOT NULL 
                AND {symbol}.{symbol_hash} IS NOT NULL AND {scope}.{HAKCScope.get_primary_key()} IS NOT NULL
                AND {symbol_symbol_dag_edge}.weight IS NOT NULL 
        OPTIONAL MATCH ({symbol})-[{symbol_compilation_unit_edge}]-({compilation_unit})
        RETURN DISTINCT {type_attrs}, {scope_attrs}, {symbol_attrs}, {cu_attrs}, {symbol_symbol_dag_edge}.weight;
        """
        # print(cmd)
        response = self.execute_prepared_stmt(cmd, symbol_hash=int(_symbol.get_computed_hash().final_hash))
        dag_edges = set()

        if response.has_next():
            # Note: the uint64 hashes seem to be cast to floats if a nan is present
            info = response.get_as_df()
            # print(info)
            for data in info.to_dict(orient='records'):
                if data["HAKCSymbol.IsFunction"] is True:
                    func = HAKCDatabase.__create_object_from_response(HAKCFunction, **data)
                    dag_edge = (func, data[f"{symbol_symbol_dag_edge}.weight"])
                    dag_edges.add(dag_edge)
                    # print(f"Found DAG: {dag_edge}")
                else:
                    gv = HAKCDatabase.__create_object_from_response(HAKCGlobalVariable, **data)
                    dag_edge = (gv, data[f"{symbol_symbol_dag_edge}.weight"])
                    dag_edges.add(dag_edge)
                    # print(f"Found DAG: {dag_edge}")
        return dag_edges


    def get_division_compartment(self, _symbol: HAKCSymbol) -> tuple[HAKCDivision, HAKCCompartment]:
        # TODO: How to get unique compartment_id from the division
        # want to reconstruct the output from the yaml exactly, so the compartmentalization can be rebuilt from the database
        # Note: Only need to get enough data to match the node in the networkx graph
        symbol = HAKCSymbol.get_table_name()
        symbol_hash = HAKCSymbol.get_primary_key()
        division = HAKCDivision.get_table_name()
        symbol_division_edge = HAKCSymbol.relation_division
        _type = HAKCType.get_table_name()
        division_compartment_edge = HAKCDivision.relation_compartment
        compartment = HAKCCompartment.get_table_name()
        scope = HAKCScope.get_table_name()
        symbol_scope_edge = HAKCSymbol.relation_scope
        scope_hash = HAKCScope.get_primary_key()

        symbol_attrs = HAKCDatabase.get_object_attributes(HAKCSymbol)
        division_attrs = HAKCDatabase.get_object_attributes(HAKCDivision)
        compartment_attrs = HAKCDatabase.get_object_attributes(HAKCCompartment)
        scope_attrs = HAKCDatabase.get_object_attributes(HAKCScope)
        cmd = f"""
        MATCH ({scope})<-[:{symbol_scope_edge}]-({symbol})-[:{symbol_division_edge}]->({division})-[:{division_compartment_edge}]->({compartment})
        WHERE {symbol}.{symbol_hash} = $symbol_hash AND {division}.division_hash IS NOT NULL AND {compartment}.compartment_hash IS NOT NULL AND {scope}.{scope_hash} IS NOT NULL 
        RETURN DISTINCT {scope_attrs}, {symbol_attrs}, {division_attrs}, {compartment_attrs};
        """
        # print(cmd)
        response = self.execute_prepared_stmt(cmd, symbol_hash=int(_symbol.get_computed_hash().final_hash))
        div, comp = None, None
        if response.has_next():
            # Note: the uint64 hashes seem to be cast to floats if a nan is present
            info = response.get_as_df()
            # print(info)
            assert len(info) == 1, print(info)
            for data in info.to_dict(orient='records'):
                # print(f"Division and Compartment data: {data}")
                div, comp = HAKCDatabase.__create_object_from_response(HAKCDivision, **data)
                # print(div)
                # print(comp)
        assert(div and comp)
        return div, comp

    @staticmethod
    def get_object_attributes(cls):
        assert(cls in {HAKCType, HAKCScope, HAKCSymbol, HAKCFunction, HAKCCompilationUnit, HAKCDivision, HAKCCompartment})
        return ", ".join([f"{cls.get_table_name()}.{x.column_name}" for x in cls.get_data_columns()] + [f"{cls.get_table_name()}.{cls.get_primary_key()}"])

    # fix to determinism is using semicolon on edge for kuzu query
    # otherwise, I guess the edge is just labeled but not actually matched
    def get_functions(self) -> set:
        # want to reconstruct the output from the yaml exactly, so the compartmentalization can be rebuilt from the database
        # this [:edge*1..2] which recursively searches is probably not needed
        symbol = HAKCSymbol.get_table_name()
        symbol_hash = HAKCSymbol.get_primary_key()
        scope = HAKCScope.get_table_name()
        compilation_unit = HAKCCompilationUnit.get_table_name()
        cu_hash = HAKCCompilationUnit.get_primary_key()
        _type = HAKCType.get_table_name()
        symbol_type_edge = HAKCSymbol.relation_type
        symbol_scope_edge = HAKCSymbol.relation_scope
        symbol_compilation_unit_edge = HAKCSymbol.relation_compilation_unit

        type_attrs = HAKCDatabase.get_object_attributes(HAKCType)
        scope_attrs = HAKCDatabase.get_object_attributes(HAKCScope)
        symbol_attrs = HAKCDatabase.get_object_attributes(HAKCSymbol)
        cu_attrs = HAKCDatabase.get_object_attributes(HAKCCompilationUnit)
        # probably can remove any null checks from primary keys
        cmd = f"""
        MATCH ({_type})<-[:{symbol_type_edge}]-({symbol})-[:{symbol_scope_edge}]->({scope})
        WHERE {_type}.{HAKCType.get_primary_key()} IS NOT NULL AND {symbol}.{symbol_hash} IS NOT NULL AND {scope}.{HAKCScope.get_primary_key()} IS NOT NULL
        OPTIONAL MATCH ({symbol})-[{symbol_compilation_unit_edge}]->({compilation_unit})
        WHERE {symbol_compilation_unit_edge}.DefiningLine IS NOT NULL
        RETURN DISTINCT {type_attrs}, {scope_attrs}, {symbol_attrs}, {cu_attrs}, {symbol_compilation_unit_edge}.DefiningLine;
        """
        # print(cmd)
        response = self.execute_prepared_stmt(cmd)
        functions = set()
        gvs = set()
        if response.has_next():
            # Note: the uint64 hashes seem to be cast to floats if a nan is present
            info = response.get_as_df()
            # print(len(info))
            for data in info.to_dict(orient='records'):
                # print(data)
                # only NaN is not equal to itself
                if ((data[f"{compilation_unit}.DefiningFile"] == data[f"{compilation_unit}.DefiningFile"]) and
                        (data[f"{compilation_unit}.{cu_hash}"] == data[f"{compilation_unit}.{cu_hash}"]) and
                        (data[f"{symbol_compilation_unit_edge}.DefiningLine"] == data[f"{symbol_compilation_unit_edge}.DefiningLine"])):
                    # print(f"Found compilation_unit! {data}")
                    # print(data[f"{symbol_compilation_unit_edge}.DefiningLine"])
                    # with this query, the compilation_unit will sometimes be nan, which is a float, so I have to cast the found value back to int
                    # print(data)
                    data[f"{compilation_unit}.DefiningLine"] = int(data[f"{symbol_compilation_unit_edge}.DefiningLine"])
                    data[f"{compilation_unit}.{cu_hash}"] = int(data[f"{compilation_unit}.{cu_hash}"])
                if data["HAKCSymbol.IsFunction"] is True:
                    func = HAKCDatabase.__create_object_from_response(HAKCFunction, **data)
                    functions.add(func)
                    # print(func.debug_print())
                else:
                    assert(False)
                    gv = HAKCDatabase.__create_object_from_response(HAKCGlobalVariable, **data)
                    # print(gv)
                    gvs.add(gv)

        # the 'base' HAKCSymbol is now created, now look for all symbols used, direct calls, indirect calls, types used
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

            types_used = self.get_types_used(func)
            # print(f"types_used: {len(types_used)}")
            for type_used in types_used:
                func.types_used.append(type_used)
        # print(f"Returning {len(functions)} functions")
        return functions


    def get_functions_to_merge(self) -> set:
        # find two functions that are a child of a function (direct call), and both have the same RWX to the same type
        #   and then join?
        # e.g.,
        # foo->bar-110->int, foo->baz-100->int
        function_types_used = HAKCFunction.relation_types_used
        symbol_symbol_direct_call_edge = HAKCFunction.relation_direct_calls
        symbol = HAKCSymbol.get_table_name()
        symbol_hash = HAKCSymbol.get_primary_key()
        _type = HAKCType.get_table_name()

        cmd = f"""
        MATCH (parent:{symbol})-[dir0:{symbol_symbol_direct_call_edge}]->(child0:{symbol})-[RWX0:{function_types_used}]->(ty0:{_type}),
              (parent:{symbol})-[dir1:{symbol_symbol_direct_call_edge}]->(child1:{symbol})-[RWX1:{function_types_used}]->(ty1:{_type}),
              (child0)-[:{symbol_symbol_direct_call_edge}]->(post:{symbol})<-[:{symbol_symbol_direct_call_edge}]-(child1)
        WHERE child0.name <> child1.name AND RWX0.R=RWX1.R AND RWX0.W=RWX1.W AND RWX0.X=RWX1.X AND ty0.LLVMType=ty1.LLVMType
        RETURN DISTINCT child0.*, child1.*;
        """
        # print(cmd)
        response = self.execute_prepared_stmt(cmd)
        merge_functions = set()
        if response.has_next():
            # Note: the uint64 hashes seem to be cast to floats if a nan is present
            info = response.get_as_df()
            # print(info)
            for data in info.to_dict(orient='records'):
                # print(data)
                if data["child0.IsFunction"] is True:
                    child0 = data['child0.symbol_hash']
                    child1 = data['child1.symbol_hash']
                    # print(f"Child0 and Child1: ({child0},{child1})")
                    merge_functions.add((child0, child1))
        return merge_functions

    def create_node_table(self, node_type: Type[HAKCDBNode]):
        create_cmd = f'CREATE NODE TABLE IF NOT EXISTS {node_type.get_table_definition()}'
        # print(create_cmd)
        self.execute_prepared_stmt(create_cmd)

    def create_relationship_table(self, edge_type: HAKCDBRelation):
        create_cmd = f'CREATE REL TABLE IF NOT EXISTS {edge_type.get_definition()}'
        # print(create_cmd)
        self.execute_prepared_stmt(create_cmd)

    def get_symbol_hash(self, Name, DefiningFile, DefiningLine):
        symbol = HAKCSymbol.get_table_name()
        symbol_hash = HAKCSymbol.get_primary_key()
        symbol_cu_edge = HAKCSymbol.relation_compilation_unit
        cu = HAKCCompilationUnit.get_table_name()
        _type = HAKCType.get_table_name()
        cmd = f"""
            MATCH ({symbol})-[{symbol_cu_edge}]->({cu})
            WHERE {symbol}.Name=$Name AND {cu}.DefiningFile=$DefiningFile AND {cu}.DefiningLine=$DefiningLine
            RETURN DISTINCT {symbol}.{symbol_hash}
        """
        response = self.execute_prepared_stmt(cmd, Name=Name, DefiningFile=DefiningFile, DefiningLine=DefiningLine)
        ret = response.get_as_df()
        if(f"{symbol}.{symbol_hash}" in ret) and (len(ret[f"{symbol}.{symbol_hash}"]) > 0):
            hash = ret[f"{symbol}.{symbol_hash}"][0]
            # print(type(hash))
            print(f"Queried symbol hash: {hash} from (Name, DefiningFile, DefiningLine) = ({Name}, {DefiningFile}, {DefiningLine})")
            return int(hash)
        else:
            print(f"Queried symbol hash from (Name, DefiningFile, DefiningLine) = ({Name}, {DefiningFile}, {DefiningLine}), but could not find symbol hash!")
            return None
