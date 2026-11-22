# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 GuardRail-ROS contributors
"""Privacy-by-design visitor event handling (requirements GR-PRIV-*).

Pure standard-library module so it is testable without ROS or network access.
Rules enforced here:
  * No raw biometric/image payloads leave the robot (GR-PRIV-001).
  * Identifiers are pseudonymised with a keyed HMAC (GR-PRIV-002).
  * Events expire after a retention TTL (GR-PRIV-003).
  * Every accepted/rejected event is audit-logged without personal data (GR-PRIV-004).
"""

from __future__ import annotations

import hashlib
import hmac
from collections.abc import Callable
from dataclasses import dataclass, field
from typing import Any

# Fields that must never be forwarded to enterprise systems.
FORBIDDEN_FIELDS = frozenset({"image", "frame", "face_embedding", "raw", "pointcloud"})
ALLOWED_FIELDS = frozenset({"visitor_ref", "zone", "timestamp_s", "event_type"})
ALLOWED_EVENT_TYPES = frozenset({"arrived", "guided", "departed"})
MIN_KEY_BYTES = 32


class PrivacyViolation(ValueError):
    """Raised when an event would violate a privacy rule."""


@dataclass(frozen=True)
class AuditRecord:
    timestamp_s: float
    outcome: str  # "accepted" | "rejected"
    reason: str


@dataclass
class VisitorEventStore:
    """In-memory store with TTL retention and an audit trail."""

    hmac_key: bytes
    retention_s: float = 24 * 3600.0
    clock: Callable[[], float] = field(default=lambda: 0.0)
    _events: list[dict[str, Any]] = field(default_factory=list, init=False)
    audit_log: list[AuditRecord] = field(default_factory=list, init=False)

    def __post_init__(self) -> None:
        if len(self.hmac_key) < MIN_KEY_BYTES:
            raise ValueError(f"hmac_key must be at least {MIN_KEY_BYTES} bytes")
        if self.retention_s <= 0:
            raise ValueError("retention_s must be positive")

    def pseudonymise(self, raw_id: str) -> str:
        digest = hmac.new(self.hmac_key, raw_id.encode("utf-8"), hashlib.sha256)
        return digest.hexdigest()[:32]

    def ingest(self, event: dict[str, Any]) -> dict[str, Any]:
        """Validate, pseudonymise and store an event. Returns the stored copy."""
        now = self.clock()
        try:
            stored = self._validate_and_transform(event)
        except PrivacyViolation as exc:
            self.audit_log.append(AuditRecord(now, "rejected", str(exc)))
            raise
        self._events.append(stored)
        self.audit_log.append(AuditRecord(now, "accepted", stored["event_type"]))
        return dict(stored)

    def _validate_and_transform(self, event: dict[str, Any]) -> dict[str, Any]:
        present_forbidden = FORBIDDEN_FIELDS & set(event)
        if present_forbidden:
            raise PrivacyViolation(
                "forbidden fields present: " + ",".join(sorted(present_forbidden))
            )
        unknown = set(event) - ALLOWED_FIELDS - {"visitor_id"}
        if unknown:
            raise PrivacyViolation("unknown fields: " + ",".join(sorted(unknown)))
        if event.get("event_type") not in ALLOWED_EVENT_TYPES:
            raise PrivacyViolation("invalid event_type")
        visitor_id = event.get("visitor_id")
        if not isinstance(visitor_id, str) or not visitor_id:
            raise PrivacyViolation("visitor_id missing")
        ts = event.get("timestamp_s")
        if isinstance(ts, bool) or not isinstance(ts, (int, float)) or ts != ts:
            raise PrivacyViolation("timestamp_s invalid")
        zone = event.get("zone", "")
        if not isinstance(zone, str):
            raise PrivacyViolation("zone invalid")
        return {
            "visitor_ref": self.pseudonymise(visitor_id),
            "zone": zone,
            "timestamp_s": float(ts),
            "event_type": event["event_type"],
        }

    def purge_expired(self) -> int:
        """Delete events older than the retention TTL. Returns number removed."""
        cutoff = self.clock() - self.retention_s
        before = len(self._events)
        self._events = [e for e in self._events if e["timestamp_s"] >= cutoff]
        removed = before - len(self._events)
        self.audit_log.append(AuditRecord(self.clock(), "accepted", f"purged:{removed}"))
        return removed

    def __len__(self) -> int:
        return len(self._events)
