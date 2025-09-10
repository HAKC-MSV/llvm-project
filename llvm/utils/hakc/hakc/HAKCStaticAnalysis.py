import concurrent.futures
import io
import itertools
import logging
import pstats
import time
from typing import cast

import networkx as nx
import yaml

from .HAKCDatabase import HAKCDatabase
from .HAKCLogger import HAKCLogger
from .HAKCObjects import HAKCSymbol, HAKCFunction, HAKCCompartment, HAKCDivision

logging.setLoggerClass(HAKCLogger)

logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc-static-analysis'))


mp_conn: HAKCDatabase | None = None

def init_mp_database(db_dir: str):
    global mp_conn
    mp_conn = HAKCDatabase(db_dir, read_only=True)

def compute_dag_edges_for_symbol(symbol_hashes):
    global mp_conn
    conn = mp_conn
    results = list()
    for symbol_hash in symbol_hashes:
        try:
            for t in compute_dag_edges_for_symbol_with_conn(conn, symbol_hash):
                results.append(t)
        except Exception as e:
            raise RuntimeError(f'compute_dag_edges_for_symbol failed for {symbol_hash}: {str(e)}')

    return results

def compute_dag_edges_for_symbol_with_conn(conn: HAKCDatabase, symbol_hash: int):
    results = list()
    tail_hash_types = conn.get_dag_computation_edges(symbol_hash)
    # logger.error(f"Found tail hash: {tail_hash_types}")
    dag_info = dict()

    for dag_edge_type, tail_hashes in tail_hash_types.items():
        for tail_hash in tail_hashes:
            if tail_hash not in dag_info:
                dag_info[tail_hash] = dict()
            dag_info[tail_hash][dag_edge_type] = True
    for tail_hash, tail_info in dag_info.items():
        dag_weight = compute_dag_edge_weight(**tail_info)
        if dag_weight > 0:
            results.append((symbol_hash, tail_hash, dag_weight))

    return results


# def create_compartmentalization_single_thread(files: set[str], conn: HAKCDatabase) -> HAKCCompartmentalization:
#     compartmentalization = HAKCCompartmentalization()
#     for filename in logger.progress_bar(iterable=sorted(files), desc='Parsing YAML'):
#         subG = HAKCCompartmentalization.parse_yaml(filename)
#         if subG:
#             compartmentalization = nx.compose(compartmentalization, subG)
#         else:
#             logger.error(f"Loaded compartmentalization is None!")
#     logger.info(f'Adding compartmentalization')
#     compartmentalization.add_default_compartmentalization(conn)
#     compartmentalization.persist_to_database(conn, create_schema=True)
#     return compartmentalization

