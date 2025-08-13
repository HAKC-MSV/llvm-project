import logging
import multiprocessing as mp
from typing import Type, Optional, Tuple

import pandas as pd

from .HAKCBase import HAKCDBNode, HAKCDBRelation
from .HAKCLogger import HAKCLogger
from .HAKCObjects import HAKCSymbol, HAKCFunction, HAKCScope, HAKCType, HAKCGlobalVariable, HAKCDivision, \
    HAKCCompartment, HAKCCompilationUnit, HAKCSymbolDefinitionLocation

logging.setLoggerClass(HAKCLogger)
logger = logging.getLogger('hakc-dag')


class HAKCDatabase:
    def __init__(self, db_dir: str, read_only: bool = False, max_num_threads=int(mp.cpu_count() / 2)):
        self.db_dir = db_dir
        self.database = None
        self.conn = None
        self.open(read_only=read_only, max_num_threads=max_num_threads)

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
            logger.debug(f'Command: {cmd} returned None')
            logger.debug(f'Searched with compartment_id: {compartment_id}')
            return None
        else:
            entry_token = ret["entry_token"][0]
            logger.debug(f"Found entry_token: {entry_token} for compartment_id: {compartment_id}")
            return int(entry_token)

    def get_division_access_token_from_id(self, division_id: int, compartment_id: int) -> Optional[int]:
        cmd = f"""
        MATCH (div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(comp:{HAKCCompartment.get_table_name()})
        WITH div.DivisionID as division_id, div.AccessToken as access_token, comp.CompartmentID AS compartment_id, comp.EntryToken AS entry_token
        WHERE division_id = $division_id AND compartment_id = $compartment_id 
        RETURN access_token, entry_token;
        """
        response = self.execute_prepared_stmt(cmd, division_id=division_id, compartment_id=compartment_id)
        ret = response.get_as_df()
        if ret.empty:
            logger.debug(f'Command: {cmd} returned None')
            logger.debug(f'Searched with division_id: {division_id}, compartment_id: {compartment_id}')
            return None
        else:
            access_token = ret["access_token"][0]
            logger.debug(f"Found access_token: {access_token} for division_id: {division_id}")
            return int(access_token)

    def get_division_id_compartment_id_from_symbol(self, symbol: HAKCSymbol) -> Optional[
        Tuple[HAKCDivision, HAKCCompartment]]:
        cmd = f"""        
        MATCH (scope:{HAKCScope.get_table_name()})<-[:{HAKCSymbol.relation_scope}]-(sym:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_division}]->(div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(comp:{HAKCCompartment.get_table_name()})
        WITH sym.Name as Name, scope.Scope as Scope, div.DivisionID as DivisionID, div.Salt as Salt, comp.CompartmentID as CompartmentID
        WHERE Name = $Name AND Scope = $Scope
        RETURN DivisionID, Salt, CompartmentID
        """
        response = self.execute_prepared_stmt(cmd, Name=symbol.name, Scope=symbol.scope.scope)
        # TODO: double check that this only returns one row
        ret = response.get_as_df()
        if ret.empty:
            logger.debug(f'Command: {cmd} returned None\n')
            logger.debug(f'Searched with Name: {symbol.name}, Scope: {str(symbol.scope)}')
            return None
        else:
            data = ret.to_dict(orient='records')
            division = HAKCDivision(**data)
            compartment = HAKCCompartment(**data)
            logger.debug(
                f"Found division_id, access_token, compartment_id, entry_token: ({division}, {compartment}) for symbol: {symbol}")
            return division, compartment

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
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()})
        RETURN sym.{str(HAKCSymbol.get_primary_key())} AS symbol_hash;
        """
        response = self.execute_prepared_stmt(cmd)
        ret = response.get_as_df()['symbol_hash'].to_list()
        return ret

    def get_symbol_by_hash(self, symbol_hashes: list[int], **kwargs) -> list[HAKCSymbol]:
        result = self._get_symbols(
            where_clause=f'WHERE sym.symbol_hash in [{", ".join([str(sh) for sh in symbol_hashes])}]', **kwargs)
        return result

    def get_symbol_hashes_to_symbols(self):
        symbol_hashes = self.get_all_symbol_hashes()
        symbols = self.get_symbol_by_hash(symbol_hashes, assume_defined=False)
        assert (len(symbol_hashes) == len(symbols))
        return dict(zip(symbol_hashes, symbols))

    def delete_all_compartments(self):
        cmd = f"""
        MATCH (div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(c:{HAKCCompartment.get_table_name()})
        DETACH DELETE div;
        """
        self.execute_prepared_stmt(cmd)
        cmd = f"""
        MATCH (c:{HAKCCompartment.get_table_name()})
        DETACH DELETE c;
        """
        self.execute_prepared_stmt(cmd)

    def get_all_divisions(self):
        cmd = f"""
        MATCH (div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(c:{HAKCCompartment.get_table_name()})
        RETURN div.DivisionID as DivisionID, c.CompartmentID AS compartment_id
        """
        divisions = set()
        response = self.execute_prepared_stmt(cmd)
        if response.has_next():
            info = response.get_as_df()
            for data in info.to_dict(orient='records'):
                divisions.add(HAKCDivision(**data))
        return divisions

    def get_symbol_definition_location(self, symbol: HAKCSymbol) -> HAKCSymbolDefinitionLocation | None:
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()})-[e:{HAKCSymbol.relation_compilation_unit}]->(cu:{HAKCCompilationUnit.get_table_name()})
        WHERE sym.{str(HAKCSymbol.get_primary_key())} = $symbol_hash
        RETURN cu.DefiningFile as DefiningFile, e.DefiningLine as DefiningLine;
        """
        response = self.execute_prepared_stmt(cmd, symbol_hash=hash(symbol))
        if response.has_next():
            resp_dict = response.get_as_df().to_dict(orient='records')
            cu = HAKCCompilationUnit(**resp_dict)
            return HAKCSymbolDefinitionLocation(DefiningFile=cu, DefiningLine=resp_dict['DefiningLine'])
        return None

    def get_dag_computation_edges(self, symbol_hash: int) -> dict[str, list[int]]:
        result = dict()
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()})-[:{HAKCFunction.relation_indirect_calls}]->(:{HAKCType.get_table_name()})<-[:{HAKCSymbol.relation_type}]-(indirect:{HAKCSymbol.get_table_name()})
        WHERE sym.{str(HAKCSymbol.get_primary_key())} = $symbol_hash
        RETURN DISTINCT indirect.{str(HAKCSymbol.get_primary_key())} AS {HAKCFunction.relation_direct_calls}
        """
        response = self.execute_prepared_stmt(cmd, symbol_hash=symbol_hash)
        df = response.get_as_pl()
        for table_name, entries in df.to_dict(as_series=False).items():
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

    def execute_prepared_stmt(self, prepared_stmt: str, **kwargs):
        response = self.conn.execute(prepared_stmt, parameters=kwargs)
        return response

    def insert_from_dataframe(self, table_name: str, df: pd.DataFrame):
        self.conn.execute(f'COPY {table_name} FROM df')

    def _get_symbols(self, where_clause: None | str = None, limit: int = 0, assume_defined: bool = True) -> list[
                                                                                                                HAKCSymbol] | int:
        # assumes all symobls are defined, but this may not be a valid assumption
        cmd = [f"""
        MATCH (scope:{HAKCScope.get_table_name()})<-[:{HAKCSymbol.relation_scope}]-(sym:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_type}]->(ty:{HAKCType.get_table_name()})
        """]
        if assume_defined:
            cmd.append(
                f",(sym)-[def:{HAKCSymbol.relation_compilation_unit}]->(cu:{HAKCCompilationUnit.get_table_name()})")
        if where_clause is not None:
            cmd.append(where_clause)

        cmd.append("RETURN")
        return_str = """
        sym.Name, sym.IsFunction AS is_function, scope.Scope, scope.LocalScopeName, ty.DebugType, ty.LLVMType
        """
        cmd.append(f'{return_str}')
        if assume_defined:
            cmd.append(", def.DefiningLine AS DefiningLine, cu.DefiningFile AS DefiningFile")
        cmd.append(f"""
            ORDER BY sym.Name, ty.DebugType, scope.Scope, scope.LocalScopeName
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
        #
        cmd = f"""
                MATCH (head: {HAKCSymbol.get_table_name()})-[:{HAKCFunction.relation_direct_calls}]->(tail: {HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_type}]->(ty:{HAKCType.get_table_name()})
                OPTIONAL MATCH (head)-[def:{HAKCSymbol.relation_compilation_unit}]->(cu:{HAKCCompilationUnit.get_table_name()})
                WHERE head.symbol_hash = $symbol_hash and head.symbol_hash <> tail.symbol_hash
                RETURN DISTINCT head.*, tail.*, ty.*, def.*, cu.*;
            """
        try:
            response = self.execute_prepared_stmt(cmd, symbol_hash=hash(symbol))
            direct_calls = []
            if response.has_next():
                info = response.get_as_df()
                for index, row in info.iterrows():
                    entry = row.to_dict()
                    # print(entry)
                    # TODO put this check in the kuzu query?
                    # TODO: also rely on the head not being equal to the tail?
                    if entry["head.IsFunction"] and entry["tail.IsFunction"]:
                        call = self._create_symbol_from_response(is_function=True, symbol_prefix='tail.', **entry)
                        direct_calls.append(call)
        except Exception as e:
            logger.error(f'get_direct_calls failed')
            raise e
        return direct_calls

    def get_used_symbols(self, symbol: HAKCSymbol):
        cmd = f"""
            MATCH (head:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_symbol}]->(tail:{HAKCSymbol.get_table_name()}),
            (sc:{HAKCScope.get_table_name()})<-[:{HAKCSymbol.relation_scope}]-(tail)-[:{HAKCSymbol.relation_type}]->(ty:{HAKCType.get_table_name()})
            WHERE head.symbol_hash=$symbol_hash
            RETURN tail.Name, tail.DefiningFile, tail.DefiningLine, tail.IsFunction AS is_function, sc.Scope,
            sc.LocalScopeName, ty.DebugType, ty.LLVMType;
        """
        try:
            response = self.execute_prepared_stmt(cmd, symbol_hash=hash(symbol))
            used_symbols = []
            if response.has_next():
                info = response.get_as_df()
                for data in info.to_dict(orient='records'):
                    symbol = self._create_symbol_from_response(symbol_prefix='tail.', scope_prefix='sc.',
                                                               type_prefix='ty.', **data)
                    used_symbols.append(symbol)
        except Exception as e:
            logger.error(f'get_used_symbols failed')
            raise e

        return used_symbols

    def get_function_perm_types(self):
        cmd = f"""
            MATCH (head:{HAKCSymbol.get_table_name()})-[rwx:{HAKCFunction.relation_types_used}]->(usety:{HAKCType.get_table_name()}),
            (head)-[:{HAKCSymbol.relation_type}]->(ty:{HAKCType.get_table_name()})
            RETURN DISTINCT head.*, rwx.*, ty.*, usety.*;
        """
        try:
            response = self.execute_prepared_stmt(cmd)
            function_perm_types = []
            if response.has_next():
                info = response.get_as_df()
                for index, row in info.iterrows():
                    entry = row.to_dict()
                    # print(entry)
                    if entry["head.IsFunction"]:
                        func = self._create_symbol_from_response(is_function=True, symbol_prefix='head.', **entry)
                        perm = self._create_perm_edge_from_response(perm_prefix="rwx.", **entry)
                        ty = self._create_type_from_response(type_prefix='usety.', **entry)
                        print(f"Function {func} [- {perm} -> {ty}")
                        function_perm_types.append((func, perm, ty))

        except Exception as e:
            logger.error(f'get_function_to_types')
            raise e

        return function_perm_types

    def get_symbols(self, limit: int = 0):
        return self._get_symbols(limit=limit)

    def get_all_types_used(self) -> list:
        # this [:edge*1..2] which reucrsively searches is probably not needed
        cmd = f"""
        MATCH p = (head:{HAKCSymbol.get_table_name()})-[:TypesUsed*1..2]->(tail:{HAKCType.get_table_name()})
        RETURN DISTINCT nodes(p), rels(p)
        """
        response = self.execute_prepared_stmt(cmd)
        return response.get_as_df()

    def create_node_table(self, node_type: Type[HAKCDBNode]):
        create_cmd = f'CREATE NODE TABLE IF NOT EXISTS {node_type.get_table_definition()}'
        self.execute_prepared_stmt(create_cmd)

    def create_relationship_table(self, edge_type: HAKCDBRelation):
        create_cmd = f'CREATE REL TABLE IF NOT EXISTS {edge_type.get_definition()}'
        self.execute_prepared_stmt(create_cmd)
