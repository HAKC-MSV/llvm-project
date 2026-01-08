import io
import itertools
import logging
import pstats
from typing import cast

from .HAKCDatabase import HAKCDatabase
from .HAKCLogger import HAKCLogger
from .HAKCObjects import HAKCSymbol, HAKCFunction

logging.setLoggerClass(HAKCLogger)

logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc.static-analysis'))

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
    if len(tail_hash_types) == 0:
        raise RuntimeError("Unable to get dag computation edges!")
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
