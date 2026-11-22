# Contributing

- Commits follow [Conventional Commits](https://www.conventionalcommits.org/); versions follow
  [SemVer](https://semver.org/); changes are logged in `CHANGELOG.md`.
- Sign off commits (DCO): `git commit -s`.
- Every new requirement gets an ID in `docs/requirements.md` and a test that cites it.
- Core rules (`guardrail_core`): no exceptions, no RTTI, no heap allocation after construction,
  no ROS headers, `-Werror` clean, explicit casts. Deviations go in `docs/deviations.md`.
- Before opening a PR: build + `ctest`, `clang-format`, `clang-tidy`, `ruff`, `mypy`.
- Architecture decisions: add an ADR under `docs/adr/`.
