#ifndef VIGIA_SELF_TESTS_H
#define VIGIA_SELF_TESTS_H

#include "edge_core.h"
#include <cstdint>
#include <cmath>

// These tests run the SAME C++ engine as the application. They exercise pure
// logic with synthetic acquisition times, not GPIO, physical water or network.
namespace vigia {
namespace self_test_detail {

inline Inputs defaults() {
  Inputs input = {25.0f, 10.0f, 6.0f, 2.0f, true, true, true, true};
  return input;
}
struct Fixture {
  EdgeController core;
  uint32_t now;
  Inputs input;
  explicit Fixture(uint32_t start = 0, FilterMode mode = FilterMode::MEDIAN)
      : core(mode), now(start), input(defaults()) {
    core.begin(now);
  }
  void step(uint32_t delta = 1000) {
    now += delta;
    core.ingest(now, input);
    core.tick(now);
  }
  void feed(float optical, uint16_t count, float flow = 6.0f) {
    input.optical = optical;
    input.flowLpm = flow;
    for (uint16_t i = 0; i < count; ++i) step();
  }
};
inline bool close(float a, float b) { return std::fabs(a - b) < 0.001f; }
inline void record(void (*report)(const char*, bool), uint16_t& passed,
                   uint16_t& failed, const char* name, bool ok) {
  if (ok) ++passed; else ++failed;
  if (report) report(name, ok);
}
}  // namespace self_test_detail

inline bool runSelfTests(void (*report)(const char*, bool),
                         uint16_t& passed, uint16_t& failed) {
  using namespace self_test_detail;
  passed = failed = 0;

  {
    Fixture f;
    const View v = f.core.view();
    record(report, passed, failed, "boot_is_unknown_not_green",
      v.waterState == WaterState::UNKNOWN &&
      v.state == AssessmentState::UNAVAILABLE &&
      v.reason == Reason::INITIALIZING && !v.hasFresh && !v.filteredValid);
  }
  {
    Fixture f;
    f.step();
    bool ok = f.core.view().state == AssessmentState::UNAVAILABLE &&
      f.core.view().flow == FlowContext::UNKNOWN;
    f.step();
    ok = ok && f.core.view().flow == FlowContext::RENEWING;
    f.step();
    ok = ok && f.core.view().flow == FlowContext::FLOWING &&
      f.core.view().windowSize == 0;
    f.step();
    ok = ok && f.core.view().windowSize == 1;
    record(report, passed, failed, "flow_two_samples_then_renew_then_collect", ok);
  }
  {
    Fixture f;
    f.feed(10, 7, 1);
    bool ok = f.core.view().flow == FlowContext::RENEWING;
    f.feed(10, 1, 1);
    ok = ok && f.core.view().flow == FlowContext::FLOWING &&
      f.core.view().windowSize == 0;
    record(report, passed, failed, "renewal_depends_on_integrated_volume", ok);
  }
  {
    Fixture f;
    f.feed(10, 12, 0.15f);
    record(report, passed, failed, "deadband_cannot_start_unconfirmed_flow",
      f.core.view().state == AssessmentState::UNAVAILABLE &&
      f.core.view().reason == Reason::NO_FLOW);
  }

  const float starts[] = {10.0f, 40.0f, 80.0f};
  const AssessmentState targets[] = {AssessmentState::NO_CHANGE,
    AssessmentState::ATTENTION, AssessmentState::CHECK};
  const char* startNames[] = {"startup_normal_requires_fresh_window",
    "startup_attention_requires_fresh_window", "startup_check_requires_fresh_window"};
  for (uint8_t i = 0; i < 3; ++i) {
    Fixture f;
    f.feed(starts[i], 9);
    bool ok = f.core.view().state == AssessmentState::UNAVAILABLE &&
      f.core.view().assessmentSeq == 2;
    f.feed(starts[i], 1);
    ok = ok && f.core.view().state == targets[i] &&
      f.core.view().assessmentSeq == 3;
    record(report, passed, failed, startNames[i], ok);
  }
  {
    Fixture f;
    f.feed(10, 12);
    f.feed(40, 4);
    bool ok = f.core.view().waterState == WaterState::NO_CHANGE &&
      f.core.view().state == AssessmentState::UNAVAILABLE;
    f.feed(40, 1);
    record(report, passed, failed, "median_attention_requires_five_new_step_samples",
      ok && f.core.view().state == AssessmentState::ATTENTION);
  }
  {
    Fixture f;
    f.feed(10, 12);
    f.feed(80, 4);
    bool ok = f.core.view().waterState == WaterState::NO_CHANGE &&
      f.core.view().state == AssessmentState::UNAVAILABLE;
    f.feed(80, 1);
    record(report, passed, failed, "persistent_severe_step_can_confirm_check_directly",
      ok && f.core.view().state == AssessmentState::CHECK);
  }
  {
    Fixture f;
    f.feed(80, 12);
    f.feed(57, 15);
    record(report, passed, failed, "check_hysteresis_holds_at_57",
      f.core.view().state == AssessmentState::CHECK);
  }
  {
    Fixture f;
    f.feed(40, 12);
    f.feed(28, 15);
    record(report, passed, failed, "attention_hysteresis_holds_at_28",
      f.core.view().state == AssessmentState::ATTENTION);
  }
  {
    Fixture f;
    f.feed(80, 12);
    bool sawAttention = false, sawNormal = false, order = true;
    for (uint8_t i = 0; i < 16; ++i) {
      f.feed(20, 1);
      if (f.core.view().state == AssessmentState::ATTENTION) sawAttention = true;
      if (f.core.view().state == AssessmentState::NO_CHANGE) {
        sawNormal = true;
        if (!sawAttention) order = false;
      }
    }
    record(report, passed, failed, "recovery_descends_one_level_at_a_time",
      sawAttention && sawNormal && order);
  }
  {
    Fixture f;
    f.feed(30, 12);
    bool ok = f.core.view().state == AssessmentState::ATTENTION;
    f.feed(60, 12);
    ok = ok && f.core.view().state == AssessmentState::CHECK;
    f.feed(55, 7);
    ok = ok && f.core.view().state == AssessmentState::ATTENTION;
    f.feed(25, 7);
    record(report, passed, failed, "inclusive_thresholds_30_60_55_25",
      ok && f.core.view().state == AssessmentState::NO_CHANGE);
  }
  {
    Fixture f;
    f.feed(10, 12);
    f.feed(100, 1);
    bool ok = close(f.core.view().filteredOptical, 10);
    for (uint8_t i = 0; i < 5; ++i) {
      f.feed(10, 1);
      ok = ok && f.core.view().state == AssessmentState::NO_CHANGE;
    }
    record(report, passed, failed, "median_rejects_one_spike_base10", ok);
  }
  {
    Fixture f;
    f.feed(20, 12);
    f.feed(100, 1);
    bool ok = close(f.core.view().filteredOptical, 20) &&
      f.core.view().observation == Observation::ELEVATED;
    f.feed(20, 5);
    record(report, passed, failed, "median_spike_base20_is_provisional_not_alert",
      ok && f.core.view().state == AssessmentState::NO_CHANGE &&
      f.core.view().observation == Observation::NONE);
  }
  {
    Fixture f;
    for (uint8_t i = 0; i < 20; ++i) f.feed(i % 2 ? 61 : 31, 1);
    record(report, passed, failed, "general_change_retained_while_severe_is_unconfirmed",
      f.core.view().waterState == WaterState::ATTENTION &&
      f.core.view().state == AssessmentState::UNAVAILABLE &&
      f.core.view().observation == Observation::ELEVATED);
  }

  for (uint8_t channel = 0; channel < 4; ++channel) {
    Fixture f;
    f.feed(80, 12);
    const uint32_t fresh = f.core.view().lastFreshMs;
    const uint32_t seq = f.core.view().assessmentSeq;
    if (channel == 0) f.input.tempValid = false;
    if (channel == 1) f.input.opticalValid = false;
    if (channel == 2) f.input.flowValid = false;
    if (channel == 3) f.input.pressureValid = false;
    f.step();
    const View v = f.core.view();
    const char* names[] = {"invalid_temperature_preserves_check",
      "invalid_optical_preserves_check", "invalid_flow_preserves_check",
      "invalid_pressure_preserves_check"};
    const bool critical = channel == 1 || channel == 2;
    const bool behaviour = critical ?
      (v.state == AssessmentState::UNAVAILABLE && v.reason == Reason::SENSOR_INVALID &&
       !v.filteredValid && v.windowSize == 0 && v.lastFreshMs == fresh && v.assessmentSeq == seq) :
      (v.state == AssessmentState::CHECK && v.filteredValid && v.windowSize == 5 &&
       v.lastFreshMs == f.now && v.assessmentSeq == seq + 1);
    record(report, passed, failed, names[channel], behaviour &&
      v.waterState == WaterState::CHECK && v.quality[channel] == Quality::INVALID &&
      v.quality[channel == 1 ? 0 : 1] == Quality::VALID);
  }
  {
    Fixture f;
    f.input.optical = NAN;
    f.step();
    record(report, passed, failed, "nan_is_invalid_not_zero",
      f.core.view().reason == Reason::SENSOR_INVALID && !f.core.view().hasFresh);
  }
  {
    Fixture f;
    f.input.optical = 101;
    f.step();
    record(report, passed, failed, "out_of_range_is_invalid",
      f.core.view().reason == Reason::SENSOR_INVALID);
  }
  {
    Fixture f;
    f.feed(80, 12);
    f.input.opticalValid = false;
    f.step();
    f.input.opticalValid = true;
    f.feed(57, 9);
    bool ok = f.core.view().state == AssessmentState::UNAVAILABLE &&
      f.core.view().waterState == WaterState::CHECK;
    f.feed(57, 1);
    record(report, passed, failed, "invalid_resume57_reuses_hysteresis_not_old_window",
      ok && f.core.view().state == AssessmentState::CHECK);
  }
  {
    Fixture f;
    f.feed(80, 12);
    f.input.opticalValid = false;
    f.step();
    f.input.opticalValid = true;
    f.feed(20, 10);
    bool ok = f.core.view().state == AssessmentState::ATTENTION;
    f.feed(20, 2);
    ok = ok && f.core.view().state == AssessmentState::ATTENTION;
    f.feed(20, 1);
    record(report, passed, failed, "invalid_resume20_recovers_in_two_confirmed_steps",
      ok && f.core.view().state == AssessmentState::NO_CHANGE);
  }
  {
    Fixture f;
    f.feed(10, 12);
    const View before = f.core.view();
    f.now += 1501;
    f.core.tick(f.now);
    const View after = f.core.view();
    record(report, passed, failed, "acquisition_gap_clears_window_not_timestamps",
      after.state == AssessmentState::UNAVAILABLE && after.windowSize == 0 &&
      after.flow == FlowContext::UNKNOWN && after.lastFreshMs == before.lastFreshMs &&
      after.assessmentSeq == before.assessmentSeq);
  }
  {
    Fixture f;
    f.feed(10, 12);
    f.core.tick(f.now + 10000u);
    bool ok = f.core.view().reason != Reason::SENSOR_STALE;
    f.core.tick(f.now + 10001u);
    record(report, passed, failed, "stale_strictly_after_ten_seconds",
      ok && f.core.view().reason == Reason::SENSOR_STALE &&
      f.core.view().state == AssessmentState::UNAVAILABLE);
  }
  {
    Fixture f;
    f.core.tick(10001u);
    record(report, passed, failed, "never_received_batch_can_become_stale",
      !f.core.view().hasFresh && f.core.view().reason == Reason::SENSOR_STALE);
  }
  {
    Fixture f;
    f.feed(10, 7, 1);
    f.step(2001);
    bool ok = f.core.view().flow == FlowContext::UNKNOWN &&
      f.core.view().windowSize == 0;
    f.feed(10, 6, 1);
    ok = ok && f.core.view().flow == FlowContext::RENEWING;
    f.feed(10, 1, 1);
    record(report, passed, failed, "gap_does_not_invent_renewal_volume",
      ok && f.core.view().flow == FlowContext::FLOWING);
  }
  {
    Fixture f;
    f.feed(80, 12);
    f.feed(80, 1, 0);
    bool ok = f.core.view().state == AssessmentState::UNAVAILABLE &&
      f.core.view().reason == Reason::NO_FLOW && f.core.view().windowSize == 0 &&
      f.core.view().waterState == WaterState::CHECK;
    f.feed(80, 1, 0);
    f.feed(57, 9);
    ok = ok && f.core.view().state == AssessmentState::UNAVAILABLE;
    f.feed(57, 1);
    record(report, passed, failed, "stop_and_restart_require_new_renewal_and_window",
      ok && f.core.view().state == AssessmentState::CHECK);
  }
  {
    Fixture f;
    f.feed(10, 12);
    f.feed(10, 3, 0.15f);
    record(report, passed, failed, "deadband_retains_already_confirmed_flow",
      f.core.view().flow == FlowContext::FLOWING &&
      f.core.view().state == AssessmentState::NO_CHANGE);
  }
  {
    Fixture f;
    f.feed(10, 12);
    const uint32_t seq = f.core.view().assessmentSeq;
    for (uint16_t i = 0; i < 100; ++i) f.core.tick(f.now + i);
    for (uint8_t i = 0; i < 10; ++i) f.core.ingest(f.now, f.input);
    record(report, passed, failed, "ticks_and_duplicate_batches_do_not_advance_persistence",
      f.core.view().assessmentSeq == seq);
  }
  {
    Fixture f(0xfffff000u);
    f.feed(10, 12);
    bool ok = f.core.view().state == AssessmentState::NO_CHANGE;
    f.core.tick(f.now + 10001u);
    record(report, passed, failed, "millis_rollover_preserves_intervals_and_stale",
      ok && f.core.view().reason == Reason::SENSOR_STALE);
  }
  {
    Fixture f;
    f.input.temperatureC = 0;
    f.input.pressureBar = 0;
    f.feed(0, 12);
    record(report, passed, failed, "zero_optical_temperature_pressure_are_valid_values",
      f.core.view().state == AssessmentState::NO_CHANGE && f.core.view().hasFresh);
  }
  {
    Fixture f;
    f.input.temperatureC = 0;
    f.input.pressureBar = 0;
    f.feed(0, 3, 0);
    record(report, passed, failed, "zero_flow_is_no_flow_not_sensor_fault",
      f.core.view().reason == Reason::NO_FLOW && f.core.view().hasFresh);
  }
  {
    Fixture f;
    f.input.temperatureC = 125;
    f.input.pressureBar = 6;
    f.feed(10, 12);
    record(report, passed, failed, "temperature_pressure_context_not_sanitary_triggers",
      f.core.view().state == AssessmentState::NO_CHANGE);
  }
  {
    Fixture mean(0, FilterMode::MEAN), median;
    mean.feed(20, 12); median.feed(20, 12);
    mean.feed(100, 1); median.feed(100, 1);
    bool ok = close(mean.core.view().filteredOptical, 36) &&
      close(median.core.view().filteredOptical, 20);
    mean.feed(20, 5); median.feed(20, 5);
    record(report, passed, failed, "compare_mean36_median20_raw_confirmation_stops_old_spike",
      ok && mean.core.view().state == AssessmentState::NO_CHANGE &&
      median.core.view().state == AssessmentState::NO_CHANGE);
  }
  {
    Fixture f;
    f.feed(20, 12); f.feed(100, 2);
    bool ok = close(f.core.view().filteredOptical, 20);
    f.feed(20, 5);
    record(report, passed, failed, "median_rejects_two_impulses_in_five_samples",
      ok && f.core.view().state == AssessmentState::NO_CHANGE);
  }
  {
    Fixture f;
    f.feed(20, 12); f.feed(80, 3);
    bool ok = f.core.view().waterState == WaterState::NO_CHANGE &&
      f.core.view().observation == Observation::ELEVATED;
    f.feed(20, 5);
    record(report, passed, failed, "raw_return_cancels_confirmation_from_overlapping_windows",
      ok && f.core.view().state == AssessmentState::NO_CHANGE);
  }
  {
    Fixture f;
    f.feed(80, 1);
    bool ok = f.core.view().observation == Observation::ELEVATED &&
      f.core.view().state == AssessmentState::UNAVAILABLE &&
      f.core.view().waterState == WaterState::UNKNOWN;
    f.feed(80, 1);
    ok = ok && f.core.view().reason == Reason::RENEWING &&
      f.core.view().observation == Observation::ELEVATED;
    f.feed(80, 8);
    record(report, passed, failed, "startup_high_is_recorded_before_confirmed_assessment",
      ok && f.core.view().state == AssessmentState::CHECK &&
      f.core.view().observation == Observation::NONE);
  }
  {
    Fixture f;
    f.input.opticalValid = false;
    f.feed(80, 1);
    record(report, passed, failed, "invalid_optical_never_becomes_provisional_observation",
      f.core.view().observation == Observation::NONE &&
      f.core.view().quality[1] == Quality::INVALID);
  }
  {
    Fixture f;
    f.feed(10, 12); f.core.tick(f.now + 1501);
    const View v = f.core.view();
    record(report, passed, failed, "gap_marks_each_channel_stale_before_sensor_timeout",
      v.quality[0] == Quality::STALE && v.quality[1] == Quality::STALE &&
      v.quality[2] == Quality::STALE && v.quality[3] == Quality::STALE &&
      v.state == AssessmentState::UNAVAILABLE && v.reason == Reason::CONFIRMING);
  }
  {
    Fixture f;
    f.input.temperatureC = 126; f.input.pressureBar = 7;
    f.feed(10, 12);
    record(report, passed, failed, "out_of_range_context_is_reported_without_erasing_optical_assessment",
      f.core.view().state == AssessmentState::NO_CHANGE &&
      f.core.view().quality[0] == Quality::OUT_OF_RANGE &&
      f.core.view().quality[3] == Quality::OUT_OF_RANGE &&
      f.core.view().quality[1] == Quality::VALID);
  }
  {
    Fixture f;
    f.input.temperatureC = NAN;
    f.feed(40, 12);
    record(report, passed, failed, "nan_context_does_not_claim_valid_or_erase_other_channels",
      f.core.view().state == AssessmentState::ATTENTION &&
      f.core.view().quality[0] == Quality::INVALID &&
      f.core.view().quality[1] == Quality::VALID && f.core.view().filteredValid);
  }
  {
    Fixture f;
    f.feed(10, 12); f.input.pressureValid = false; f.step();
    bool ok = f.core.view().quality[3] == Quality::INVALID;
    f.input.pressureValid = true; f.step();
    record(report, passed, failed, "context_recovers_on_new_sample_without_clearing_filter",
      ok && f.core.view().quality[3] == Quality::VALID &&
      f.core.view().windowSize == 5 && f.core.view().state == AssessmentState::NO_CHANGE);
  }
  {
    Fixture f;
    f.feed(10, 12); f.feed(80, 1, 0);
    record(report, passed, failed, "no_flow_retains_state_but_records_high_optical_observation",
      f.core.view().state == AssessmentState::UNAVAILABLE &&
      f.core.view().waterState == WaterState::NO_CHANGE &&
      f.core.view().observation == Observation::ELEVATED);
  }
  {
    AlarmLatch a;
    bool ok = !a.active() && !a.observe(WaterState::UNKNOWN) &&
      !a.observe(WaterState::NO_CHANGE) && !a.active();
    record(report, passed, failed, "alarm_boot_and_normal_are_silent", ok);
  }
  {
    AlarmLatch a;
    record(report, passed, failed, "alarm_attention_latches_until_acknowledgement",
      a.observe(WaterState::ATTENTION) && a.active());
  }
  {
    AlarmLatch a;
    a.observe(WaterState::CHECK);
    bool ok = true;
    for (uint16_t i = 0; i < 1000; ++i)
      ok = ok && !a.observe(WaterState::CHECK) && a.active();
    record(report, passed, failed, "alarm_has_no_automatic_timeout", ok);
  }
  {
    AlarmLatch a;
    a.observe(WaterState::CHECK); a.silence();
    record(report, passed, failed, "alarm_silence_does_not_rearm_same_occurrence",
      !a.active() && !a.observe(WaterState::CHECK) && !a.active());
  }
  {
    AlarmLatch a;
    a.observe(WaterState::CHECK);
    a.silence();
    record(report, passed, failed, "alarm_button_resume_restores_sound_for_confirmed_condition",
      !a.active() && a.resume(true) && a.active());
  }
  {
    AlarmLatch a;
    a.observe(WaterState::CHECK);
    a.silence();
    a.observe(WaterState::NO_CHANGE);
    record(report, passed, failed, "alarm_button_resume_after_normalization_only_arms_next_alert",
      !a.resume(false) && !a.active());
  }
  {
    AlarmLatch a;
    a.observe(WaterState::ATTENTION);
    a.silence();
    record(report, passed, failed, "alarm_button_resume_during_unavailable_assessment_stays_quiet",
      !a.resume(false) && !a.active());
  }
  {
    AlarmLatch a;
    a.observe(WaterState::ATTENTION);
    a.observe(WaterState::NO_CHANGE);
    record(report, passed, failed, "alarm_normalization_does_not_acknowledge_pending_event", a.active());
  }
  {
    AlarmLatch a;
    a.observe(WaterState::CHECK); a.silence();
    record(report, passed, failed, "alarm_recovery_to_attention_does_not_rearm",
      !a.observe(WaterState::ATTENTION) && !a.active());
  }
  {
    AlarmLatch a;
    a.observe(WaterState::ATTENTION); a.silence();
    record(report, passed, failed, "alarm_escalation_rearms_after_acknowledgement",
      a.observe(WaterState::CHECK) && a.active());
  }
  {
    AlarmLatch a;
    a.observe(WaterState::ATTENTION); a.silence();
    a.observe(WaterState::NO_CHANGE);
    record(report, passed, failed, "alarm_new_occurrence_rearms_after_normalization",
      a.observe(WaterState::ATTENTION) && a.active());
  }
  {
    Fixture f; AlarmLatch a;
    f.feed(80, 12); a.observe(f.core.view().waterState);
    f.feed(80, 1, 0); a.observe(f.core.view().waterState);
    record(report, passed, failed, "alarm_unavailable_assessment_does_not_clear_pending_event",
      f.core.view().state == AssessmentState::UNAVAILABLE && a.active());
  }
  {
    AlarmLatch a;
    a.observe(WaterState::CHECK); a.reset();
    bool ok = !a.active() && !a.observe(WaterState::UNKNOWN);
    record(report, passed, failed, "alarm_reset_clears_pending_and_allows_new_alert",
      ok && a.observe(WaterState::ATTENTION) && a.active());
  }
  return failed == 0;
}

}  // namespace vigia
#endif

