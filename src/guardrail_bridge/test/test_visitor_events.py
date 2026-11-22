# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 GuardRail-ROS contributors
import unittest

from guardrail_bridge.visitor_events import PrivacyViolation, VisitorEventStore

KEY = b"k" * 32


def make_store(now=None, **kw):
    if now is None:
        now = [1000.0]
    return VisitorEventStore(hmac_key=KEY, clock=lambda: now[0], **kw), now


def good(**over):
    e = {"visitor_id": "alice@example.com", "zone": "lobby",
         "timestamp_s": 1000.0, "event_type": "arrived"}
    e.update(over)
    return e


class VisitorEventTests(unittest.TestCase):
    def test_pseudonymises_and_drops_raw_id(self):  # GR-PRIV-002
        s, _ = make_store()
        out = s.ingest(good())
        self.assertNotIn("visitor_id", out)
        self.assertNotIn("alice", str(out))
        self.assertEqual(out["visitor_ref"], s.pseudonymise("alice@example.com"))
        self.assertEqual(len(out["visitor_ref"]), 32)

    def test_pseudonym_depends_on_key(self):
        a = VisitorEventStore(hmac_key=b"a" * 32).pseudonymise("x")
        b = VisitorEventStore(hmac_key=b"b" * 32).pseudonymise("x")
        self.assertNotEqual(a, b)

    def test_rejects_raw_image(self):  # GR-PRIV-001
        s, _ = make_store()
        with self.assertRaises(PrivacyViolation):
            s.ingest(good(image=b"\x00\x01"))
        self.assertEqual(len(s), 0)

    def test_rejects_unknown_field_and_bad_type(self):
        s, _ = make_store()
        with self.assertRaises(PrivacyViolation):
            s.ingest(good(extra="x"))
        with self.assertRaises(PrivacyViolation):
            s.ingest(good(event_type="hacked"))
        with self.assertRaises(PrivacyViolation):
            s.ingest(good(timestamp_s=float("nan")))
        with self.assertRaises(PrivacyViolation):
            s.ingest(good(timestamp_s=True))

    def test_retention_purge(self):  # GR-PRIV-003
        s, now = make_store(retention_s=100.0)
        s.ingest(good(timestamp_s=1000.0))
        now[0] = 1050.0
        self.assertEqual(s.purge_expired(), 0)
        now[0] = 1101.0
        self.assertEqual(s.purge_expired(), 1)
        self.assertEqual(len(s), 0)

    def test_audit_log_has_no_personal_data(self):  # GR-PRIV-004
        s, _ = make_store()
        s.ingest(good())
        with self.assertRaises(PrivacyViolation):
            s.ingest(good(image=b"x"))
        text = " ".join(f"{r.outcome} {r.reason}" for r in s.audit_log)
        self.assertNotIn("alice", text)
        self.assertEqual([r.outcome for r in s.audit_log], ["accepted", "rejected"])

    def test_weak_key_rejected(self):
        with self.assertRaises(ValueError):
            VisitorEventStore(hmac_key=b"short")


if __name__ == "__main__":
    unittest.main()
