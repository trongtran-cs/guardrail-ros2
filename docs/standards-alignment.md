# Standards alignment

**Nothing in this repository is certified or independently assessed.** The wording is
"aligned with" / "inspired by". Paywalled standards were not read in full; verification status
is stated honestly in the last column.

| Standard / practice | What this repo does | Status |
|---|---|---|
| MISRA C++:2023 (absorbs AUTOSAR C++14) | Subset-aligned style in core (no exceptions/RTTI, fixed-size containers, explicit casts, `-Wconversion`); deviations in `docs/deviations.md` | Aligned subset; no MISRA checker run (free tools lack MISRA C++ support) |
| CERT C++ / C++ Core Guidelines | `.clang-tidy` enables `cert-*`, `bugprone-*`, `cppcoreguidelines-*` | Configured; clang-tidy run in CI (not yet run locally) |
| ROS 2 developer guide, REP-103/105 | SI units and frames documented in `.msg` comments | Not re-verified against REP text |
| REP-2004 package quality | Targeting Level 2 (docs, tests, versioning, license) | Self-assessed |
| ASPICE SWE.1–SWE.6 | `docs/requirements.md` traceability matrix | Aligned, not assessed |
| ISO 26262 Part 6 / Part 8 | HARA-lite, fail-safe default, tool-confidence note | Inspired only |
| ISO 21448 SOTIF | Limits acknowledged in HARA | Inspired only |
| ISO/SAE 21434, IEC 62443 | `docs/threat-model.md`, SROS2 guidance | Inspired only |
| EU Cyber Resilience Act | SBOM generation, vulnerability policy (`SECURITY.md`) | Practices only, not a conformity claim |
| SPDX / REUSE | `SPDX-License-Identifier` headers, Apache-2.0 | Headers present |
| SBOM (CycloneDX) | CI job generates SBOM artifact | Defined in CI; not yet run |
| Semantic Versioning, Conventional Commits, Keep a Changelog | `CHANGELOG.md`, `CONTRIBUTING.md` | Adopted |
| OpenSSF Scorecard / SLSA | Workflow stubs with pinned actions guidance | Not yet run |
