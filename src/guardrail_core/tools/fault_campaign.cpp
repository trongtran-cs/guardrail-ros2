// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 GuardRail-ROS contributors
//
// Closed-loop fault campaign: synthetic 30 Hz perception stream -> FaultInjector ->
// Supervisor. Measures detection latency and false alarms per scenario and checks them
// against explicit expectations (GR-FLT-002).
//
// SCOPE: this exercises the *decision logic* with a simulated stream. It is NOT a
// robot-level or Gazebo measurement and must not be reported as one.
//
// Usage: fault_campaign [--scenario NAME|all] [--csv FILE]
// Exit: 0 all expectations met, 1 an expectation failed, 2 usage/IO error.

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "guardrail_core/fault_injector.hpp"
#include "guardrail_core/supervisor.hpp"

namespace
{

using guardrail::FaultConfig;
using guardrail::FaultInjector;
using guardrail::SafetyState;
using guardrail::Sample;
using guardrail::Supervisor;

constexpr std::int64_t kMs = 1'000'000;
constexpr std::int64_t kPeriodNs = 33'333'333;  // 30 Hz
constexpr std::int64_t kDurationNs = 20'000'000'000;
constexpr std::int64_t kOnsetNs = 5'000'000'000;
constexpr std::int64_t kSettleNs = 1'000'000'000;  // supervisor starts in SAFE_STOP
constexpr std::int64_t kBaseLatencyNs = 20 * kMs;
constexpr float kHealthyConfidence = 0.9F;
constexpr std::uint64_t kSeed = 20261122U;

constexpr int sev(SafetyState s) {return static_cast<int>(s);}

struct Expect
{
  SafetyState min_state;      ///< worst state must be at least this
  SafetyState max_state;      ///< worst state must be at most this
  std::int64_t max_detect_ns; ///< time to reach min_state; ignored if min_state == kNominal
  bool must_recover;          ///< must be kNominal at the end (requires end_ns > 0)
};

struct Scenario
{
  const char * name;
  FaultConfig fault;
  std::int64_t extra_latency_ns;  ///< added to pipeline latency in the fault window
  float fault_confidence;         ///< < 0: unused
  float person_m;                 ///< < 0: unused
  std::int64_t end_ns;            ///< 0: fault never ends
  Expect expect;
};

struct Result
{
  SafetyState worst{SafetyState::kNominal};
  std::int64_t detect_ns{-1};  ///< onset -> first cycle at/above expect.min_state
  std::uint32_t false_alarms{0U};
  SafetyState final_state{SafetyState::kSafeStop};
};

FaultConfig cfg(double drop, std::int64_t delay, float noise, std::int64_t skew, bool freeze)
{
  FaultConfig c;
  c.drop_probability = drop;
  c.added_delay_ns = delay;
  c.noise_amplitude = noise;
  c.clock_skew_ns = skew;
  c.freeze = freeze;
  return c;
}

const char * stateName(SafetyState s)
{
  switch (s) {
    case SafetyState::kNominal: return "NOMINAL";
    case SafetyState::kDegraded: return "DEGRADED";
    case SafetyState::kSafeStop: return "SAFE_STOP";
    default: return "?";
  }
}

// Detection budget for stale-data faults: stale_stop (500 ms) + three cycles of margin.
constexpr std::int64_t kStopDetectNs = 500 * kMs + 3 * kPeriodNs;

const std::array<Scenario, 9> kScenarios = {{
  {"baseline", cfg(0.0, 0, 0.0F, 0, false), 0, -1.0F, -1.0F, 0,
    {SafetyState::kNominal, SafetyState::kNominal, 0, false}},
  {"camera_freeze", cfg(0.0, 0, 0.0F, 0, true), 0, -1.0F, -1.0F, 0,
    {SafetyState::kSafeStop, SafetyState::kSafeStop, kStopDetectNs, false}},
  {"total_dropout", cfg(1.0, 0, 0.0F, 0, false), 0, -1.0F, -1.0F, 10'000'000'000,
    {SafetyState::kSafeStop, SafetyState::kSafeStop, kStopDetectNs, true}},
  {"latency_spike_150ms", cfg(0.0, 0, 0.0F, 0, false), 150 * kMs, -1.0F, -1.0F, 10'000'000'000,
    {SafetyState::kDegraded, SafetyState::kDegraded, 2 * kPeriodNs, true}},
  {"clock_skew_future_1s", cfg(0.0, 0, 0.0F, 1'000 * kMs, false), 0, -1.0F, -1.0F, 10'000'000'000,
    {SafetyState::kSafeStop, SafetyState::kSafeStop, 2 * kPeriodNs, true}},
  {"clock_skew_past_300ms", cfg(0.0, 0, 0.0F, -300 * kMs, false), 0, -1.0F, -1.0F, 10'000'000'000,
    {SafetyState::kDegraded, SafetyState::kDegraded, 2 * kPeriodNs, true}},
  {"intermittent_drop_30pct", cfg(0.3, 0, 0.0F, 0, false), 0, -1.0F, -1.0F, 0,
    {SafetyState::kNominal, SafetyState::kDegraded, 0, false}},
  {"low_confidence", cfg(0.0, 0, 0.0F, 0, false), 0, 0.2F, -1.0F, 10'000'000'000,
    {SafetyState::kDegraded, SafetyState::kDegraded, 2 * kPeriodNs, true}},
  {"person_very_close", cfg(0.0, 0, 0.0F, 0, false), 0, -1.0F, 0.5F, 10'000'000'000,
    {SafetyState::kSafeStop, SafetyState::kSafeStop, 2 * kPeriodNs, true}},
}};

Result runScenario(const Scenario & sc)
{
  Supervisor sup{guardrail::Config{}};
  FaultInjector injector(sc.fault, kSeed);
  Result r;

  std::int64_t last_frame_ns = 0;
  float confidence = kHealthyConfidence;

  for (std::int64_t t = kPeriodNs; t <= kDurationNs; t += kPeriodNs) {
    const bool in_window = (t >= kOnsetNs) && ((sc.end_ns == 0) || (t < sc.end_ns));

    Sample s{t, kHealthyConfidence};
    if (in_window && (sc.fault_confidence >= 0.0F)) {
      s.value = sc.fault_confidence;
    }
    bool delivered = true;
    if (in_window) {
      delivered = injector.apply(s);
    }
    if (delivered) {
      last_frame_ns = s.stamp_ns;
      confidence = s.value;
    }

    guardrail::Inputs in;
    in.now_ns = t;
    in.last_frame_ns = last_frame_ns;
    in.confidence = confidence;
    in.nearest_person_m = (in_window && (sc.person_m >= 0.0F)) ? sc.person_m : -1.0F;
    in.pipeline_latency_ns = kBaseLatencyNs + (in_window ? sc.extra_latency_ns : 0);

    const guardrail::Decision d = sup.update(in);
    r.final_state = d.state;
    if (sev(d.state) > sev(r.worst) && t >= kOnsetNs) {
      r.worst = d.state;
    }
    if ((t >= kSettleNs) && (t < kOnsetNs) && (d.state != SafetyState::kNominal)) {
      ++r.false_alarms;
    }
    if ((r.detect_ns < 0) && (t >= kOnsetNs) && (sev(d.state) >= sev(sc.expect.min_state)) &&
      (sc.expect.min_state != SafetyState::kNominal))
    {
      r.detect_ns = t - kOnsetNs;
    }
  }
  return r;
}

bool check(const Scenario & sc, const Result & r, std::string & why)
{
  const Expect & e = sc.expect;
  if (r.false_alarms != 0U) {why = "false alarms before onset"; return false;}
  if (sev(r.worst) < sev(e.min_state)) {why = "fault not detected at required severity"; return false;}
  if (sev(r.worst) > sev(e.max_state)) {why = "over-reaction (worse than allowed)"; return false;}
  if ((e.min_state != SafetyState::kNominal) &&
    ((r.detect_ns < 0) || (r.detect_ns > e.max_detect_ns)))
  {
    why = "detection too slow";
    return false;
  }
  if (e.must_recover && (r.final_state != SafetyState::kNominal)) {
    why = "did not recover to NOMINAL";
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char ** argv)
{
  std::string only = "all";
  const char * csv_path = nullptr;
  for (int i = 1; i < argc; ++i) {
    if ((std::strcmp(argv[i], "--scenario") == 0) && ((i + 1) < argc)) {
      only = argv[++i];
    } else if ((std::strcmp(argv[i], "--csv") == 0) && ((i + 1) < argc)) {
      csv_path = argv[++i];
    } else {
      std::fprintf(stderr, "usage: fault_campaign [--scenario NAME|all] [--csv FILE]\n");
      return 2;
    }
  }

  std::FILE * csv = nullptr;
  if (csv_path != nullptr) {
    csv = std::fopen(csv_path, "w");
    if (csv == nullptr) {
      std::fprintf(stderr, "fault_campaign: cannot write %s\n", csv_path);
      return 2;
    }
    std::fprintf(csv, "scenario,worst_state,detect_ms,false_alarms,final_state,pass\n");
  }

  int ran = 0;
  int failed = 0;
  for (const Scenario & sc : kScenarios) {
    if ((only != "all") && (only != sc.name)) {
      continue;
    }
    ++ran;
    const Result r = runScenario(sc);
    std::string why;
    const bool ok = check(sc, r, why);
    const double detect_ms = (r.detect_ns < 0) ? -1.0 : static_cast<double>(r.detect_ns) / 1.0e6;
    std::printf("%-26s worst=%-9s detect_ms=%7.1f false_alarms=%u final=%-9s %s%s%s\n", sc.name,
      stateName(r.worst), detect_ms, static_cast<unsigned>(r.false_alarms),
      stateName(r.final_state), ok ? "PASS" : "FAIL", ok ? "" : " - ", ok ? "" : why.c_str());
    if (csv != nullptr) {
      std::fprintf(csv, "%s,%s,%.1f,%u,%s,%d\n", sc.name, stateName(r.worst), detect_ms,
        static_cast<unsigned>(r.false_alarms), stateName(r.final_state), ok ? 1 : 0);
    }
    if (!ok) {++failed;}
  }
  if (csv != nullptr) {std::fclose(csv);}

  if (ran == 0) {
    std::fprintf(stderr, "fault_campaign: unknown scenario '%s'\n", only.c_str());
    return 2;
  }
  std::printf("%d/%d scenarios passed (decision-logic simulation, seed %llu)\n", ran - failed,
    ran, static_cast<unsigned long long>(kSeed));
  return (failed == 0) ? 0 : 1;
}
