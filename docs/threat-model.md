# Threat model (STRIDE-lite)

| Asset | Threat | Mitigation |
|---|---|---|
| Visitor identity | Disclosure via logs/exports | Keyed HMAC pseudonyms, audit log without PII (GR-PRIV-002/004) |
| Camera frames | Exfiltration | Bridge rejects image/embedding fields (GR-PRIV-001) |
| `safety_state` topic | Spoofing / tampering on the DDS graph | Enable SROS2 (signed + encrypted topics) in deployment; least-privilege governance |
| HMAC key | Weak or leaked key | ≥ 32-byte key enforced; load from secret store, never from the repo |
| Supply chain | Malicious dependency | Pinned versions, SBOM artifact, Dependabot, Scorecard |
| Supervisor input | Malformed / poisoned values | Input validation → SAFE_STOP (GR-SUP-002) |

Out of scope: physical tampering, OS hardening, real OIDC integration (mocked).
