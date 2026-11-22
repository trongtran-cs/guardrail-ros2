# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 GuardRail-ROS contributors
import io
import tempfile
import unittest
from pathlib import Path

from guardrail_tools import trace_to_latency as t

FIX = Path(__file__).parent / "fixtures"


def run_cli(argv):
    args = t.build_parser().parse_args(argv)
    out, err = io.StringIO(), io.StringIO()
    code = t.run(args, out, err)
    return code, out.getvalue(), err.getvalue()


class CsvTests(unittest.TestCase):
    def test_pairs_and_latencies(self):  # GR-LAT-004
        code, out, _ = run_cli(
            ["--input", str(FIX / "stages.csv"), "--start-stage", "capture",
             "--end-stage", "detect_published"])
        self.assertEqual(code, t.EXIT_OK)
        self.assertEqual(out.split(), ["15000000", "18000000", "24000000"])

    def test_bad_header_rejected(self):
        with self.assertRaises(t.TraceError):
            list(t.parse_csv(["a,b,c\n"]))

    def test_bad_number_and_negative_stamp(self):
        with self.assertRaises(t.TraceError):
            list(t.parse_csv(["stamp_ns,stage,key\n", "x,s,k\n"]))
        with self.assertRaises(t.TraceError):
            list(t.parse_csv(["stamp_ns,stage,key\n", "-1,s,k\n"]))

    def test_negative_latency_is_error(self):
        events = [t.Event(200, "end", "k"), t.Event(100, "start", "k")]
        # end seen before start -> unmatched end, not a negative latency
        res = t.pair_latencies(events, "start", "end")
        self.assertEqual(res.unmatched_ends, 1)
        events2 = [t.Event(200, "start", "k"), t.Event(100, "end", "k")]
        res2 = t.pair_latencies(events2, "start", "end")
        self.assertEqual(res2.latencies_ns, [-100])

    def test_cli_fails_on_negative_latency(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "x.csv"
            p.write_text("stamp_ns,stage,key\n200,s,k\n100,e,k\n")
            code, out, err = run_cli(
                ["--input", str(p), "--start-stage", "s", "--end-stage", "e"])
        self.assertEqual(code, t.EXIT_ERROR)
        self.assertEqual(out, "")
        self.assertIn("negative latency", err)

    def test_unmatched_strict_vs_allowed(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "x.csv"
            p.write_text("stamp_ns,stage,key\n100,s,a\n150,e,a\n200,s,b\n")
            base = ["--input", str(p), "--start-stage", "s", "--end-stage", "e"]
            self.assertEqual(run_cli(base)[0], t.EXIT_ERROR)
            code, out, _ = run_cli(base + ["--allow-unmatched"])
        self.assertEqual(code, t.EXIT_OK)
        self.assertEqual(out.split(), ["50"])

    def test_duplicate_start_keeps_earliest(self):
        ev = [t.Event(100, "s", "k"), t.Event(120, "s", "k"), t.Event(200, "e", "k")]
        res = t.pair_latencies(ev, "s", "e")
        self.assertEqual(res.latencies_ns, [100])
        self.assertEqual(res.duplicate_starts, 1)

    def test_missing_file_and_no_pairs(self):
        code, _, err = run_cli(
            ["--input", "/nonexistent/x.csv", "--start-stage", "s", "--end-stage", "e"])
        self.assertEqual(code, t.EXIT_ERROR)
        self.assertIn("trace_to_latency", err)
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "x.csv"
            p.write_text("stamp_ns,stage,key\n")
            code, _, err = run_cli(
                ["--input", str(p), "--start-stage", "s", "--end-stage", "e"])
        self.assertEqual(code, t.EXIT_ERROR)
        self.assertIn("no start/end pairs", err)


class Babeltrace2Tests(unittest.TestCase):
    """Synthetic samples plus one excerpt of a real Jazzy capture."""

    def test_rclcpp_to_rmw_publish_latency(self):
        code, out, _ = run_cli(
            ["--input", str(FIX / "synthetic_babeltrace2.txt"), "--format", "babeltrace2",
             "--start-stage", "ros2:rclcpp_publish", "--end-stage", "ros2:rmw_publish"])
        self.assertEqual(code, t.EXIT_OK)
        self.assertEqual(out.split(), ["15000000", "27000000"])

    def test_real_capture_jazzy_publish_path(self):
        """Excerpt of a real ros2_tracing capture (Jazzy, aarch64 container, 2026-11-22)."""
        code, out, _ = run_cli(
            ["--input", str(FIX / "real_ros2_trace_jazzy.txt"), "--format", "babeltrace2",
             "--start-stage", "ros2:rclcpp_publish", "--end-stage", "ros2:rmw_publish"])
        self.assertEqual(code, t.EXIT_OK)
        values = [int(v) for v in out.split()]
        self.assertEqual(len(values), 3)
        self.assertTrue(all(0 < v < 1_000_000 for v in values))  # publish path is < 1 ms

    def test_timestamp_forms(self):
        self.assertEqual(
            t._bt_stamp_ns("[00:00:01.5] x", 1), 1_500_000_000)
        self.assertEqual(
            t._bt_stamp_ns("[12.000000001] x", 1), 12_000_000_001)
        with self.assertRaises(t.TraceError):
            t._bt_stamp_ns("garbage", 1)

    def test_non_event_line_rejected(self):
        with self.assertRaises(t.TraceError):
            list(t.parse_babeltrace2(["hello world\n"], "message"))

    def test_events_without_key_are_ignored(self):
        evs = list(t.parse_babeltrace2(
            [(FIX / "synthetic_babeltrace2.txt").read_text().splitlines()[3]], "message"))
        self.assertEqual(evs, [])


if __name__ == "__main__":
    unittest.main()
