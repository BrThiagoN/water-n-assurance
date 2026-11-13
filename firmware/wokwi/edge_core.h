#ifndef VIGIA_EDGE_CORE_H
#define VIGIA_EDGE_CORE_H

#include <cstdint>
#include <cmath>

// Pure C++ decision engine. No GPIO, network, clock or Arduino dependency.
// All thresholds describe the academic simulation, not drinking-water limits.
namespace vigia {

enum class WaterState { UNKNOWN, NO_CHANGE, ATTENTION, CHECK };
enum class AssessmentState { NO_CHANGE, ATTENTION, CHECK, UNAVAILABLE };
enum class Reason {
  INITIALIZING, SENSOR_INVALID, SENSOR_STALE, NO_FLOW, RENEWING,
  FILTER_WARMUP, CONFIRMING, ASSESSED
};
enum class FlowContext { STOPPED, RENEWING, FLOWING, UNKNOWN };
enum class FilterMode { MEDIAN, MEAN };
enum class Channel : uint8_t { TEMPERATURE, OPTICAL, FLOW, PRESSURE };
enum class Quality : uint8_t { NOT_READY, VALID, INVALID, OUT_OF_RANGE, STALE };
enum class Observation : uint8_t { NONE, RAISED, ELEVATED };

inline const char* qualityName(Quality value) {
  switch (value) {
    case Quality::VALID: return "VALID";
    case Quality::INVALID: return "INVALID";
    case Quality::OUT_OF_RANGE: return "OUT_OF_RANGE";
    case Quality::STALE: return "STALE";
    default: return "NOT_READY";
  }
}
inline const char* observationName(Observation value) {
  return value == Observation::ELEVATED ? "HIGH" :
    (value == Observation::RAISED ? "RAISED" : "NONE");
}

inline const char* waterName(WaterState value) {
  switch (value) {
    case WaterState::NO_CHANGE: return "NO_CHANGE";
    case WaterState::ATTENTION: return "ATTENTION";
    case WaterState::CHECK: return "CHECK";
    default: return "UNKNOWN";
  }
}
inline const char* assessmentName(AssessmentState value) {
  switch (value) {
    case AssessmentState::NO_CHANGE: return "NO_CHANGE";
    case AssessmentState::ATTENTION: return "ATTENTION";
    case AssessmentState::CHECK: return "CHECK";
    default: return "UNAVAILABLE";
  }
}
inline const char* reasonName(Reason value) {
  switch (value) {
    case Reason::SENSOR_INVALID: return "SENSOR_INVALID";
    case Reason::SENSOR_STALE: return "SENSOR_STALE";
    case Reason::NO_FLOW: return "NO_FLOW";
    case Reason::RENEWING: return "RENEWING";
    case Reason::FILTER_WARMUP: return "FILTER_WARMUP";
    case Reason::CONFIRMING: return "CONFIRMING";
    case Reason::ASSESSED: return "ASSESSED";
    default: return "INITIALIZING";
  }
}
inline const char* flowName(FlowContext value) {
  switch (value) {
    case FlowContext::STOPPED: return "STOPPED";
    case FlowContext::RENEWING: return "RENEWING";
    case FlowContext::FLOWING: return "FLOWING";
    default: return "UNKNOWN";
  }
}

struct Inputs {
  float temperatureC, optical, flowLpm, pressureBar;
  bool tempValid, opticalValid, flowValid, pressureValid;
};

struct View {
  WaterState waterState;
  AssessmentState state;
  Reason reason;
  FlowContext flow;
  float filteredOptical;
  bool filteredValid;
  uint8_t windowSize;
  uint32_t lastFreshMs, assessmentSeq;
  bool hasFresh;
  Quality quality[4];
  Observation observation;  // New raw change, not a confirmed classification.
};

class EdgeController {
 public:
  explicit EdgeController(FilterMode mode = FilterMode::MEDIAN) : mode_(mode) { begin(0); }