# def create_dag_single_thread(files: set[str], conn: HAKCDatabase) -> HAKCCompartmentalization:
#     # TODO: update this
#     compartmentalization = create_compartmentalization_single_thread(files, conn)
#     dag_edges = dict()
#     dag_edges_added = 0
#     symbols = compartmentalization.get_symbols()
#     for symbol in logger.progress_bar(iterable=symbols, desc='DAG Edge computation'):
#         for (head_hash, tail_hash, dag_edge_weight) in compute_dag_edges_for_symbol_with_conn(conn, hash(symbol)):
#             if head_hash not in dag_edges:
#                 dag_edges[head_hash] = dict()
#             dag_edges[head_hash][tail_hash] = dag_edge_weight
#             tail_symbol = compartmentalization.get_symbol_by_hash(tail_hash)
#             compartmentalization.add_dag_edge(symbol, tail_symbol, dag_edge_weight=dag_edge_weight)
#             dag_edges_added += 1
#     logger.info(f'Adding {dag_edges_added} DAG edges to compartmentalization')
#     conn.persist_dag_edges(dag_edges)
#     logger.info(f'Adding compartmentalization')
#     # compartmentalization.add_default_compartmentalization(conn)
#     return compartmentalization
#
#
# def adjust_compartmentalization(adjust_path: str, db_dir: str):
#
#     adjustment = None
#     with open(adjust_path, 'r') as f:
#         # TODO: does CLoader work here?
#         adjustment = yaml.load(f, Loader=yaml.Loader)
#
#     if not adjustment:
#         raise RuntimeError(f"Unable to load Adjustments from {adjust_path}")
#
#     logger.info(f'Adjusting compartmentalization based on {adjust_path}')
#     conn = HAKCDatabase(db_dir)
#
#     symbols = conn.get_symbols()
#     assert len(symbols) != 0, f"No symbols were returned from database!"
#     compartmentalization = HAKCCompartmentalization()
#     nec_division = None
#     no_enforcement_compartment = None
#     if adjustment.add_no_enforcement_compartment:
#         nec_division = HAKCDivision(HAKCCompartmentalization.no_enforcement_division)
#         no_enforcement_compartment = HAKCCompartment(HAKCCompartmentalization.no_enforcement_compartment_id)
#         compartmentalization.add_division(nec_division, no_enforcement_compartment)
#
#     for symbol in logger.progress_bar(iterable=symbols, desc='Adjusting Compartmentalization'):
#         compartmentalization.add_symbol(symbol, already_persisted=True)
#         adjustments = adjustment.get_adjusted_division_and_compartment(symbol.definition_location.defining_file if symbol.definition_location else None)
#         adjusted_division = None
#         adjusted_compartment = None
#         if adjustments is None:
#             if adjustment.add_no_enforcement_compartment:
#                 adjusted_division = nec_division
#                 adjusted_compartment = no_enforcement_compartment
#             else:
#                 adjustments = conn.get_division_id_compartment_id_from_symbol(symbol)
#                 if adjustments is None:
#                     logger.error(f'Could not find original division for {symbol}')
#                     continue
#
#         if adjustments is not None:
#             adjusted_division, adjusted_compartment = adjustments
#
#         if adjusted_division is not None:
#             if adjusted_division != nec_division:
#                 logger.info(f'{symbol} is moving to {adjusted_division}')
#             else:
#                 logger.debug(f'{symbol} is moving to NEC {adjusted_division}')
#             compartmentalization.set_symbol_division_by_object(symbol, adjusted_division, adjusted_compartment)
#         else:
#             logger.info(f'{symbol} is unchanged')
#
#     logger.info(f'Removing existing compartments')
#     conn.delete_all_compartments()
#     compartmentalization.persist_to_database(conn)
#     logger.info(f'Done adjusting compartmentalization')
#     logger.info(f'Compartmentalization now has {len(conn.get_all_divisions())} divisions across {len(conn.get_all_compartments())} compartments')
#     conn.close()
#
# def create_compartmentalization_multithread(files: set[str], core_count: int, conn: HAKCDatabase) -> HAKCCompartmentalization:
#     import traceback
#     logger.info(f'Starting multiprocess HAKCCompartmentalization creation using {core_count} cores')
#     with concurrent.futures.ProcessPoolExecutor(max_workers=core_count) as executor:
#         futures_to_files = {}
#         for file in logger.progress_bar(iterable=sorted(files), desc="YAML parsing scheduling"):
#             futures_to_files[executor.submit(HAKCCompartmentalization.parse_yaml, file)] = file
#         compartmentalization = HAKCCompartmentalization()
#         compartmentalization.create_schema(conn)
#         with logger.progress_bar(total=len(files), desc='YAML parsing results') as pbar:
#             for future in list(concurrent.futures.as_completed(futures_to_files)):
#                 pbar.update(1)
#                 file = futures_to_files[future]
#                 try:
#                     subG: HAKCCompartmentalization = future.result()
#                     if subG:
#                         compartmentalization.update(edges=subG.edges(data=True, keys=True), nodes=subG.nodes(data=True))
#                     else:
#                         logger.error(f"Loaded compartmentalization is None!")
#
#                 except Exception as e:
#                     traceback.print_exc()
#                     logger.error(f'Error parsing {file}: {str(e)} with {subG}')
#
#     logger.info(f'Adding compartmentalization')
#     compartmentalization.add_default_compartmentalization(conn)
#     compartmentalization.persist_to_database(conn)
#     logger.info(f'Total symbols {len(conn.get_symbols())}')
#     return compartmentalization
#
# def create_dag_multithread(compartmentalization: HAKCCompartmentalization, core_count: int, db_dir: str) -> HAKCCompartmentalization:
#     # TODO: We might be able to rewrite this without depending on the compartmentalization object
#     logger.info(f'Starting multiprocess DAG creation using {core_count} cores from {compartmentalization}')
#     symbol_hashes = compartmentalization.get_symbol_hashes()
#     batch_size = 100
#     dag_edges_added = 0
#     with concurrent.futures.ProcessPoolExecutor(max_workers=core_count, initializer=init_mp_database,
#                                                 initargs=(db_dir,)) as executor:
#         futures = list()
#         with logger.progress_bar(total=int(len(symbol_hashes) / batch_size) + 1,
#                                  desc='DAG Edge computation scheduling') as pbar:
#             try:
#                 for symbol_hash_batch in batched(symbol_hashes.keys(), batch_size):
#                     future = executor.submit(compute_dag_edges_for_symbol, symbol_hash_batch)
#                     futures.append(future)
#                     pbar.update(1)
#             except Exception as e:
#                 logger.error(f'Error submitting tasks: {str(e)}')
#                 executor.shutdown(wait=False, cancel_futures=True)
#                 raise e
#
#         with logger.progress_bar(total=len(futures), desc='DAG Edge addition') as pbar:
#             try:
#                 dag_edges = dict()
#                 for future in concurrent.futures.as_completed(futures):
#                     pbar.update(1)
#                     try:
#                         edge_weights = future.result()
#                         # logger.error(f"edge weights: {edge_weights}")
#                         for (head_hash, tail_hash, dag_edge_weight) in edge_weights:
#                             if head_hash not in dag_edges:
#                                 dag_edges[head_hash] = dict()
#                             head_symbol = symbol_hashes[head_hash]
#                             tail_symbol = symbol_hashes[tail_hash]
#                             compartmentalization.add_dag_edge(head_symbol, tail_symbol, dag_edge_weight=dag_edge_weight)
#                             dag_edges[head_hash][tail_hash] = dag_edge_weight
#                             dag_edges_added += 1
#                     except Exception as e:
#                         logger.error(f'Error computing DAG edge: {str(e)}')
#             except KeyboardInterrupt as ki:
#                 logger.info(f'Stopping edge computation')
#                 executor.shutdown(wait=False, cancel_futures=True)
#                 raise ki
#
#     logger.info(f'Adding {dag_edges_added} DAG edges to compartmentalization')
#     conn = HAKCDatabase(db_dir, max_num_threads=core_count)
#     conn.persist_dag_edges(dag_edges)
#     conn.close()
#     return compartmentalization
#
# def construct_dag(compartmentalization, core_count: int, db_dir: str) -> HAKCCompartmentalization:
#     core_count = max(1, core_count)
#     logger.info(f'Starting DAG construction from {compartmentalization} with {core_count} cores')
#     start = time.time()
#     dag_compartmentalization = create_dag_multithread(compartmentalization, core_count, db_dir)
#     end = time.time()
#
#     logger.info(f'Finished creating DAG {dag_compartmentalization}')
#     logger.info(f'    Total Time: {end - start} seconds')
#     return dag_compartmentalization

def print_edge_data(G):
    for edge in G.edges:
        print(f"Edge: {edge} -> {G.get_edge_data(edge[0], edge[1])}")

def output_profile_stats(profile):
    s = io.StringIO()
    ps = pstats.Stats(profile, stream=s).sort_stats('tottime')
    ps.print_stats()
    logger.info(s.getvalue())


def compute_dag_edge_weight(**kwargs) -> int:
    edge_weight = 0

    if kwargs.get(HAKCFunction.relation_indirect_calls, False):
        edge_weight += 1

    if kwargs.get(HAKCFunction.relation_direct_calls, False):
        edge_weight += 1

    if kwargs.get(HAKCSymbol.relation_symbol, False):
        edge_weight += 1

    return edge_weight


def batched(iterable, n):
    if n < 1:
        raise ValueError('n must be at least one')
    iterator = iter(iterable)
    while batch := tuple(itertools.islice(iterator, n)):
        yield batch

