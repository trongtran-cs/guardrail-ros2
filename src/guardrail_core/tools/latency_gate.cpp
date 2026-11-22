// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 GuardRail-ROS contributors
//
// CI latency gate. Reads one latency (nanoseconds, integer) per line and fails
// (exit 1) if p99 / jitter / max exceed the budgets. Exit 2 = usage/IO error.
// Input is produced by tools/trace_to_latency.py from ros2_tracing (LTTng) output
// or by the simulation harness. Requirements GR-LAT-003.
//
// Usage: latency_gate --input FILE --p99-ms X [--jitter-ms Y] [--max-ms Z]

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "guardrail_core/latency_histogram.hpp"

namespace
{

constexpr int kExitPass = 0;
constexpr int kExitViolation = 1;
constexpr int kExitError = 2;
constexpr std::int64_t kNsPerMs = 1'000'000;
constexpr std::int64_t kMinBucketNs = 100'000;  // 100 us floor; widened for large ranges

struct Budgets
{
  double p99_ms{-1.0};
  double jitter_ms{-1.0};
  double max_ms{-1.0};
};

bool parseDouble(const char * text, double & out)
{
  errno = 0;
  char * end = nullptr;
  const double v = std::strtod(text, &end);
  if ((errno != 0) || (end == text) || (*end != '\0') || !(v >= 0.0)) {
    return false;
  }
  out = v;
  return true;
}

void usage()
{
  std::cerr << "usage: latency_gate --input FILE --p99-ms X [--jitter-ms Y] [--max-ms Z]\n";
}

}  // namespace

int main(int argc, char ** argv)
{
  std::string input;
  Budgets b;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    const bool has_value = (i + 1) < argc;
    if (arg == "--input" && has_value) {
      input = argv[++i];
    } else if (arg == "--p99-ms" && has_value) {
      if (!parseDouble(argv[++i], b.p99_ms)) {usage(); return kExitError;}
    } else if (arg == "--jitter-ms" && has_value) {
      if (!parseDouble(argv[++i], b.jitter_ms)) {usage(); return kExitError;}
    } else if (arg == "--max-ms" && has_value) {
      if (!parseDouble(argv[++i], b.max_ms)) {usage(); return kExitError;}
    } else {
      usage();
      return kExitError;
    }
  }
  if (input.empty() || (b.p99_ms < 0.0)) {
    usage();
    return kExitError;
  }

  std::ifstream file(input);
  if (!file) {
    std::cerr << "latency_gate: cannot open " << input << "\n";
    return kExitError;
  }

  // Read all samples first so the bucket width can cover the whole range. A fixed narrow
  // width collapses every sample above 25.6 ms into the overflow bucket and makes p99
  // meaningless for slower pipelines (found with a real capture, see GR-LAT-005).
  std::vector<std::int64_t> samples;
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty()) {
      continue;
    }
    errno = 0;
    char * end = nullptr;
    const long long v = std::strtoll(line.c_str(), &end, 10);
    if ((errno != 0) || (end == line.c_str()) || (*end != '\0')) {
      std::cerr << "latency_gate: malformed line: " << line << "\n";
      return kExitError;
    }
    samples.push_back(static_cast<std::int64_t>(v));
  }

  std::int64_t max_sample = 0;
  for (const std::int64_t v : samples) {
    max_sample = std::max(max_sample, v);
  }
  const std::int64_t buckets = static_cast<std::int64_t>(guardrail::LatencyHistogram::kBuckets);
  const std::int64_t needed = (max_sample / (buckets - 1)) + 1;  // ceil-ish: max fits in-range
  const std::int64_t bucket_ns = std::max(kMinBucketNs, needed);

  guardrail::LatencyHistogram hist(bucket_ns);
  for (const std::int64_t v : samples) {
    hist.record(v);
  }

  if (hist.count() == 0U || hist.rejected() != 0U) {
    std::cerr << "latency_gate: no valid samples or negative samples present\n";
    return kExitError;
  }

  const double p99_ms = static_cast<double>(hist.percentile_ns(99.0)) / static_cast<double>(kNsPerMs);
  const double jit_ms = static_cast<double>(hist.jitter_ns()) / static_cast<double>(kNsPerMs);
  const double max_ms = static_cast<double>(hist.max_ns()) / static_cast<double>(kNsPerMs);

  std::printf("samples=%llu p99_ms=%.3f jitter_ms=%.3f max_ms=%.3f (bucket_us=%.0f)\n",
    static_cast<unsigned long long>(hist.count()), p99_ms, jit_ms, max_ms,
    static_cast<double>(bucket_ns) / 1000.0);

  bool ok = true;
  if (p99_ms > b.p99_ms) {
    std::printf("FAIL p99 %.3f ms > budget %.3f ms\n", p99_ms, b.p99_ms);
    ok = false;
  }
  if ((b.jitter_ms >= 0.0) && (jit_ms > b.jitter_ms)) {
    std::printf("FAIL jitter %.3f ms > budget %.3f ms\n", jit_ms, b.jitter_ms);
    ok = false;
  }
  if ((b.max_ms >= 0.0) && (max_ms > b.max_ms)) {
    std::printf("FAIL max %.3f ms > budget %.3f ms\n", max_ms, b.max_ms);
    ok = false;
  }
  if (ok) {
    std::printf("PASS\n");
  }
  return ok ? kExitPass : kExitViolation;
}