  void begin(uint32_t now) {
    bootMs_ = now;
    output_.waterState = WaterState::UNKNOWN;
    output_.state = AssessmentState::UNAVAILABLE;
    output_.reason = Reason::INITIALIZING;
    output_.flow = FlowContext::UNKNOWN;
    output_.lastFreshMs = now;
    output_.assessmentSeq = 0;
    output_.hasFresh = false;
    for (uint8_t i = 0; i < 4; ++i) output_.quality[i] = Quality::NOT_READY;
    output_.observation = Observation::NONE;
    hasBatch_ = false;
    lastBatchMs_ = now;
    latestOptical_ = 0;
    invalidActive_ = false;
    resetCirculation();
    clearWindow();
  }

  // Call only for a NEW acquisition batch, normally once per second.
  // Critical freshness refers to optical + flow. App retains the individual
  // driver acquisition times for telemetry; contextual validity is independent.
  void ingest(uint32_t now, const Inputs& input) {
    if (hasBatch_ && uint32_t(now - lastBatchMs_) == 0) return;
    hasBatch_ = true;
    lastBatchMs_ = now;
    output_.quality[0] = channelQuality(input.temperatureC, input.tempValid, -55, 125);
    output_.quality[1] = channelQuality(input.optical, input.opticalValid, 0, 100);
    output_.quality[2] = channelQuality(input.flowLpm, input.flowValid, 0, 20);
    output_.quality[3] = channelQuality(input.pressureBar, input.pressureValid, 0, 6);
    latestOptical_ = input.optical;
    // Temperature and pressure are contextual. Their failure is reported per
    // channel; it cannot erase valid optical/flow measurements or diagnose water.
    if (output_.quality[1] != Quality::VALID || output_.quality[2] != Quality::VALID) {
      invalidActive_ = true;
      resetCirculation();
      suspend(Reason::SENSOR_INVALID, FlowContext::UNKNOWN);
      return;
    }

    const bool hadFresh = output_.hasFresh;
    const uint32_t delta = hadFresh ? uint32_t(now - output_.lastFreshMs) : 0;

    // Repeating the same timestamp is not a new acquisition. This also stops
    // a caller from completing persistence by repeatedly passing one sample.
    if (hadFresh && delta == 0) {
      return;
    }

    if (invalidActive_ || (hadFresh && delta > 1500u)) {
      resetCirculation();
      clearWindow();
    }
    invalidActive_ = false;
    output_.hasFresh = true;
    output_.lastFreshMs = now;

    const float flow = input.flowLpm;
    if (flow <= 0.1f) {
      offCount_ = increment(offCount_, true, 2);
      onCount_ = 0;
      if (offCount_ >= 2) {
        flowConfirmed_ = false;
      }
      // Even the first possible-stop batch is ineligible. Do not continue
      // classifying a former flowing sample while Q is currently zero/low.
      renewed_ = false;
      renewalLitres_ = 0.0f;
      rememberFlow(flow);
      suspend(Reason::NO_FLOW, FlowContext::STOPPED);
      return;
    }
    offCount_ = 0;

    if (!flowConfirmed_) {
      onCount_ = increment(onCount_, flow >= 0.2f, 2);
      if (onCount_ < 2) {
        rememberFlow(flow);
        suspend(Reason::NO_FLOW, FlowContext::UNKNOWN);
        return;
      }
      flowConfirmed_ = true;
      renewed_ = false;
      renewalLitres_ = 0.0f;
      rememberFlow(flow);
      suspend(Reason::RENEWING, FlowContext::RENEWING);
      return;  // Renewal starts NOW, not in the interval before confirmation.
    }

    if (!renewed_) {
      // Left rectangle: the previous known flow represents this known
      // interval. Never extrapolate a missing interval. A gap >1500 ms was
      // already reset above; the 2000-ms integration bound is defensive.
      if (previousFlowKnown_ && previousFlow_ > 0.1f &&
          delta > 0 && delta <= 2000u) {
        renewalLitres_ += previousFlow_ * float(delta) / 60000.0f;
      }
      rememberFlow(flow);
      if (renewalLitres_ + 0.000001f >= 0.10f) {
        renewed_ = true;
        suspend(Reason::FILTER_WARMUP, FlowContext::FLOWING);
      } else {
        suspend(Reason::RENEWING, FlowContext::RENEWING);
      }
      // The batch completing renewal NEVER enters the optical window.
      return;
    }

    rememberFlow(flow);
    output_.flow = FlowContext::FLOWING;
    collect(input.optical);
  }

