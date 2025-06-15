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
        # symbol = HAKCSymbol.node_key
        symbol = HAKCSymbol.get_table_name()
        symbol_hash = str(HAKCSymbol.get_primary_key())
        cmd = f"""
        MATCH ({symbol})
        RETURN {symbol}.{symbol_hash};
        """
        response = self.execute_prepared_stmt(cmd)
        ret = response.get_as_df()[f"{symbol}.{symbol_hash}"].to_list()
        return ret

    def get_symbol_by_hash(self, symbol_hashes: list[int], **kwargs) -> list[HAKCSymbol]:
        result = self._get_symbols(
            where_clause=f'WHERE sym.symbol_hash in [{", ".join([str(sh) for sh in symbol_hashes])}]', **kwargs)
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
        # symbol_hash = HAKCSymbol.get_primary_key().column_name
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
        cmd = [f"""
        MATCH ({scope})<-[:{symbol_scope_edge}]-({symbol})-[:{symbol_type_edge}]->({_type})
        """]
        if assume_defined:
            cmd.append(f", MATCH ({symbol})-[{symbol_cu_edge}]->({cu})")
        if where_clause is not None:
            cmd.append(where_clause)

        cmd.append("RETURN")
        return_str = f"""
        {symbol}.*, {symbol}.IsFunction, {scope}.Scope, {scope}.LocalScopeName, {_type}.DebugType, {_type}.LLVMType
        """
        cmd.append(f'{return_str}')
        if assume_defined:
            cmd.append(f", {cu}.DefiningLine, {cu}.DefiningFile")
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

                symbol = self._create_symbol_from_response(**data)
                symbols.append(symbol)

        return symbols

    def _create_type_perm_from_response(self, perm_prefix: str = f"{HAKCFunction.relation_types_used}.", **kwargs) -> HAKCTypePerm:
        _type = self._create_type_from_response(**kwargs)
        # print(f"Reconstructed type {_type} from **kwargs {kwargs}")
        perm_data = {key.removeprefix(perm_prefix): val for key, val in kwargs.items() if key.startswith(perm_prefix)}
        if len(perm_data) == 0:
            raise RuntimeError('No type data provided')
        type_perm = HAKCTypePerm(Type=_type, **perm_data)
        return type_perm

    def _create_type_from_response(self, type_prefix: str = f"{HAKCType.get_table_name()}.", **kwargs) -> HAKCType:
        type_data = {key.removeprefix(type_prefix): val for key, val in kwargs.items()}
        if len(type_data) == 0:
            raise RuntimeError('No type data provided')
        ty = HAKCType(**type_data)
        return ty

    def _create_symbol_from_response(self, is_function: bool, type_prefix: str = f"{HAKCType.get_table_name()}.", scope_prefix: str = f"{HAKCScope.get_table_name()}.",
                                     symbol_prefix: str = f"{HAKCSymbol.get_table_name()}.", **kwargs) -> HAKCSymbol:
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
        cmd = f"""
            MATCH ({symbol})-[:{symbol_type_indirect_call_edge}]->({_type})
            WHERE {symbol}.{symbol_hash} = $symbol_hash
            RETURN {_type}.DebugType, {_type}.LLVMType
            ORDER BY {_type}.DebugType, {_type}.LLVMType;
        """
        try:
            response = self.execute_prepared_stmt(cmd, symbol_hash=hash(symbol))
            types = []
            if response.has_next():
                info = response.get_as_df()
                for data in info.to_dict(orient='records'):
                    ty = self._create_type_from_response(**data)
                    types.append(ty)
        except Exception as e:
            logger.error(f'get_indirect_calls failed')
            raise e
        return types

    def get_direct_calls(self, symbol: HAKCSymbol) -> list[HAKCType]:
        # NOTE: this seems to not properly return HAKCFunction with defining line and file information
        # seems to be duplicate defining line showing up as DefiningLine, and head.DefiningLine, which is causing issues...
        # so, I guess, don't try to manually rename things in the return statement or it might not be processed correctly
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
        cmd = f"""
                MATCH (_:{symbol})-[:{symbol_symbol_direct_call_edge}]->({symbol})-[:{symbol_type_edge}]->({_type})
                WHERE _.{symbol_hash} = $symbol_hash and _.{symbol_hash} <> {symbol}.{symbol_hash}
                RETURN DISTINCT _.*, {symbol}.*, {_type}.*;
            """
                # OPTIONAL MATCH (head)-[def:{HAKCSymbol.DefinedInTable}]->(cu:{HAKCCompilationUnit.get_table_name()})
                # RETURN DISTINCT head.*, tail.*, ty.*, def.line AS DefiningLine, cu.filename AS DefiningFile;
        try:
            response = self.execute_prepared_stmt(cmd, symbol_hash=hash(symbol))
            direct_calls = []
            if response.has_next():
                info = response.get_as_df()
                print(info)
                # for data in info.to_dict(orient='records'):
                for index, row in info.iterrows():
                    entry = row.to_dict()
                    # print(entry)
                    # TODO put this check in the kuzu query?
                    # TODO: also rely on the head not being equal to the tail?
                    if entry["head.is_function"] and entry["tail.is_function"]:
                        call = self._create_symbol_from_response(is_function=True, symbol_prefix='tail.', **entry)
                        direct_calls.append(call)
        except Exception as e:
            logger.error(f'get_direct_calls failed')
            raise e
        return direct_calls


    def get_used_symbols(self, symbol: HAKCSymbol):
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
        cmd = f"""
            MATCH (_:{symbol})-[:{symbol_symbol_edge}]->({symbol}),
            ({scope})<-[:{symbol_scope_edge}]-({symbol})-[:{symbol_type_edge}]->({_type})
            WHERE _.{symbol_hash}=$symbol_hash
            RETURN {symbol}.*, {scope}.*, {_type}.*;
        """
        try:
            response = self.execute_prepared_stmt(cmd, symbol_hash=hash(symbol))
            used_symbols = []
            if response.has_next():
                info = response.get_as_df()
                for data in info.to_dict(orient='records'):
                    symbol = self._create_symbol_from_response(**data)
                    used_symbols.append(symbol)
        except Exception as e:
            logger.error(f'get_used_symbols failed')
            raise e

        return used_symbols


    def get_function_perm_types(self):
        symbol = HAKCSymbol.get_table_name()
        function = HAKCFunction.get_table_name()
        function_types_used_edge = HAKCFunction.relation_types_used
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
        # need to include definition and cu, even if its missing, because other queries use it and if its missing in one it will cause the nodes to not be equal (causing duplicates)
        cmd = f"""
            MATCH ({function})-[{function_types_used_edge}]->({_type}),
            RETURN DISTINCT {function}.*, {function_types_used_edge}.*, {_type}.*;
        """
        try:
            response = self.execute_prepared_stmt(cmd)
            function_perm_types = []
            if response.has_next():
                info = response.get_as_df()
                # print(info)
                for index, row in info.iterrows():
                    entry = row.to_dict()
                    # print(entry)
                    if entry["head.is_function"]:
                        func = self._create_symbol_from_response(is_function=True,  **entry)
                        type_perm = self._create_type_perm_from_response(**entry)
                        # print(f"Function {func} [- {type_perm} -> {type_perm.perm_type}")
                        function_perm_types.append((func, type_perm))

        except Exception as e:
            logger.error(f'get_function_to_types')
            raise e

        return function_perm_types


    def get_symbols(self, limit: int = 0):
        return self._get_symbols(limit=limit)

    # def get_all_types_used(self) -> list:
    #     # this [:edge*1..2] which reucrsively searches is probably not needed
    #     cmd = f"""
    #     MATCH p = (head:HAKCSymbol)-[:TypesUsed*1..2]->(tail:HAKCType)
    #     RETURN DISTINCT nodes(p), rels(p)
    #     """
    #     response = self.execute_prepared_stmt(cmd)
    #     return response.get_as_df()

    def create_node_table(self, node_type: Type[HAKCDBNode]):
        create_cmd = f'CREATE NODE TABLE IF NOT EXISTS {node_type.get_table_definition()}'
        self.execute_prepared_stmt(create_cmd)

    def create_relationship_table(self, edge_type: HAKCDBRelation):
        create_cmd = f'CREATE REL TABLE IF NOT EXISTS {edge_type.get_definition()}'
        self.execute_prepared_stmt(create_cmd)

    def get_symbol_hash(self, Name, DefiningFile, DefiningLine):
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
