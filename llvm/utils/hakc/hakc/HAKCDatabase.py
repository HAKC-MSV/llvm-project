import logging
import multiprocessing as mp
from typing import Type, Optional

import pandas as pd

from .HAKCBase import HAKCDBNode, HAKCDBRelation
from .HAKCLogger import HAKCLogger
from .HAKCObjects import HAKCSymbol, HAKCFunction, HAKCScope, HAKCTypePerm, HAKCType, HAKCGlobalVariable, HAKCDivision, \
    HAKCCompartment, HAKCCompilationUnit

logging.setLoggerClass(HAKCLogger)
logger = logging.getLogger('hakc-dag')


class HAKCDatabase:
    def __init__(self, db_dir: str, read_only: bool = False, max_num_threads=int(mp.cpu_count() / 2),
                 batch_size: int = 1000):
        self.db_dir = db_dir
        self.database = None
        self.conn = None
        self.batch_size = batch_size
        self.open(read_only=read_only, max_num_threads=max_num_threads)
        self.count_executed_statements = 0
        self.query_history = []

    def close(self):
        logger.debug("closing connection")
        if self.conn is not None:
            self.conn.close()
        logger.debug("closing database")
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

    def get_all_symbol_hashes_in_compartment(self, compartment_id: int) -> list[int]:
        cmd = f"""
        MATCH (comp1:{HAKCCompartment.get_table_name()})<-[:{HAKCDivision.relation_compartment}]-(div1:{HAKCDivision.get_table_name()})<-[:{HAKCSymbol.relation_division}]-(sym1:{HAKCSymbol.get_table_name()})
        WHERE comp1.CompartmentID = $source_compartment_id
        return sym1.{str(HAKCSymbol.get_primary_key())} AS symbol_hash;
        """
        response = self.execute_prepared_stmt(cmd, source_compartment_id=compartment_id)
        ret = response.get_as_df()['symbol_hash'].to_list()
        return ret

    def get_all_divisions_in_compartment(self, compartment_ids: set[int]) -> dict[int, list[HAKCDivision]]:
        # TODO: ask derrick - is the arrow between comp1 and div1 backwards?
        cmd = f"""
        MATCH (comp1:{HAKCCompartment.get_table_name()})<-[:{HAKCDivision.relation_compartment}]-(div1:{HAKCDivision.get_table_name()})
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
            MATCH (sym:{HAKCSymbol.get_table_name()})-[e:{HAKCSymbol.relation_division}]->(div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(c:{HAKCCompartment.get_table_name()})
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

    def get_valid_targets_from_compartment_id(self, source_compartment_id: int) -> list[int]:
        logger.debug(f"Top of fn get_valid_targets_from_compartment_id")
        cmd = f"""
        MATCH (comp1:{HAKCCompartment.get_table_name()})<-[:{HAKCDivision.relation_compartment}]-(div1:{HAKCDivision.get_table_name()})<-[:{HAKCSymbol.relation_division}]-(sym1:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_dag}]->(sym2:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_division}]->(div2:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(comp2:{HAKCCompartment.get_table_name()})
        WHERE comp1.CompartmentID = $source_compartment_id
        RETURN DISTINCT comp2.CompartmentID AS Target;
        """

        logger.debug(f"About to execute command: {cmd}")
        response = self.execute_prepared_stmt(cmd, source_compartment_id=source_compartment_id)
        logger.debug(f"Executed command: {cmd}")
        targets = response.get_as_df()['Target'].tolist()
        logger.debug(f"Found targets {targets} for compartment_id = {source_compartment_id}")

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

    def get_symbols_by_name_list(self, symbol_list: list[str]) -> list[HAKCSymbol]:
        result = self._get_symbols(
            where_clause=f'WHERE sym.Name in [{", ".join(["\"" + str(symbol_name) + "\"" for symbol_name in symbol_list])}]'
        )
        return result

    def get_symbols_by_name(self, symbol_name: str) -> list[HAKCSymbol]:
        result = self._get_symbols(
            where_clause=f'WHERE sym.Name = $symbol_name',
            symbol_name=symbol_name
        )
        return result

    def get_symbol_hashes_to_symbols(self):
        symbol_hashes = self.get_all_symbol_hashes()
        symbols = self.get_symbol_by_hash(symbol_hashes, assume_defined=False)
        try:
            assert (len(symbol_hashes) == len(symbols))
        except AssertionError as e:
            logger.debug(f"Asserting len(symbol_hashes) == len(symbols); {len(symbol_hashes)} =?= {len(symbols)}")
            logger.debug(e)
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
        RETURN comp1.{str(HAKCCompartment.get_primary_key())} AS CompartmentID,  COUNT {{ MATCH (comp1)<-[:{HAKCDivision.relation_compartment}]-(:{HAKCDivision.get_table_name()})<-[:{HAKCSymbol.relation_division}]-(:{HAKCSymbol.get_table_name()}) }} AS Count
        """
        response = self.execute_prepared_stmt(cmd)
        result = dict()
        for _, row in response.get_as_df().iterrows():
            compartment_id = int(row['CompartmentID'].item())
            count = int(row['Count'].item())
            result[compartment_id] = count

        return result

    def get_symbol_definition_location(self, symbol: HAKCSymbol) -> Optional[tuple[HAKCCompilationUnit, int]]:
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
        response = self.conn.execute(prepared_stmt, parameters=kwargs)
        self.query_history.append((prepared_stmt, response))
        return response

    def insert_from_dataframe(self, table_name: str, df: pd.DataFrame):
        self.conn.execute(f'COPY {table_name} FROM df')

    # def _get_symbols(self, where_clause: Optional[str] = None, limit: int = 0, **kwargs) -> list[HAKCSymbol]:
    def _get_symbols(self, where_clause: None | str = None, limit: int = 0, assume_defined: bool = True) -> list[
                                                                                                                HAKCSymbol] | int:
        # assumes all symobls are defined, but this may not be a valid assumption
        symbol = HAKCSymbol.get_table_name()
        symbol_cu_edge = HAKCSymbol.relation_compilation_unit
        symbol_type_edge = HAKCSymbol.relation_type
        cu = HAKCCompilationUnit.get_table_name()
        _type = HAKCType.get_table_name()
        scope = HAKCScope.get_table_name()
        symbol_scope_edge = HAKCSymbol.relation_scope

        type_attrs = HAKCDatabase.get_object_attributes(HAKCType)
        scope_attrs = HAKCDatabase.get_object_attributes(HAKCScope)
        symbol_attrs = HAKCDatabase.get_object_attributes(HAKCSymbol)
        cu_attrs = HAKCDatabase.get_object_attributes(HAKCCompilationUnit)

        # MATCH (scope:{HAKCScope.get_table_name()})<-[:{HAKCSymbol.HasScopeTable}]-(sym:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.IsTypeTable}]->(ty:{HAKCType.get_table_name()}),
        # (sym)-[def:{HAKCSymbol.DefinedInTable}]->(cu:{HAKCCompilationUnit.get_table_name()})
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
        cmd_str = " ".join(cmd)
        response = self.execute_prepared_stmt(cmd_str)
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
            response = self.execute_prepared_stmt(cmd, symbol_hash=hash(_symbol))
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
            response = self.execute_prepared_stmt(cmd, symbol_hash=hash(_symbol))
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
            # print(f"final_hash: {int(_symbol.get_computed_hash().final_hash)}")
            response = self.execute_prepared_stmt(cmd, symbol_hash=hash(_symbol))
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
            response = self.execute_prepared_stmt(cmd, symbol_hash=hash(_symbol))
            types_used = set()
            if response.has_next():
                info = response.get_as_df()
                # print(info)
                for data in info.to_dict(orient='records'):
                    # print(data)
                    ty = HAKCDatabase.__create_object_from_response(HAKCType, **data)
                    ty_perm = HAKCTypePerm(Type=ty, RWX=(data["RWX.R"] << 2) + (data["RWX.W"] << 1) + data["RWX.X"])
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
        if cls == HAKCFunction or cls == HAKCGlobalVariable:
            data["HAKCSymbol.Scope"] = HAKCDatabase.__create_object_from_response(HAKCScope, **data)
            data["HAKCSymbol.Type"] = HAKCDatabase.__create_object_from_response(HAKCType, **data)

            # print(data.keys())
            # if HAKCCompilationUnit.get_primary_key().column_name in data:
            if "HAKCCompilationUnit.DefiningLine" in data:
                # print(f"Found CompilationUnit")
                data["HAKCSymbol.CompilationUnit"] = HAKCDatabase.__create_object_from_response(HAKCCompilationUnit,
                                                                                                **data)
            cls_data = {key.removeprefix(f"{cls.get_table_name()}."): val for key, val in data.items()}
            if cls == HAKCFunction:
                return HAKCFunction(**cls_data)
            if cls == HAKCGlobalVariable:
                return HAKCGlobalVariable(**cls_data)

        # elif cls == HAKCGlobalVariable:
        #     data["HAKCSymbol.Scope"] = HAKCDatabase.__create_object_from_response(HAKCScope, **data)
        #     data["HAKCSymbol.Type"] = HAKCDatabase.__create_object_from_response(HAKCType, **data)
        #     cls_data = {key.removeprefix(f"{cls.get_table_name()}."): val for key, val in data.items()}
        #     print(f"__create_object_from_response GV: {cls_data}")
        #     return HAKCGlobalVariable(**cls_data)
        elif cls == HAKCDivision:
            # data["HAKCDivision.CompartmentID"] = data["HAKCCompartment.CompartmentID"]
            compartment = HAKCDatabase.__create_object_from_response(HAKCCompartment, **data)
            # data["HAKCDivision.CompartmentID"] = data["HAKCCompartment.CompartmentID"]
            div_data = {key.removeprefix(f"{cls.get_table_name()}."): val for key, val in data.items() if
                        key.startswith(cls.get_table_name())}
            return HAKCDivision(**div_data), compartment
        elif cls == HAKCCompartment:
            cls_data = {key.removeprefix(f"{cls.get_table_name()}."): val for key, val in data.items() if
                        key.startswith(cls.get_table_name())}
            logger.debug(f"cls_data for compartment: {cls_data}")
            return HAKCCompartment(**cls_data)

        cls_data = {key.removeprefix(f"{cls.get_table_name()}."): val for key, val in data.items() if
                    key.startswith(cls.get_table_name())}
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
        response = self.execute_prepared_stmt(cmd, symbol_hash=hash(_symbol))
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
        assert (isinstance(_symbol, HAKCSymbol))
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
        WHERE {symbol}.{symbol_hash} = $symbol_hash  
        RETURN DISTINCT {scope_attrs}, {symbol_attrs}, {division_attrs}, {compartment_attrs};
        """
        # print(cmd)
        # Note: some issue with the below prepared statement
        response = self.execute_prepared_stmt(cmd, symbol_hash=hash(_symbol))
        div, comp = None, None
        if response.has_next():
            # Note: the uint64 hashes seem to be cast to floats if a nan is present
            info = response.get_as_df()
            # print(info)
            assert len(info) == 1, print(info)
            logger.error(info)
            for data in info.to_dict(orient='records'):
                # print(f"Division and Compartment data: {data}")
                div, comp = HAKCDatabase.__create_object_from_response(HAKCDivision, **data)

                # print(div)
                # print(comp)
        # for division, compartment in self.get_divisions_compartments():
        #     compartment.entry_token = self.compute_entry_token(compartment.compartment_id)
        #     division.access_token = self.compute_access_token(division.division_id, compartment.compartment_id)
        #     print(f"Computed access token and entry token for {division}, and {compartment}")
        assert (div and comp)
        return div, comp

    @staticmethod
    def get_object_attributes(cls):
        assert (cls in {HAKCType, HAKCScope, HAKCSymbol, HAKCFunction, HAKCCompilationUnit, HAKCDivision,
                        HAKCCompartment})
        return ", ".join([f"{cls.get_table_name()}.{x.column_name}" for x in cls.get_data_columns()] + [
            f"{cls.get_table_name()}.{cls.get_primary_key()}"])

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
                        (data[f"{symbol_compilation_unit_edge}.DefiningLine"] == data[
                            f"{symbol_compilation_unit_edge}.DefiningLine"])):
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
                    # assert(False)
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
        if (f"{symbol}.{symbol_hash}" in ret) and (len(ret[f"{symbol}.{symbol_hash}"]) > 0):
            hash = ret[f"{symbol}.{symbol_hash}"][0]
            # print(type(hash))
            logger.debug(
                f"Queried symbol hash: {hash} from (Name, DefiningFile, DefiningLine) = ({Name}, {DefiningFile}, {DefiningLine})")
            return int(hash)
        else:
            logger.debug(
                f"Queried symbol hash from (Name, DefiningFile, DefiningLine) = ({Name}, {DefiningFile}, {DefiningLine}), but could not find symbol hash!")
            return None

    def set_compartment_id_by_symbol(self, _symbol: HAKCSymbol, new_compartment_id: int):
        symbol = HAKCSymbol.get_table_name()
        symbol_hash = HAKCSymbol.get_primary_key()
        division = HAKCDivision.get_table_name()
        symbol_division_edge = HAKCSymbol.relation_division
        _type = HAKCType.get_table_name()
        division_compartment_edge = HAKCDivision.relation_compartment
        compartment = HAKCCompartment.get_table_name()

        # delete old relationship between symbol and compartment
        # Note: Assumes that the new compartment id exists in the database, which is true for the time being
        cmd = f"""
        MATCH ({symbol})-[:{symbol_division_edge}]->(div:{division})-[old_edge:{division_compartment_edge}]->(comp:{compartment})
        WHERE {symbol}.{symbol_hash} = $symbol_hash
        DELETE old_edge
        RETURN comp.CompartmentID
        """
        response = self.execute_prepared_stmt(cmd, symbol_hash=hash(_symbol))
        ret = response.get_as_df()
        logger.debug(ret)
        cmd = f"""
        MATCH ({symbol})-[:{symbol_division_edge}]->(div:{division}), (comp:{compartment})
        WHERE {symbol}.{symbol_hash} = $symbol_hash AND comp.CompartmentID = $compartment_id
        CREATE (div)-[new_edge]->(comp)
        RETURN comp.CompartmentID
        """
        response = self.execute_prepared_stmt(cmd, symbol_hash=int(_symbol.get_computed_hash().final_hash),
                                              compartment_id=new_compartment_id)
        ret = response.get_as_df()
        logger.debug(ret)
        return ret["comp.CompartmentID"][0]