  // Keep calling while acquisition/network test modes are paused.
  void tick(uint32_t now) {
    if (hasBatch_ && uint32_t(now - lastBatchMs_) > 1500u) {
      for (uint8_t i = 0; i < 4; ++i) {
        if (output_.quality[i] == Quality::VALID) output_.quality[i] = Quality::STALE;
      }
    }
    if (invalidActive_) {
      return;  // Explicit invalidity has priority until a new valid batch.
    }
    const uint32_t reference = output_.hasFresh ? output_.lastFreshMs : bootMs_;
    const uint32_t age = uint32_t(now - reference);
    if (age > 10000u) {
      resetCirculation();
      suspend(Reason::SENSOR_STALE, FlowContext::UNKNOWN);
    } else if (output_.hasFresh && age > 1500u) {
      // Pending data is already unsuitable for NEW assessment; only after
      // >10 s is it specifically reported as SENSOR_STALE.
      resetCirculation();
      suspend(Reason::CONFIRMING, FlowContext::UNKNOWN);
    }
  }

  View view() const {
    View result = output_;
    result.observation = Observation::NONE;
    if (result.quality[1] == Quality::VALID) {
      if (latestOptical_ >= 60 && result.state != AssessmentState::CHECK)
        result.observation = Observation::ELEVATED;
      else if (latestOptical_ >= 30 &&
               (result.state == AssessmentState::UNAVAILABLE || result.state == AssessmentState::NO_CHANGE))
        result.observation = Observation::RAISED;
    }
    if (result.observation != Observation::NONE && result.state != AssessmentState::UNAVAILABLE) {
      // Present the change as unconfirmed, retaining the last classified state.
      // Do not clear the filter or manufacture a confirmed water alert.
      result.state = AssessmentState::UNAVAILABLE;
      result.reason = Reason::CONFIRMING;
    }
    return result;
  }

  FilterMode filterMode() const { return mode_; }

 private:
  View output_;
  uint32_t bootMs_;
  bool invalidActive_;
  FilterMode mode_;
  bool hasBatch_;
  uint32_t lastBatchMs_;
  float latestOptical_;
  bool flowConfirmed_, renewed_, previousFlowKnown_;
  uint8_t onCount_, offCount_;
  float renewalLitres_, previousFlow_;
  float opticalWindow_[5], opticalSum_;
  uint8_t nextWindow_, usedWindow_, resumed_;
  uint8_t hi30_, hi60_, lo25_, lo55_, lo30_;

