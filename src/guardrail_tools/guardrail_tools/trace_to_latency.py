# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 GuardRail-ROS contributors
"""Convert trace events into per-message latencies for ``latency_gate`` (GR-LAT-004).

Pairs a *start* event and an *end* event that share a key (e.g. a message pointer or
sequence id) and emits ``end - start`` in integer nanoseconds, one per line.

Input formats
-------------
``csv``          header ``stamp_ns,stage,key``; stages are matched by name. Verified by tests.
``babeltrace2``  text output of ``babeltrace2`` (LTTng/ros2_tracing). Tested on synthetic
                 samples and on an excerpt of one real ROS 2 Jazzy capture (see
                 docs/verification-status.md); other distros/tracepoints are unverified.

Exit codes: 0 ok, 2 on malformed input / negative latency / no pairs (strict by default).
"""

from __future__ import annotations

import argparse
import csv
import re
import sys
from collections.abc import Iterable, Iterator
from dataclasses import dataclass
from typing import TextIO

NS_PER_S = 1_000_000_000
EXIT_OK = 0
EXIT_ERROR = 2


class TraceError(ValueError):
    """Raised for malformed trace input."""


@dataclass(frozen=True)
class Event:
    stamp_ns: int
    stage: str
    key: str


@dataclass
class PairResult:
    latencies_ns: list[int]
    unmatched_ends: int
    unmatched_starts: int
    duplicate_starts: int


# ----------------------------------------------------------------------------- parsers
def parse_csv(lines: Iterable[str]) -> Iterator[Event]:
    reader = csv.reader(lines)
    header = next(reader, None)
    if header is None or [h.strip() for h in header] != ["stamp_ns", "stage", "key"]:
        raise TraceError("csv header must be: stamp_ns,stage,key")
    for lineno, row in enumerate(reader, start=2):
        if not row:
            continue
        if len(row) != 3:
            raise TraceError(f"line {lineno}: expected 3 columns")
        try:
            stamp = int(row[0])
        except ValueError as exc:
            raise TraceError(f"line {lineno}: bad stamp_ns {row[0]!r}") from exc
        if stamp < 0:
            raise TraceError(f"line {lineno}: negative stamp_ns")
        stage, key = row[1].strip(), row[2].strip()
        if not stage or not key:
            raise TraceError(f"line {lineno}: empty stage or key")
        yield Event(stamp, stage, key)


_BT_TS = re.compile(r"^\[(?:(\d+):(\d{2}):(\d{2})|(\d+))\.(\d{1,9})\]")
_BT_EVENT = re.compile(r"^\[[^\]]*\]\s+(?:\(\+[^)]*\)\s+)?\S+\s+([A-Za-z0-9_]+:[A-Za-z0-9_:]+):")
_BT_FIELD = re.compile(r"([A-Za-z_][A-Za-z0-9_]*) = ([^,}\s]+)")


def _bt_stamp_ns(line: str, lineno: int) -> int:
    m = _BT_TS.match(line)
    if not m:
        raise TraceError(f"line {lineno}: cannot parse timestamp")
    hh, mm, ss, secs, frac = m.groups()
    nanos = int(frac.ljust(9, "0"))
    if hh is not None:
        whole = int(hh) * 3600 + int(mm) * 60 + int(ss)
    else:
        whole = int(secs)
    return whole * NS_PER_S + nanos


def parse_babeltrace2(lines: Iterable[str], key_field: str) -> Iterator[Event]:
    for lineno, raw in enumerate(lines, start=1):
        line = raw.rstrip("\n")
        if not line.strip():
            continue
        ev = _BT_EVENT.match(line)
        if not ev:
            raise TraceError(f"line {lineno}: not a babeltrace2 event line")
        stamp = _bt_stamp_ns(line, lineno)
        fields = dict(_BT_FIELD.findall(line))
        if key_field not in fields:
            continue  # event of the right provider but without our key: ignore
        yield Event(stamp, ev.group(1), fields[key_field])


# ----------------------------------------------------------------------------- pairing
def pair_latencies(events: Iterable[Event], start_stage: str, end_stage: str) -> PairResult:
    open_starts: dict[str, int] = {}
    latencies: list[int] = []
    unmatched_ends = 0
    duplicates = 0
    for e in events:
        if e.stage == start_stage:
            if e.key in open_starts:
                duplicates += 1  # keep the earliest start: conservative (larger latency)
            else:
                open_starts[e.key] = e.stamp_ns
        elif e.stage == end_stage:
            start = open_starts.pop(e.key, None)
            if start is None:
                unmatched_ends += 1
            else:
                latencies.append(e.stamp_ns - start)
    return PairResult(latencies, unmatched_ends, len(open_starts), duplicates)


# ----------------------------------------------------------------------------- CLI
def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("--input", required=True)
    p.add_argument("--format", choices=["csv", "babeltrace2"], default="csv")
    p.add_argument("--start-stage", required=True)
    p.add_argument("--end-stage", required=True)
    p.add_argument("--key-field", default="message", help="babeltrace2: field used as pairing key")
    p.add_argument("--output", help="default: stdout")
    p.add_argument(
        "--allow-unmatched", action="store_true", help="do not fail on unmatched start/end events"
    )
    return p


def run(args: argparse.Namespace, out: TextIO, err: TextIO) -> int:
    try:
        with open(args.input, encoding="utf-8") as fh:
            lines = fh.readlines()
        if args.format == "csv":
            events = list(parse_csv(lines))
        else:
            events = list(parse_babeltrace2(lines, args.key_field))
        res = pair_latencies(events, args.start_stage, args.end_stage)
    except (OSError, TraceError) as exc:
        err.write(f"trace_to_latency: {exc}\n")
        return EXIT_ERROR

    err.write(
        f"pairs={len(res.latencies_ns)} unmatched_ends={res.unmatched_ends} "
        f"unmatched_starts={res.unmatched_starts} duplicate_starts={res.duplicate_starts}\n"
    )
    if not res.latencies_ns:
        err.write("trace_to_latency: no start/end pairs found\n")
        return EXIT_ERROR
    if any(v < 0 for v in res.latencies_ns):
        err.write("trace_to_latency: negative latency (end before start); check clocks/stages\n")
        return EXIT_ERROR
    if not args.allow_unmatched and (
        res.unmatched_ends or res.unmatched_starts or res.duplicate_starts
    ):
        err.write("trace_to_latency: unmatched/duplicate events (use --allow-unmatched)\n")
        return EXIT_ERROR

    out.write("".join(f"{v}\n" for v in res.latencies_ns))
    return EXIT_OK


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if args.output:
        with open(args.output, "w", encoding="utf-8") as fh:
            return run(args, fh, sys.stderr)
    return run(args, sys.stdout, sys.stderr)


if __name__ == "__main__":
    raise SystemExit(main())
