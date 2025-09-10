import io
import itertools
import logging
import pstats
from typing import cast

from .HAKCLogger import HAKCLogger
from .HAKCObjects import HAKCSymbol, HAKCFunction

logging.setLoggerClass(HAKCLogger)

logger: HAKCLogger = cast(HAKCLogger, logging.getLogger('hakc-utils'))


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