  static uint8_t increment(uint8_t count, bool condition, uint8_t maximum = 3) {
    return !condition ? 0 : (count < maximum ? uint8_t(count + 1) : maximum);
  }
  static bool inRange(float value, float low, float high) {
    return std::isfinite(value) && value >= low && value <= high;
  }
  static Quality channelQuality(float value, bool driverValid, float low, float high) {
    if (!driverValid || !std::isfinite(value)) return Quality::INVALID;
    return inRange(value, low, high) ? Quality::VALID : Quality::OUT_OF_RANGE;
  }
  void resetCounts() { hi30_ = hi60_ = lo25_ = lo55_ = lo30_ = 0; }
  void clearWindow() {
    opticalSum_ = 0.0f;
    nextWindow_ = usedWindow_ = resumed_ = 0;
    for (uint8_t i = 0; i < 5; ++i) opticalWindow_[i] = 0.0f;
    output_.filteredOptical = 0.0f;
    output_.filteredValid = false;
    output_.windowSize = 0;
    resetCounts();
  }
  void resetCirculation() {
    flowConfirmed_ = renewed_ = previousFlowKnown_ = false;
    onCount_ = offCount_ = 0;
    renewalLitres_ = previousFlow_ = 0.0f;
  }
  void rememberFlow(float value) {
    previousFlow_ = value;
    previousFlowKnown_ = true;
  }
  void suspend(Reason reason, FlowContext flow) {
    clearWindow();
    output_.state = AssessmentState::UNAVAILABLE;
    output_.reason = reason;
    output_.flow = flow;
    // Retain waterState and assessmentSeq; neither is a current measurement.
  }
  void collect(float value) {
    if (usedWindow_ == 5) {
      opticalSum_ -= opticalWindow_[nextWindow_];
    } else {
      ++usedWindow_;
    }
    opticalWindow_[nextWindow_] = value;
    opticalSum_ += value;
    nextWindow_ = uint8_t((nextWindow_ + 1) % 5);
    output_.windowSize = usedWindow_;
    if (usedWindow_ < 5) {
      output_.state = AssessmentState::UNAVAILABLE;
      output_.reason = Reason::FILTER_WARMUP;
      return;
    }

    float x = opticalSum_ / 5.0f;
    if (mode_ == FilterMode::MEDIAN) {
      float ordered[5];
      for (uint8_t i = 0; i < 5; ++i) ordered[i] = opticalWindow_[i];
      for (uint8_t i = 1; i < 5; ++i) {
        float item = ordered[i];
        uint8_t j = i;
        while (j > 0 && ordered[j - 1] > item) { ordered[j] = ordered[j - 1]; --j; }
        ordered[j] = item;
      }
      x = ordered[2];
    }
    output_.filteredOptical = x;
    output_.filteredValid = true;
    ++output_.assessmentSeq;
    resumed_ = increment(resumed_, true);
    // Overlapping windows are not independent evidence. Each confirmation also
    // requires the NEW raw sample to support the same condition. One old spike
    // cannot keep advancing persistence after the input returned to baseline.
    hi30_ = increment(hi30_, x >= 30.0f && value >= 30.0f);
    hi60_ = increment(hi60_, x >= 60.0f && value >= 60.0f);
    lo25_ = increment(lo25_, x <= 25.0f && value <= 25.0f);
    lo55_ = increment(lo55_, x <= 55.0f && value <= 55.0f);
    lo30_ = increment(lo30_, x < 30.0f && value < 30.0f);

    WaterState target = output_.waterState;
    if (target != WaterState::CHECK && hi60_ >= 3) {
      target = WaterState::CHECK;
    } else if ((target == WaterState::UNKNOWN || target == WaterState::NO_CHANGE)
               && hi30_ >= 3) {
      target = WaterState::ATTENTION;
    } else if (target == WaterState::CHECK && lo55_ >= 3) {
      target = WaterState::ATTENTION;
    } else if (target == WaterState::ATTENTION && lo25_ >= 3) {
      target = WaterState::NO_CHANGE;
    } else if (target == WaterState::UNKNOWN && lo30_ >= 3) {
      target = WaterState::NO_CHANGE;
    }
    if (target != output_.waterState) {
      output_.waterState = target;
      resetCounts();
    }
    if (resumed_ < 3 || target == WaterState::UNKNOWN) {
      output_.state = AssessmentState::UNAVAILABLE;
      output_.reason = Reason::CONFIRMING;
    } else {
      output_.state = target == WaterState::CHECK ? AssessmentState::CHECK :
        (target == WaterState::ATTENTION ? AssessmentState::ATTENTION :
                                         AssessmentState::NO_CHANGE);
      output_.reason = Reason::ASSESSED;
    }
  }
};

// A confirmed water-change alarm requires acknowledgement. Measurement state
// may recover or become unavailable without clearing the pending occurrence.
class AlarmLatch {
 public:
  AlarmLatch() { reset(); }
  void reset() { pending_ = false; previous_ = WaterState::UNKNOWN; }
  bool observe(WaterState current) {
    const bool alert = current == WaterState::ATTENTION || current == WaterState::CHECK;
    const bool newOccurrence = alert &&
      (previous_ == WaterState::UNKNOWN || previous_ == WaterState::NO_CHANGE ||
       (previous_ == WaterState::ATTENTION && current == WaterState::CHECK));
    if (newOccurrence) pending_ = true;
    previous_ = current;
    return newOccurrence;
  }
  void silence() { pending_ = false; }
  bool resume(bool confirmedAlert) {
    if (confirmedAlert && isAlert(previous_)) pending_ = true;
    return pending_;
  }
  bool active() const { return pending_; }
 private:
  static bool isAlert(WaterState state) {
    return state == WaterState::ATTENTION || state == WaterState::CHECK;
  }
  bool pending_;
  WaterState previous_;
};

}  // namespace vigia
#endif

