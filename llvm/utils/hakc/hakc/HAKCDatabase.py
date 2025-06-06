import logging
import multiprocessing as mp
from typing import Type, Optional, Tuple

import pandas as pd

from .HAKCBase import HAKCDBNode, HAKCDBRelation
from .HAKCObjects import HAKCSymbol, HAKCFunction, HAKCScope, HAKCType, HAKCGlobalVariable, HAKCDivision, \
    HAKCCompartment, HAKCCompilationUnit

logger = logging.getLogger('hakc-dag')


class HAKCDatabase:
    def __init__(self, db_dir: str, read_only: bool = False, max_num_threads=int(mp.cpu_count() / 2),
                 batch_size: int = 1000):
        self.db_dir = db_dir
        self.database = None
        self.conn = None
        self.batch_size = batch_size
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

    def get_all_symbol_hashes_in_compartment(self, compartment_id: int) -> list[int]:
        cmd = f"""
        MATCH (comp1:{HAKCCompartment.get_table_name()})<-[:{HAKCDivision.InCompartmentTable}]-(div1:{HAKCDivision.get_table_name()})<-[:{HAKCSymbol.InDivisionTable}]-(sym1:{HAKCSymbol.get_table_name()})
        WHERE comp1.CompartmentID = $source_compartment_id
        return sym1.{str(HAKCSymbol.get_primary_key())} AS symbol_hash;
        """
        response = self.execute_prepared_stmt(cmd, source_compartment_id=compartment_id)
        ret = response.get_as_df()['symbol_hash'].to_list()
        return ret

    def get_all_divisions_in_compartment(self, compartment_ids: set[int]) -> dict[int, list[HAKCDivision]]:
        cmd = f"""
        MATCH (comp1:{HAKCCompartment.get_table_name()})<-[:{HAKCDivision.InCompartmentTable}]-(div1:{HAKCDivision.get_table_name()})
        WHERE comp1.CompartmentID IN [{','.join([str(i) for i in compartment_ids])}]
        RETURN div1.DivisionID as DivisionID, comp1.CompartmentID AS compartment_id, div1.{str(HAKCDivision.get_primary_key())} AS {str(HAKCDivision.get_primary_key())}
        """
        divisions = dict()
        response = self.execute_prepared_stmt(cmd)
        if response.has_next():
            info = response.get_as_df()
            for data in info.to_dict(orient='records'):
                division = HAKCDivision(**data)
                if division.compartment_id not in divisions:
                    divisions[division.compartment_id] = list()
                divisions[division.compartment_id].append(division)
        return divisions

    def merge_compartments(self, compartments_to_merge: dict[int, int]):
        target_compartments = set(compartments_to_merge.values())
        compartments_to_remove = set(compartments_to_merge.keys())
        existing_divisions = self.get_all_divisions_in_compartment(target_compartments)

        cmd = f"""
            MATCH (sym:{HAKCSymbol.get_table_name()})-[e:{HAKCSymbol.InDivisionTable}]->(div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.InCompartmentTable}]->(c:{HAKCCompartment.get_table_name()})
            WHERE c.{str(HAKCCompartment.get_primary_key())} IN [{','.join([str(i) for i in compartments_to_remove])}]
            DELETE e
            RETURN sym.{str(HAKCSymbol.get_primary_key())} AS SymbolHash, div.DivisionID AS DivisionID, c.{str(HAKCCompartment.get_primary_key())} AS CompartmentID;
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
        self.insert_from_dataframe(HAKCSymbol.InDivisionTable, df)

        cmd = f"""
            MATCH (div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.InCompartmentTable}]->(c:{HAKCCompartment.get_table_name()})
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

    def get_division_id_compartment_id_from_symbol(self, symbol: HAKCSymbol) -> Optional[
        Tuple[HAKCDivision, HAKCCompartment]]:
        cmd = f"""        
        MATCH (scope:{HAKCScope.get_table_name()})<-[:{HAKCSymbol.HasScopeTable}]-(sym:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.InDivisionTable}]->(div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.InCompartmentTable}]->(comp:{HAKCCompartment.get_table_name()})
        WITH sym.Name as Name, scope.Scope as Scope, div.DivisionID as DivisionID, div.AccessToken as AccessToken, comp.CompartmentID as CompartmentID, comp.EntryToken as EntryToken
        WHERE Name = $Name AND Scope = $Scope
        RETURN DivisionID, AccessToken, CompartmentID, EntryToken
        """
        response = self.execute_prepared_stmt(cmd, Name=symbol.name, Scope=symbol.scope.scope)
        # TODO: double check that this only returns one row
        ret = response.get_as_df()
        if ret.empty:
            logger.debug(f'Command: {cmd} returned None\n')
            logger.debug(f'Searched with Name: {symbol.name}, Scope: {str(symbol.scope)}')
            return None
        else:
            # TODO: check that this is correct when this is eventually called
            info = ret.to_dict(orient='records')
            logger.debug(f"Found {info} for symbol: {symbol}")
            return HAKCDivision(**info), HAKCCompartment(**info)

    def get_valid_targets_from_compartment_id(self, source_compartment_id: int) -> list[int]:
        cmd = f"""
        MATCH (comp1:{HAKCCompartment.get_table_name()})<-[:{HAKCDivision.InCompartmentTable}]-(div1:{HAKCDivision.get_table_name()})<-[:{HAKCSymbol.InDivisionTable}]-(sym1:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.DagEdgeTable}]->(sym2:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.InDivisionTable}]->(div2:{HAKCDivision.get_table_name()})-[:{HAKCDivision.InCompartmentTable}]->(comp2:{HAKCCompartment.get_table_name()})
        WHERE comp1.CompartmentID = $source_compartment_id
        RETURN DISTINCT comp2.CompartmentID AS Target;
        """
        response = self.execute_prepared_stmt(cmd, source_compartment_id=source_compartment_id)
        targets = response.get_as_df()['Target'].tolist()

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
        self.insert_from_dataframe(HAKCSymbol.DagEdgeTable, df)

    def get_all_symbol_hashes(self) -> list[int]:
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()})
        RETURN sym.{HAKCSymbol.get_primary_key().column_name} AS symbol_hash;
        """
        response = self.execute_prepared_stmt(cmd)
        ret = response.get_as_df()['symbol_hash'].to_list()
        return ret

    def get_symbol_by_hash(self, symbol_hashes: list[int]) -> list[HAKCSymbol]:
        result = self._get_symbols(
            where_clause=f'WHERE sym.symbol_hash in [{", ".join([str(sh) for sh in symbol_hashes])}]')
        return result

    def get_symbols_by_name(self, symbol_name: str) -> list[HAKCSymbol]:
        result = self._get_symbols(
            where_clause=f'WHERE sym.Name = {symbol_name}'
        )
        return result

    def delete_all_compartments(self):
        cmd = f"""
        MATCH (div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.InCompartmentTable}]->(c:{HAKCCompartment.get_table_name()})
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
        MATCH (div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.InCompartmentTable}]->(c:{HAKCCompartment.get_table_name()})
        RETURN div.DivisionID as DivisionID, c.CompartmentID AS compartment_id
        """
        divisions = set()
        response = self.execute_prepared_stmt(cmd)
        if response.has_next():
            info = response.get_as_df()
            for data in info.to_dict(orient='records'):
                divisions.add(HAKCDivision(**data))
        return divisions

    def get_all_compartments(self) -> list[HAKCCompartment]:
        cmd = f"""
        MATCH (c:{HAKCCompartment.get_table_name()})
        RETURN {", ".join(["c." + str(col) + " as " + str(col) for col in HAKCCompartment.get_db_table_columns()])}
        """
        compartments = list()
        response = self.execute_prepared_stmt(cmd)
        if response.has_next():
            info = response.get_as_df()
            for data in info.to_dict(orient='records'):
                compartment = HAKCCompartment(**data)
                compartments.append(compartment)
        return compartments

    def get_compartment_symbol_count(self) -> dict[int, int]:
        cmd = f"""
        MATCH
        (comp1:{HAKCCompartment.get_table_name()})
        RETURN comp1.{str(HAKCCompartment.get_primary_key())} AS CompartmentID,  COUNT {{ MATCH (comp1)<-[:{HAKCDivision.InCompartmentTable}]-(:{HAKCDivision.get_table_name()})<-[:{HAKCSymbol.InDivisionTable}]-(:{HAKCSymbol.get_table_name()}) }} AS Count
        """
        response = self.execute_prepared_stmt(cmd)
        result = dict()
        for _, row in response.get_as_df().iterrows():
            compartment_id = int(row['CompartmentID'].item())
            count = int(row['Count'].item())
            result[compartment_id] = count

        return result

    def get_symbol_definition_location(self, symbol: HAKCSymbol) -> tuple[HAKCCompilationUnit, int] | None:
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()})-[e:{HAKCSymbol.DefinedInTable}]->(cu:{HAKCCompilationUnit.get_table_name()})
        WHERE sym.{HAKCSymbol.get_primary_key().column_name} = $symbol_hash
        RETURN cu.filename as filename, e.line as line;
        """
        response = self.execute_prepared_stmt(cmd, symbol_hash=hash(symbol))
        if response.has_next():
            resp_dict = response.get_as_df().to_dict(orient='records')
            logger.info(f'resp_dict df: {resp_dict}')
            return HAKCCompilationUnit(filename=resp_dict['filename'][0]), resp_dict['line'][0]
        return None

    def get_dag_computation_edges(self, symbol_hash: int) -> dict[str, list[int]]:
        result = dict()
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()})-[:{HAKCFunction.IndirectCallTable}]->(:{HAKCType.get_table_name()})<-[:{HAKCSymbol.IsTypeTable}]-(indirect:{HAKCSymbol.get_table_name()})
        WHERE sym.{HAKCSymbol.get_primary_key().column_name} = $symbol_hash
        RETURN DISTINCT indirect.{HAKCSymbol.get_primary_key().column_name} AS {HAKCFunction.IndirectCallTable}
        """
        response = self.execute_prepared_stmt(cmd, symbol_hash=symbol_hash)
        df = response.get_as_pl()
        for table_name, entries in df.to_dict(as_series=False).items():
            if len(entries) > 0:
                result[table_name] = entries
        cmd = f"""
        MATCH (sym: {HAKCSymbol.get_table_name()})-[:{HAKCFunction.DirectCallTable}]->(direct:{HAKCSymbol.get_table_name()})
        WHERE sym.{HAKCSymbol.get_primary_key().column_name} = $symbol_hash
        RETURN DISTINCT direct.{HAKCSymbol.get_primary_key().column_name} AS {HAKCFunction.DirectCallTable}
        """
        response = self.execute_prepared_stmt(cmd, symbol_hash=symbol_hash)
        df = response.get_as_pl()
        for table_name, entries in df.to_dict(as_series=False).items():
            if len(entries) > 0:
                result[table_name] = entries
        cmd = f"""
        MATCH (sym: {HAKCSymbol.get_table_name()})-[:{HAKCSymbol.UsesSymbolTable}]->(uses:{HAKCSymbol.get_table_name()})
        WHERE sym.{HAKCSymbol.get_primary_key().column_name} = $symbol_hash
        RETURN DISTINCT uses.{HAKCSymbol.get_primary_key().column_name} AS {HAKCSymbol.UsesSymbolTable}
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

    def _get_symbols(self, where_clause: None | str = None, limit: int = 0) -> list[HAKCSymbol] | int:
        cmd = [f"""
        OPTIONAL MATCH (scope:{HAKCScope.get_table_name()})<-[:{HAKCSymbol.HasScopeTable}]-(sym:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.IsTypeTable}]->(ty:{HAKCType.get_table_name()}),
        (sym)-[def:{HAKCSymbol.DefinedInTable}]->(cu:{HAKCCompilationUnit.get_table_name()})
        """]
        if where_clause is not None:
            cmd.append(where_clause)

        cmd.append("RETURN")
        return_str = """
        sym.Name, sym.is_function AS is_function, scope.Scope, scope.LocalScopeName, ty.DebugType, ty.LLVMType, cu.filename AS DefiningFile, def.line AS DefiningLine
        """
        cmd.append(f'{return_str}')
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
            MATCH (head: {HAKCSymbol.get_table_name()})-[:{HAKCFunction.IndirectCallTable}]->(ty: {HAKCType.get_table_name()})
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

    def get_used_symbols(self, symbol: HAKCSymbol):
        cmd = f"""
            MATCH (head:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.UsesSymbolTable}]->(tail:{HAKCSymbol.get_table_name()}),
            (sc:{HAKCScope.get_table_name()})<-[:{HAKCSymbol.HasScopeTable}]-(tail)-[:{HAKCSymbol.IsTypeTable}]->(ty:{HAKCType.get_table_name()})
            WHERE head.symbol_hash=$symbol_hash
            RETURN tail.Name, tail.DefiningFile, tail.DefiningLine, tail.is_function AS is_function, sc.Scope,
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

    def get_symbols(self, limit: int = 0):
        return self._get_symbols(limit=limit)

    def create_node_table(self, node_type: Type[HAKCDBNode]):
        create_cmd = f'CREATE NODE TABLE IF NOT EXISTS {node_type.get_table_definition()}'
        self.execute_prepared_stmt(create_cmd)

    def create_relationship_table(self, edge_type: HAKCDBRelation):
        create_cmd = f'CREATE REL TABLE IF NOT EXISTS {edge_type.get_definition()}'
        self.execute_prepared_stmt(create_cmd)
