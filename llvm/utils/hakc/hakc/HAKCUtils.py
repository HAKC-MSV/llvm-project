import io
import itertools
import logging
import os
import pstats
import shutil
from typing import cast

from .HAKCLogger import HAKCLogger
from .HAKCObjects import HAKCSymbol, HAKCFunction

logging.setLoggerClass(HAKCLogger)

logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc.utils'))

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

def delete_database(db_dir: str = ''):
    if not db_dir:
        raise RuntimeError(f"Trying to delete database but db_dir is empty!")
    logger.error(f'Removing existing database at {db_dir}')
    if os.path.exists(db_dir) and os.path.isdir(db_dir):
        shutil.rmtree(db_dir)

def copy_database(db_in: str, db_out: str):
    os.makedirs(db_out, exist_ok=True)
    logger.info(f'Copying database from {db_in} to {db_out}')
    shutil.copytree(db_in, db_out, dirs_exist_ok=True)

