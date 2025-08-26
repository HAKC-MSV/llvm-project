import logging
import multiprocessing as mp
import math
from typing import Type, Optional, Tuple, cast

import pandas as pd

from .HAKCBase import HAKCDBNode, HAKCDBRelation
from .HAKCLogger import HAKCLogger
from .HAKCObjects import HAKCSymbol, HAKCFunction, HAKCScope, HAKCType, HAKCGlobalVariable, HAKCDivision, \
    HAKCCompartment, HAKCDefinitionLocation

logging.setLoggerClass(HAKCLogger)
logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc-dag'))


class HAKCDatabase:
    def __init__(self, db_dir: str, read_only: bool = False, max_num_threads=int(mp.cpu_count() / 2)):
        self.db_dir = db_dir
        self.database = None
        self.conn = None
        self.open(read_only=read_only, max_num_threads=max_num_threads)
        self.count_executed_statements = 0

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
        self.count_executed_statements += 1
        response = self.conn.execute(prepared_stmt, parameters=kwargs)
        return response

    def get_compartment_entry_token_from_id(self, compartment_id: int) -> Optional[int]:
        cmd = f"""
        MATCH
        (comp:{HAKCCompartment.get_table_name()})<-[:{str(HAKCDivision.relation_compartment)}]-(div1:{HAKCDivision.get_table_name()})<-[:{str(HAKCSymbol.relation_division)}]-(:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_dag}]->(:{HAKCSymbol.get_table_name()})
        WHERE comp.{str(HAKCCompartment.get_primary_key())} = $compartment_id
        RETURN DISTINCT div1.DivisionID AS DivisionID
        """
        response = self.execute_prepared_stmt(cmd, compartment_id=compartment_id)
        ret = response.get_as_df()
        target_divisions = set()
        for division_id in ret["DivisionID"]:
            target_divisions.add(int(division_id))
        return HAKCCompartment.compute_entry_token(compartment_id, target_divisions)


    # def get_compartment_entry_token_from_id(self, compartment_id: int) -> Optional[int]:
    #     cmd = f"""
    #     MATCH (comp:{HAKCCompartment.get_table_name()})
    #     WITH comp.CompartmentID as compartment_id, comp.EntryToken as entry_token
    #     WHERE compartment_id = $compartment_id
    #     RETURN entry_token;
    #     """
    #     response = self.execute_prepared_stmt(cmd, compartment_id=compartment_id)
    #     ret = response.get_as_df()
    #     if ret.empty:
    #         logger.debug(f'Command: {cmd} returned None')
    #         logger.debug(f'Searched with compartment_id: {compartment_id}')
    #         return None
    #     else:
    #         entry_token = ret["entry_token"][0]
    #         logger.debug(f"Found entry_token: {entry_token} for compartment_id: {compartment_id}")
    #         return int(entry_token)

    def get_division_access_token_from_id(self, division_id: int, compartment_id: int) -> Optional[int]:
        cmd = f"""
        MATCH
        (comp:{HAKCCompartment.get_table_name()})<-[:{str(HAKCDivision.relation_compartment)}]-(div1:{HAKCDivision.get_table_name()})<-[:{str(HAKCSymbol.relation_division)}]-(:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_dag}]->(:{HAKCSymbol.get_table_name()})-[:{str(HAKCSymbol.relation_division)}]->(div2:{HAKCDivision.get_table_name()})-[:{str(HAKCDivision.relation_compartment)}]->(comp)
        WHERE comp.{str(HAKCCompartment.get_primary_key())} = $compartment_id AND div1.DivisionID = $division_id
        RETURN DISTINCT div2.DivisionID AS DivisionID
        """
        response = self.execute_prepared_stmt(cmd, division_id=division_id, compartment_id=compartment_id)
        ret = response.get_as_df()
        division_ids = {division_id}
        for division_id in ret["DivisionID"]:
            division_ids.add(int(division_id))
        return HAKCDivision.compute_access_token(compartment_id, division_ids)

    def get_division(self, division_id: int, compartment_id: int) -> Optional[HAKCDivision]:
        access_token = self.get_division_access_token_from_id(division_id, compartment_id)
        cmd = f"""
        MATCH (div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(c:{HAKCCompartment.get_table_name()})
        WHERE div.DivisionID = $division_id AND c.CompartmentID = $compartment_id
        RETURN div.DivisionID AS DivisionID, div.Salt AS Salt, div.{str(HAKCDivision.get_primary_key())} AS division_hash
        """
        response = self.execute_prepared_stmt(cmd, division_id=division_id, compartment_id=compartment_id)
        if response.has_next():
            ret = response.get_as_df()
            data = ret.to_dict(orient='records')
            division = HAKCDivision(AccessToken=access_token, **data)
            return division
        return None

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
                f"Found ({division}, {compartment}) for symbol: {symbol}")
            return division, compartment

    #
    # def get_division_access_token_from_id(self, division_id: int, compartment_id: int) -> Optional[int]:
    #     cmd = f"""
    #     MATCH (div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(comp:{HAKCCompartment.get_table_name()})
    #     WITH div.DivisionID as division_id, div.AccessToken as access_token, comp.CompartmentID AS compartment_id, comp.EntryToken AS entry_token
    #     WHERE division_id = $division_id AND compartment_id = $compartment_id
    #     RETURN access_token, entry_token;
    #     """
    #     response = self.execute_prepared_stmt(cmd, division_id=division_id, compartment_id=compartment_id)
    #     ret = response.get_as_df()
    #     if ret.empty:
    #         logger.debug(f'Command: {cmd} returned None')
    #         logger.debug(f'Searched with division_id: {division_id}, compartment_id: {compartment_id}')
    #         return None
    #     else:
    #         access_token = ret["access_token"][0]
    #         logger.debug(f"Found access_token: {access_token} for division_id: {division_id}")
    #         return int(access_token)
    #
    # def get_all_symbol_hashes_in_compartment(self, compartment_id: int) -> list[int]:
    #     cmd = f"""
    #     MATCH (comp1:{HAKCCompartment.get_table_name()})<-[:{HAKCDivision.relation_compartment}]-(div1:{HAKCDivision.get_table_name()})<-[:{HAKCSymbol.relation_division}]-(sym1:{HAKCSymbol.get_table_name()})
    #     WHERE comp1.CompartmentID = $source_compartment_id
    #     return sym1.{str(HAKCSymbol.get_primary_key())} AS symbol_hash;
    #     """
    #     response = self.execute_prepared_stmt(cmd, source_compartment_id=compartment_id)
    #     ret = response.get_as_df()['symbol_hash'].to_list()
    #     return ret
    #
    # def get_all_divisions_in_compartment(self, compartment_ids: set[int]) -> dict[int, list[HAKCDivision]]:
    #     cmd = f"""
    #     MATCH (comp1:{HAKCCompartment.get_table_name()})<-[:{HAKCDivision.relation_compartment}]-(div1:{HAKCDivision.get_table_name()})
    #     WHERE comp1.CompartmentID IN [{','.join([str(i) for i in compartment_ids])}]
    #     RETURN div1.DivisionID as DivisionID, comp1.CompartmentID AS compartment_id, div1.{str(HAKCDivision.get_primary_key())} AS {str(HAKCDivision.get_primary_key())}
    #     """
    #     divisions = dict()
    #     response = self.execute_prepared_stmt(cmd)
    #     if response.has_next():
    #         info = response.get_as_df()
    #         for data in info.to_dict(orient='records'):
    #             division = HAKCDivision(**data)
    #             if division.compartment_id not in divisions:
    #                 divisions[division.compartment_id] = list()
    #             divisions[division.compartment_id].append(division)
    #     return divisions
    #
    # def merge_compartments(self, compartments_to_merge: dict[int, int]):
    #     target_compartments = set(compartments_to_merge.values())
    #     compartments_to_remove = set(compartments_to_merge.keys())
    #     existing_divisions = self.get_all_divisions_in_compartment(target_compartments)
    #
    #     cmd = f"""
    #         MATCH (sym:{HAKCSymbol.get_table_name()})-[e:{HAKCSymbol.relation_division}]->(div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(c:{HAKCCompartment.get_table_name()})
    #         WHERE c.{str(HAKCCompartment.get_primary_key())} IN [{','.join([str(i) for i in compartments_to_remove])}]
    #         DELETE e
    #         RETURN sym.{str(HAKCSymbol.get_primary_key())} AS SymbolHash, div.DivisionID AS DivisionID, c.{str(HAKCCompartment.get_primary_key())} AS CompartmentID;
    #     """
    #     result = self.execute_prepared_stmt(cmd)
    #     data = result.get_as_df()
    #     symbol_hashes = list()
    #     division_hashes = list()
    #
    #     for _, row in data.iterrows():
    #         division_id = row['DivisionID'].item()
    #         symbol_hash = row['SymbolHash'].item()
    #         old_compartment_id = row['CompartmentID'].item()
    #
    #         division_list = existing_divisions[compartments_to_merge[old_compartment_id]]
    #         symbol_hashes.append(symbol_hash)
    #
    #         division_hash = hash(division_list[0])
    #
    #         for existing_division in division_list:
    #             if existing_division.division_id == division_id:
    #                 division_hash = hash(existing_division)
    #         division_hashes.append(division_hash)
    #
    #     df = pd.DataFrame({
    #         "from": symbol_hashes,
    #         "to": division_hashes,
    #     })
    #     self.insert_from_dataframe(HAKCSymbol.relation_division, df)
    #
    #     cmd = f"""
    #         MATCH (div:{HAKCDivision.get_table_name()})-[:{HAKCDivision.relation_compartment}]->(c:{HAKCCompartment.get_table_name()})
    #         WHERE c.CompartmentID IN [{','.join([str(i) for i in compartments_to_remove])}]
    #         DETACH DELETE div;
    #     """
    #     self.execute_prepared_stmt(cmd)
    #     cmd = f"""
    #         MATCH (c:{HAKCCompartment.get_table_name()})
    #         WHERE c.CompartmentID IN [{','.join([str(i) for i in compartments_to_remove])}]
    #         DETACH DELETE c;
    #     """
    #     self.execute_prepared_stmt(cmd)

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
        RETURN comp1.CompartmentID, comp2.CompartmentID
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

    def get_symbol_by_hash(self, symbol_hashes: list[int], **kwargs) -> set[HAKCSymbol]:
        logger.error(f"get_symbol_by_hash: {symbol_hashes} of type: {type(symbol_hashes)}")
        logger.error(f"Type of symbol_hash: {type(symbol_hashes)}, symbol_hash[0]: {type(symbol_hashes[0])}")
        # assert(isinstance(symbol_hashes, list))
        result = self._get_symbols(where=f'HAKCSymbol.symbol_hash in [{", ".join([str(sh) for sh in symbol_hashes])}]', **kwargs)
        return result

    def get_symbols_by_name(self, symbol_name: str) -> list[HAKCSymbol]:
        result = self._get_symbols(symbol_name=symbol_name)
        return list(result)

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
        MATCH (div:{HAKCDivision.get_table_name()})
        RETURN div.DivisionID as DivisionID, div.Salt as Salt, div.{str(HAKCDivision.get_primary_key())} as division_hash
        """
        divisions = set()
        response = self.execute_prepared_stmt(cmd)
        if response.has_next():
            info = response.get_as_df()
            for data in info.to_dict(orient='records'):
                divisions.add(HAKCDivision(**data))
        return divisions
    #
    # def get_all_compartments(self) -> list[HAKCCompartment]:
    #     cmd = f"""
    #     MATCH (c:{HAKCCompartment.get_table_name()})
    #     RETURN {", ".join(["c." + str(col) + " as " + str(col) for col in HAKCCompartment.get_db_table_columns()])}
    #     """
    #     compartments = list()
    #     response = self.execute_prepared_stmt(cmd)
    #     if response.has_next():
    #         info = response.get_as_df()
    #         for data in info.to_dict(orient='records'):
    #             compartment = HAKCCompartment(**data)
    #             compartments.append(compartment)
    #     return compartments
    #
    # def get_compartment_symbol_count(self) -> dict[int, int]:
    #     cmd = f"""
    #     MATCH
    #     (comp1:{HAKCCompartment.get_table_name()})
    #     RETURN comp1.{str(HAKCCompartment.get_primary_key())} AS CompartmentID,  COUNT {{ MATCH (comp1)<-[:{HAKCDivision.relation_compartment}]-(:{HAKCDivision.get_table_name()})<-[:{HAKCSymbol.relation_division}]-(:{HAKCSymbol.get_table_name()}) }} AS Count
    #     """
    #     response = self.execute_prepared_stmt(cmd)
    #     result = dict()
    #     for _, row in response.get_as_df().iterrows():
    #         compartment_id = int(row['CompartmentID'].item())
    #         count = int(row['Count'].item())
    #         result[compartment_id] = count
    #
    #     return result
    #
    # def get_symbol_definition_location(self, symbol: HAKCSymbol) -> Optional[tuple[HAKCDefinitionLocation, int]]:
    #     symbol = HAKCSymbol.get_table_name()
    #     symbol_hash = HAKCSymbol.get_primary_key()
    #     # symbol_hash = HAKCSymbol.get_primary_key().column_name
    #     symbol_cu_edge = HAKCSymbol.relation_compilation_unit
    #     cu = HAKCDefinitionLocation.get_table_name()
    #     cmd = f"""
    #     MATCH ({symbol})-[{symbol_cu_edge}]->({cu})
    #     WHERE {symbol}.{symbol_hash} = $symbol_hash
    #     RETURN {cu}.filename, {symbol_cu_edge}.line;
    #     """
    #     response = self.execute_prepared_stmt(cmd, symbol_hash=hash(symbol))
    #     if response.has_next():
    #         resp_dict = response.get_as_df().to_dict(orient='records')
    #         logger.info(f'resp_dict df: {resp_dict}')
    #         return HAKCDefinitionLocation(filename=resp_dict['filename'][0]), resp_dict['line'][0]
    #     return None

    def get_symbol_definition_location(self, symbol: HAKCSymbol) -> Optional[HAKCDefinitionLocation]:
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()})-[e:{HAKCSymbol.relation_definition_location}]->(dl:{HAKCDefinitionLocation.get_table_name()})
        WHERE sym.{str(HAKCSymbol.get_primary_key())} = $symbol_hash
        RETURN dl.DefiningFile as DefiningFile, e.DefiningLine as DefiningLine;
        """
        response = self.execute_prepared_stmt(cmd, symbol_hash=hash(symbol))
        if response.has_next():
            resp_dict = response.get_as_df().to_dict(orient='records')
            dl = HAKCDefinitionLocation(**resp_dict)
            return dl
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

    def insert_from_dataframe(self, table_name: str, df: pd.DataFrame):
        self.conn.execute(f'COPY {table_name} FROM df')
    #
    # def get_indirect_calls(self, _symbol: HAKCSymbol) -> set[HAKCFunction]:
    #     _type = HAKCType.get_table_name()
    #     symbol = HAKCSymbol.get_table_name()
    #     symbol_hash = HAKCSymbol.get_primary_key()
    #     scope = HAKCScope.get_table_name()
    #     compilation_unit = HAKCDefinitionLocation.get_table_name()
    #     _type = HAKCType.get_table_name()
    #     symbol_scope_edge = HAKCSymbol.relation_scope
    #     symbol_compilation_unit_edge = HAKCSymbol.relation_compilation_unit
    #     function_indirect_calls = HAKCFunction.relation_indirect_calls
    #
    #     type_attrs = HAKCDatabase.get_object_attributes(HAKCType)
    #     scope_attrs = HAKCDatabase.get_object_attributes(HAKCScope)
    #     symbol_attrs = HAKCDatabase.get_object_attributes(HAKCSymbol)
    #     cu_attrs = HAKCDatabase.get_object_attributes(HAKCDefinitionLocation)
    #
    #     cmd = f"""
    #     MATCH (_:{symbol})-[:{function_indirect_calls}]->({_type}),
    #     ({symbol})-[{symbol_scope_edge}]->({scope})
    #     WHERE _.{symbol_hash} = $symbol_hash AND _.{symbol_hash} <> {symbol}.{symbol_hash} AND
    #             {_type}.{HAKCType.get_primary_key()} IS NOT NULL AND {symbol}.{symbol_hash} IS NOT NULL AND {scope}.{HAKCScope.get_primary_key()} IS NOT NULL
    #     OPTIONAL MATCH ({symbol})-[{symbol_compilation_unit_edge}]-({compilation_unit})
    #     RETURN DISTINCT {type_attrs}, {scope_attrs}, {symbol_attrs}, {cu_attrs};
    #     """
    #     try:
    #         response = self.execute_prepared_stmt(cmd, symbol_hash=hash(_symbol))
    #         indirect_calls = set()
    #         if response.has_next():
    #             info = response.get_as_df()
    #             # print(info)
    #             for data in info.to_dict(orient='records'):
    #                 ty = HAKCDatabase.__create_object_from_response(HAKCType, **data)
    #                 indirect_calls.add(ty)
    #     except Exception as e:
    #         logger.error(f'get_indirect_calls failed')
    #         raise e
    #     return indirect_calls
    #
    #
    # def get_direct_calls(self, _symbol: HAKCSymbol) -> set[HAKCFunction]:
    #     # TODO: should compilation unit be added to the direct call output in dag.yaml?
    #     symbol_symbol_direct_call_edge = HAKCFunction.relation_direct_calls
    #     _type = HAKCType.get_table_name()
    #     symbol = HAKCSymbol.get_table_name()
    #     symbol_hash = HAKCSymbol.get_primary_key()
    #     scope = HAKCScope.get_table_name()
    #     compilation_unit = HAKCDefinitionLocation.get_table_name()
    #     _type = HAKCType.get_table_name()
    #     symbol_type_edge = HAKCSymbol.relation_type
    #     symbol_scope_edge = HAKCSymbol.relation_scope
    #     symbol_compilation_unit_edge = HAKCSymbol.relation_compilation_unit
    #
    #     type_attrs = HAKCDatabase.get_object_attributes(HAKCType)
    #     scope_attrs = HAKCDatabase.get_object_attributes(HAKCScope)
    #     symbol_attrs = HAKCDatabase.get_object_attributes(HAKCSymbol)
    #     cu_attrs = HAKCDatabase.get_object_attributes(HAKCDefinitionLocation)
    #
    #     cmd = f"""
    #     MATCH (_:{symbol})-[:{symbol_symbol_direct_call_edge}]->({symbol})-[:{symbol_type_edge}]->({_type}),
    #     ({symbol})-[{symbol_scope_edge}]->({scope})
    #     WHERE _.{symbol_hash} = $symbol_hash AND _.{symbol_hash} <> {symbol}.{symbol_hash} AND {symbol}.IsFunction AND
    #             {_type}.{HAKCType.get_primary_key()} IS NOT NULL AND {symbol}.{symbol_hash} IS NOT NULL AND {scope}.{HAKCScope.get_primary_key()} IS NOT NULL
    #     OPTIONAL MATCH ({symbol})-[{symbol_compilation_unit_edge}]-({compilation_unit})
    #     RETURN DISTINCT {type_attrs}, {scope_attrs}, {symbol_attrs}, {cu_attrs};
    #     """
    #     try:
    #         response = self.execute_prepared_stmt(cmd, symbol_hash=hash(_symbol))
    #         direct_calls = set()
    #         if response.has_next():
    #             info = response.get_as_df()
    #             # print(info)
    #             for data in info.to_dict(orient='records'):
    #                 func = HAKCDatabase.__create_object_from_response(HAKCFunction, **data)
    #                 direct_calls.add(func)
    #     except Exception as e:
    #         logger.error(f'get_direct_calls failed')
    #         raise e
    #     return direct_calls

    # def _get_symbols(self, where_clause: None | str = None, limit: int = 0, assume_defined: bool = True) -> list[
    #                                                                                                             HAKCSymbol] | int:
    #     # assumes all symobls are defined, but this may not be a valid assumption
    #     cmd = [f"""
    #     MATCH (scope:{HAKCScope.get_table_name()})<-[:{HAKCSymbol.relation_scope}]-(sym:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_type}]->(ty:{HAKCType.get_table_name()})
    #     """]
    #     if assume_defined:
    #         cmd.append(
    #             f",(sym)-[def:{HAKCSymbol.relation_definition_location}]->(cu:{HAKCDefinitionLocation.get_table_name()})")
    #     if where_clause is not None:
    #         cmd.append(where_clause)
    #
    #     cmd.append("RETURN")
    #     return_str = """
    #     sym.Name, sym.IsFunction AS is_function, scope.Scope, scope.LocalScopeName, ty.DebugType, ty.LLVMType
    #     """
    #     cmd.append(f'{return_str}')
    #     if assume_defined:
    #         cmd.append(", def.DefiningLine AS DefiningLine, cu.DefiningFile AS DefiningFile")
    #     cmd.append(f"""
    #         ORDER BY sym.Name, ty.DebugType, scope.Scope, scope.LocalScopeName
    #     """)
    #     if limit > 0:
    #         cmd.append(f'LIMIT {limit}')
    #     symbols = list()
    #     response = self.execute_prepared_stmt(prepared_stmt=" ".join(cmd))
    #     if response.has_next():
    #         info = response.get_as_df()
    #         for data in info.to_dict(orient='records'):
    #             symbol = self._create_symbol_from_response(**data)
    #             symbols.append(symbol)
    #
    #     return symbols

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
                MATCH (head: {HAKCSymbol.get_table_name()})-[:{HAKCFunction.relation_direct_calls}]->(tail: {HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_type}]->(ty:{HAKCType.get_table_name()}),
                (tail)-[{HAKCSymbol.relation_scope}]->(scope:{HAKCScope.get_table_name()})
                OPTIONAL MATCH (head)-[def:{HAKCSymbol.relation_definition_location}]->(dl:{HAKCDefinitionLocation.get_table_name()})
                WHERE head.symbol_hash = $symbol_hash and head.symbol_hash <> tail.symbol_hash
                RETURN DISTINCT head.*, tail.*, ty.*, def.*, dl.*, scope.*;
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
        # TODO: do we actually need the definition location of the connected symbol?
        # cmd = f"""
        #     MATCH (head:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_symbol}]->(tail:{HAKCSymbol.get_table_name()}),
        #     (sc:{HAKCScope.get_table_name()})<-[:{HAKCSymbol.relation_scope}]-(tail)-[:{HAKCSymbol.relation_type}]->(ty:{HAKCType.get_table_name()})
        #     WHERE head.symbol_hash=$symbol_hash
        #     RETURN tail.Name, tail.DefiningFile, tail.DefiningLine, tail.IsFunction AS is_function, sc.Scope,
        #     sc.LocalScopeName, ty.DebugType, ty.LLVMType;
        # """
        cmd = f"""
            MATCH (head:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_symbol}]->(tail:{HAKCSymbol.get_table_name()}),
            (sc:{HAKCScope.get_table_name()})<-[:{HAKCSymbol.relation_scope}]-(tail)-[:{HAKCSymbol.relation_type}]->(ty:{HAKCType.get_table_name()})
            WHERE head.symbol_hash=$symbol_hash
            RETURN tail.Name, tail.IsFunction AS is_function, sc.Scope,
            sc.LocalScopeName, ty.DebugType, ty.LLVMType;
        """
        try:
            # TODO: does hash(symbol) work as intended?
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


    # def get_used_symbols(self, _symbol: HAKCSymbol) -> set[HAKCSymbol]:
    #     # NOTE: this seems to not properly return HAKCFunction with defining line and file information
    #     # seems to be duplicate defining line showing up as DefiningLine, and head.DefiningLine, which is causing issues...
    #     # so, I guess, don't try to manually rename things in the return statement or it might not be processed correctly
    #     symbol_symbol_edge = HAKCSymbol.relation_symbol
    #     _type = HAKCType.get_table_name()
    #     symbol = HAKCSymbol.get_table_name()
    #     symbol_hash = HAKCSymbol.get_primary_key()
    #     scope = HAKCScope.get_table_name()
    #     compilation_unit = HAKCDefinitionLocation.get_table_name()
    #     _type = HAKCType.get_table_name()
    #     symbol_type_edge = HAKCSymbol.relation_type
    #     symbol_scope_edge = HAKCSymbol.relation_scope
    #     symbol_compilation_unit_edge = HAKCSymbol.relation_compilation_unit
    #
    #     type_attrs = HAKCDatabase.get_object_attributes(HAKCType)
    #     scope_attrs = HAKCDatabase.get_object_attributes(HAKCScope)
    #     symbol_attrs = HAKCDatabase.get_object_attributes(HAKCSymbol)
    #     cu_attrs = HAKCDatabase.get_object_attributes(HAKCDefinitionLocation)
    #
    #     cmd = f"""
    #     MATCH (_:{symbol})-[:{symbol_symbol_edge}]->({symbol})-[:{symbol_type_edge}]->({_type}),
    #     ({symbol})-[{symbol_scope_edge}]->({scope})
    #     WHERE _.{symbol_hash} = $symbol_hash AND _.{symbol_hash} <> {symbol}.{symbol_hash} AND {symbol}.IsFunction AND
    #             {_type}.{HAKCType.get_primary_key()} IS NOT NULL AND {symbol}.{symbol_hash} IS NOT NULL AND {scope}.{HAKCScope.get_primary_key()} IS NOT NULL
    #     OPTIONAL MATCH ({symbol})-[{symbol_compilation_unit_edge}]->({compilation_unit})
    #     RETURN DISTINCT {type_attrs}, {scope_attrs}, {symbol_attrs}, {cu_attrs};
    #     """
    #     try:
    #         # print(f"final_hash: {int(_symbol.get_computed_hash().final_hash)}")
    #         response = self.execute_prepared_stmt(cmd, symbol_hash=hash(_symbol))
    #         used_symbols = set()
    #         if response.has_next():
    #             info = response.get_as_df()
    #             # print(info)
    #             for data in info.to_dict(orient='records'):
    #                 func = HAKCDatabase.__create_object_from_response(HAKCFunction, **data)
    #                 used_symbols.add(func)
    #     except Exception as e:
    #         logger.error(f'get_used_symbols_calls failed')
    #         raise e
    #     return used_symbols
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
        self.execute_prepared_stmt(create_cmd)

    def create_relationship_table(self, edge_type: HAKCDBRelation):
        create_cmd = f'CREATE REL TABLE IF NOT EXISTS {edge_type.get_definition()}'
        self.execute_prepared_stmt(create_cmd)

    def get_dag_edges(self, _symbol: HAKCSymbol) -> set[tuple['HAKCSymbol', int]]:
        # want to reconstruct the output from the yaml exactly, so the compartmentalization can be rebuilt from the database
        # Note: Only need to get enough data to match the node in the networkx graph

        type_attrs = HAKCDatabase.get_object_attributes(HAKCType)
        scope_attrs = HAKCDatabase.get_object_attributes(HAKCScope)
        symbol_attrs = HAKCDatabase.get_object_attributes(HAKCSymbol)
        dl_attrs = HAKCDatabase.get_object_attributes(HAKCDefinitionLocation)
        # todo: refine command
        # cmd = f"""
        # MATCH (sym:{HAKCSymbol.get_table_name()})<-[{HAKCSymbol.relation_dag}]-({HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_type}]->({HAKCType.get_table_name()}),
        #       ({HAKCSymbol.get_table_name()})-[{HAKCSymbol.relation_scope}]->(scope:{HAKCScope.get_table_name()})
        # WHERE sym.{HAKCSymbol.get_primary_key()} = $symbol_hash AND {HAKCType.get_table_name()}.{HAKCType.get_primary_key()} IS NOT NULL
        #         AND scope.{HAKCScope.get_primary_key()} IS NOT NULL AND {HAKCSymbol.relation_dag}.weight IS NOT NULL
        # OPTIONAL MATCH (sym)-[{HAKCSymbol.relation_definition_location}]-({HAKCDefinitionLocation.get_table_name()})
        # RETURN DISTINCT {type_attrs}, {scope_attrs}, {symbol_attrs}, {dl_attrs}, ;
        # """
        cmd = f"""
        MATCH ({HAKCType.get_table_name()}:{HAKCType.get_table_name()})<-[{HAKCSymbol.relation_type}:{HAKCSymbol.relation_type}]-({HAKCSymbol.get_table_name()}:{HAKCSymbol.get_table_name()})-[{HAKCSymbol.relation_scope}:{HAKCSymbol.relation_scope}]->({HAKCScope.get_table_name()}:{HAKCScope.get_table_name()}), 
        (sym:{HAKCSymbol.get_table_name()})<-[{HAKCSymbol.relation_dag}:{HAKCSymbol.relation_dag}]-({HAKCSymbol.get_table_name()})
        WHERE sym.{HAKCSymbol.get_primary_key()} = $symbol_hash  
        OPTIONAL MATCH (sym)-[{HAKCSymbol.relation_definition_location}]-({HAKCDefinitionLocation.get_table_name()})
        RETURN DISTINCT {type_attrs}, {scope_attrs}, {symbol_attrs}, {dl_attrs}, {HAKCSymbol.relation_dag}.weight, {HAKCSymbol.relation_definition_location}.DefiningLine AS DefiningLine,
            COALESCE(
                CASE WHEN {HAKCDefinitionLocation.get_table_name()}.dl_hash IS NOT NULL THEN
                    CAST( {HAKCDefinitionLocation.get_table_name()}.dl_hash AS STRING ) ELSE NULL END, 
                    'NONE') AS dl_hash;
        """

        # print(cmd)
        response = self.execute_prepared_stmt(cmd, symbol_hash=hash(_symbol))
        dag_edges = set()

        if response.has_next():
            # Note: the uint64 hashes seem to be cast to floats if a nan is present
            info = response.get_as_df()
            # print(info)
            for data in info.to_dict(orient='records'):
                # logger.fatal(f"Processing symbol {data['HAKCSymbol.Name'] if 'HAKCSymbol.Name' in data else ''}")
                # print(data)
                data["HAKCDefinitionLocation.dl_hash"] = data["dl_hash"]
                del data["dl_hash"]
                data["HAKCDefinitionLocation.DefiningLine"] = data["DefiningLine"]
                del data["DefiningLine"]

                if data['HAKCDefinitionLocation.dl_hash'] != 'NONE':
                    # the definition location was found, so cast value from string to int
                    data['HAKCDefinitionLocation.dl_hash'] = int(data['HAKCDefinitionLocation.dl_hash'])
                else:
                    # if no definition found, then remove empty keys
                    if 'HAKCDefinitionLocation.dl_hash' in data:
                        del data['HAKCDefinitionLocation.dl_hash']
                    if 'HAKCDefinitionLocation.DefiningFile' in data:
                        del data['HAKCDefinitionLocation.DefiningFile']
                    if 'HAKCDefinitionLocation.DefiningLine' in data:
                        del data['HAKCDefinitionLocation.DefiningLine']
                if data["HAKCSymbol.IsFunction"] is True:
                    func = HAKCDatabase.__create_object_from_response(HAKCFunction, **data)
                    dag_edge = (func, data[f"{HAKCSymbol.relation_symbol}.weight"])
                    dag_edges.add(dag_edge)
                    # print(f"Found DAG: {dag_edge}")
                else:
                    gv = HAKCDatabase.__create_object_from_response(HAKCGlobalVariable, **data)
                    dag_edge = (gv, data[f"{HAKCSymbol.relation_symbol}.weight"])
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
        # print(cmd)
        # Note: some issue with the below prepared statement
        response = self.execute_prepared_stmt(cmd, symbol_hash=hash(_symbol))
        div, comp = None, None
        if response.has_next():
            # Note: the uint64 hashes seem to be cast to floats if a nan is present
            info = response.get_as_df()
            # print(info)
            assert len(info) == 1, print(f"get_division_compartment response is empty: {info}")
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
        assert(div and comp)
        return div, comp

    @staticmethod
    def get_object_attributes(cls):
        assert(cls in {HAKCType, HAKCScope, HAKCSymbol, HAKCFunction, HAKCDefinitionLocation, HAKCDivision, HAKCCompartment})
        return ", ".join([f"{cls.get_table_name()}.{x.column_name}" for x in cls.get_data_columns()] + [f"{cls.get_table_name()}.{cls.get_primary_key()}"])

    # fix to determinism is using colon on edge for kuzu query
    # otherwise, I guess the edge is just labeled but not actually matched
    def _get_symbols(self, symbol_name: str = None, symbol_hash: int = None, where: str = None ) -> set[HAKCSymbol]:
        logger.error(f"_get_symbols with where: {where}")

        # want to reconstruct the output from the yaml exactly, so the compartmentalization can be rebuilt from the database
        # this [:edge*1..2] which recursively searches is probably not needed
        # cmd = "CALL show_tables() RETURN *;"
        # response = self.execute_prepared_stmt(cmd)
        # print(response.get_as_df())
        # logger.fatal(f"Getting all symbols")
        type_attrs = HAKCDatabase.get_object_attributes(HAKCType)
        scope_attrs = HAKCDatabase.get_object_attributes(HAKCScope)
        symbol_attrs = HAKCDatabase.get_object_attributes(HAKCSymbol)
        dl_attrs = HAKCDatabase.get_object_attributes(HAKCDefinitionLocation)
        # Note: must cast dl_hash to string or 'NONE' or else pandas will coerce the type from int to float (NaN is a float), which loses precision and causes the hashes to not match
        # e.g., dl_hash = 6.534723912373195e+16, type(dl_hash) = <class 'float'>, int(dl_hash) = 65347239123731952
        # TODO: figure out why optional match is extremely slow
        # NOTE: run PROFILE MATCH ... to view the performance cost of query
        cmd = f"""
        PROFILE MATCH ({HAKCType.get_table_name()}:{HAKCType.get_table_name()})<-[{HAKCSymbol.relation_type}:{HAKCSymbol.relation_type}]-({HAKCSymbol.get_table_name()}:{HAKCSymbol.get_table_name()})-[{HAKCSymbol.relation_scope}:{HAKCSymbol.relation_scope}]->({HAKCScope.get_table_name()}:{HAKCScope.get_table_name()})
        WHERE TRUE { f' AND {HAKCSymbol.get_table_name()}.Name IS $symbol_name' if symbol_name else '' } { f' AND {HAKCSymbol.get_table_name()}.{HAKCSymbol.get_primary_key()} IS $symbol_hash' if symbol_hash else '' } { f'AND {where}' if where else ''}
        OPTIONAL MATCH ({HAKCSymbol.get_table_name()})-[{HAKCSymbol.relation_definition_location}:{HAKCSymbol.relation_definition_location}]->({HAKCDefinitionLocation.get_table_name()}:{HAKCDefinitionLocation.get_table_name()})
        RETURN DISTINCT {type_attrs}, {scope_attrs}, {symbol_attrs}, {dl_attrs}, {HAKCSymbol.relation_definition_location}.DefiningLine AS DefiningLine,
            COALESCE(
                CASE WHEN {HAKCDefinitionLocation.get_table_name()}.dl_hash IS NOT NULL THEN
                    CAST( {HAKCDefinitionLocation.get_table_name()}.dl_hash AS STRING ) ELSE NULL END, 
                    'NONE') AS dl_hash;
        """
        # logger.error(f"running command: {cmd}")

        cmdargs = dict()
        if symbol_name:
            cmdargs['symbol_name'] = symbol_name
        if symbol_hash:
            cmdargs['symbol_hash'] = symbol_hash
        response = self.execute_prepared_stmt(cmd, **cmdargs)
        # logger.fatal(response)
        functions = set()
        gvs = set()
        while response.has_next():

            lines = response.get_next()
            for line in lines:
                print(line)

            # logger.fatal(str(response.get_next()))
        # if response.has_next():
        #     info = response.get_as_df()
        #     logger.fatal(info)

            # # pd.set_option('display.max_columns', 15)
            # # logger.fatal(data[['HAKCSymbol.Name', 'HAKCDefinitionLocation.dl_hash']].head(3))
            # for data in info.to_dict(orient='records'):
            #     # print(data)
            #     # logger.debug(f"Processing symbol {data['HAKCSymbol.Name'] if 'HAKCSymbol.Name' in data else ''}")
            #     data["HAKCDefinitionLocation.dl_hash"] = data["dl_hash"]
            #     del data["dl_hash"]
            #     data["HAKCDefinitionLocation.DefiningLine"] = data["DefiningLine"]
            #     del data["DefiningLine"]
            #
            #     if data['HAKCDefinitionLocation.dl_hash'] != 'NONE':
            #         # the definition location was found, so cast value from string to int
            #         data['HAKCDefinitionLocation.dl_hash'] = int(data['HAKCDefinitionLocation.dl_hash'])
            #     else:
            #         # if no definition found, then remove empty keys
            #         if 'HAKCDefinitionLocation.dl_hash' in data:
            #             del data['HAKCDefinitionLocation.dl_hash']
            #         if 'HAKCDefinitionLocation.DefiningFile' in data:
            #             del data['HAKCDefinitionLocation.DefiningFile']
            #         if 'HAKCDefinitionLocation.DefiningLine' in data:
            #             del data['HAKCDefinitionLocation.DefiningLine']
            #
            #     if data["HAKCSymbol.IsFunction"] is True:
            #         func = HAKCDatabase.__create_object_from_response(HAKCFunction, **data)
            #         functions.add(func)
            #         # print(func.debug_print())
            #     else:
            #         gv = HAKCDatabase.__create_object_from_response(HAKCGlobalVariable, **data)
            #         # print(gv.debug_print())
            #         gvs.add(gv)

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

        # print(f"Returning {len(functions)} functions")
        symbols = functions.union(gvs)
        logger.fatal(f"Returning symbols {symbols}")
        return symbols

    def get_symbol_hash(self, Name, DefiningFile, DefiningLine):

        cmd = f"""
            MATCH (sym:{HAKCSymbol.get_table_name()})-[{HAKCSymbol.relation_definition_location}]->(dl:{HAKCDefinitionLocation.get_table_name()})
            WHERE sym.Name=$Name AND dl.DefiningFile=$DefiningFile AND dl.DefiningLine=$DefiningLine
            RETURN DISTINCT sym.{HAKCSymbol.get_primary_key()}
        """
        response = self.execute_prepared_stmt(cmd, Name=Name, DefiningFile=DefiningFile, DefiningLine=DefiningLine)
        ret = response.get_as_df()
        if(f"sym.{HAKCSymbol.get_primary_key()}" in ret) and (len(ret[f"sym.{HAKCSymbol.get_primary_key()}"]) > 0):
            symbol_hash = ret[f"sym.{HAKCSymbol.get_primary_key()}"][0]
            logger.debug(f"Queried symbol hash: {symbol_hash} from (Name, DefiningFile, DefiningLine) = ({Name}, {DefiningFile}, {DefiningLine})")
            return int(symbol_hash)
        else:
            logger.debug(f"Queried symbol hash from (Name, DefiningFile, DefiningLine) = ({Name}, {DefiningFile}, {DefiningLine}), but could not find symbol hash!")
            return None

    def set_compartment_id_by_symbol(self, _symbol : HAKCSymbol, new_compartment_id: int):

        # delete old relationship between symbol and compartment
        # Note: Assumes that the new compartment id exists in the database, which is true for the time being
        cmd = f"""
        MATCH (sym:{HAKCSymbol.get_table_name()})-[:{HAKCSymbol.relation_division}]->(div:{HAKCDivision.get_table_name()})-[old_edge:{HAKCDivision.relation_compartment}]->(comp:{HAKCCompartment.get_table_name()})
        WHERE sym.{HAKCSymbol.get_primary_key()} = $symbol_hash
        DELETE old_edge
        RETURN comp.CompartmentID
        """
        response = self.execute_prepared_stmt(cmd, symbol_hash=hash(_symbol))
        ret = response.get_as_df()
        logger.debug(ret)
        cmd = f"""
        MATCH (sym)-[:{HAKCSymbol.relation_division}]->(div:{HAKCDivision.get_table_name()}), (comp:{HAKCCompartment.get_table_name()})
        WHERE sym.{HAKCSymbol.get_primary_key()} = $symbol_hash AND comp.CompartmentID = $compartment_id
        CREATE (div)-[new_edge:{HAKCDivision.relation_compartment}]->(comp)
        RETURN comp.CompartmentID
        """
        response = self.execute_prepared_stmt(cmd, symbol_hash=int(_symbol.get_computed_hash()), compartment_id=new_compartment_id)
        ret = response.get_as_df()
        logger.debug(ret)
        return ret["comp.CompartmentID"][0]
