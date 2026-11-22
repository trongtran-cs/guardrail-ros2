#!/usr/bin/env bash
# Run cppcheck directly over all C++ packages.
# Why: ament_cppcheck skips itself on cppcheck 2.13.0 (Ubuntu 24.04), so the
# colcon lint run never actually analyses anything.
set -euo pipefail
cd "$(dirname "$0")/.."
exec cppcheck -q --enable=warning,style,performance --error-exitcode=1 --inline-suppr \
  --suppressions-list=scripts/cppcheck-suppressions.txt \
  --suppress=missingIncludeSystem \
  -I src/guardrail_core/include -I src/guardrail_detector_node/include \
  --template='{file}:{line} [{severity}/{id}] {message}' \
  src/guardrail_core src/guardrail_detector_node src/guardrail_supervisor_node
