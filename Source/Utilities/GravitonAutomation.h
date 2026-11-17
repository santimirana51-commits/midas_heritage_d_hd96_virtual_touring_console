#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <vector>

#include <juce_core/juce_core.h>

namespace nova::graviton {

enum class AutomationParameter : int {
  channelFader,
  auxMaster,
  matrixMaster,
  mainMaster,
  channelPan,
  auxSend,
  inputGain,
  eqFrequency,
  eqGain,
  eqQ,
  compressorThreshold,
  compressorRatio,
  gateThreshold
};

enum class AutomationCurve : int { linear, smooth, step };

struct AutomationPoint {
  double timeSeconds = 0.0;
  double value = 0.0;
  AutomationCurve curve = AutomationCurve::linear;
};

struct AutomationTrack {
  std::uint32_t id = 0;
  AutomationParameter parameter = AutomationParameter::channelFader;
  int targetIndex = 0;
  int secondaryIndex = 0;
  juce::String name;
  bool enabled = true;
  bool muted = false;
  bool solo = false;
  std::vector<AutomationPoint> points;
};

class AutomationModel final {
public:
  static constexpr int kMaxTracks = 64;
  static constexpr int kMaxPointsPerTrack = 2048;
  static constexpr double kMaxTimelineSeconds = 86400.0;

  int getNumTracks() const noexcept { return static_cast<int>(tracks.size()); }

  const AutomationTrack* getTrack(int index) const noexcept {
    return index >= 0 && index < getNumTracks()
               ? &tracks[static_cast<std::size_t>(index)]
               : nullptr;
  }

  AutomationTrack* getTrack(int index) noexcept {
    return index >= 0 && index < getNumTracks()
               ? &tracks[static_cast<std::size_t>(index)]
               : nullptr;
  }

  int addTrack(AutomationParameter parameter, int targetIndex,
               int secondaryIndex, juce::String name = {}) {
    if (getNumTracks() >= kMaxTracks || targetIndex < 0 ||
        secondaryIndex < 0)
      return -1;
    AutomationTrack track;
    track.id = nextId++;
    track.parameter = parameter;
    track.targetIndex = targetIndex;
    track.secondaryIndex = secondaryIndex;
    track.name = name.trim().substring(0, 40);
    if (track.name.isEmpty())
      track.name = defaultTrackName(parameter, targetIndex, secondaryIndex);
    tracks.push_back(std::move(track));
    return getNumTracks() - 1;
  }

  bool removeTrack(int index) {
    if (index < 0 || index >= getNumTracks())
      return false;
    tracks.erase(tracks.begin() + index);
    return true;
  }

  bool setPoint(int trackIndex, double timeSeconds, double value,
                AutomationCurve curve = AutomationCurve::linear) {
    auto* track = getTrack(trackIndex);
    if (track == nullptr || !std::isfinite(timeSeconds) ||
        !std::isfinite(value) || timeSeconds < 0.0 ||
        timeSeconds > kMaxTimelineSeconds ||
        static_cast<int>(curve) < 0 || static_cast<int>(curve) > 2)
      return false;

    auto it = std::lower_bound(
        track->points.begin(), track->points.end(), timeSeconds,
        [](const AutomationPoint& point, double time) {
          return point.timeSeconds < time;
        });
    if (it != track->points.end() &&
        std::abs(it->timeSeconds - timeSeconds) < 1.0e-9) {
      it->value = value;
      it->curve = curve;
      return true;
    }
    if (static_cast<int>(track->points.size()) >= kMaxPointsPerTrack)
      return false;
    track->points.insert(it, {timeSeconds, value, curve});
    return true;
  }

  bool removePoint(int trackIndex, int pointIndex) {
    auto* track = getTrack(trackIndex);
    if (track == nullptr || pointIndex < 0 ||
        pointIndex >= static_cast<int>(track->points.size()))
      return false;
    track->points.erase(track->points.begin() + pointIndex);
    return true;
  }

  bool movePoint(int trackIndex, int pointIndex, double timeSeconds,
                 double value) {
    auto* track = getTrack(trackIndex);
    if (track == nullptr || pointIndex < 0 ||
        pointIndex >= static_cast<int>(track->points.size()))
      return false;
    const auto curve =
        track->points[static_cast<std::size_t>(pointIndex)].curve;
    track->points.erase(track->points.begin() + pointIndex);
    return setPoint(trackIndex, timeSeconds, value, curve);
  }

  static double evaluate(const AutomationTrack& track,
                         double timeSeconds) noexcept {
    if (track.points.empty() || !std::isfinite(timeSeconds))
      return 0.0;
    if (timeSeconds <= track.points.front().timeSeconds)
      return track.points.front().value;
    if (timeSeconds >= track.points.back().timeSeconds)
      return track.points.back().value;

    const auto upper = std::upper_bound(
        track.points.begin(), track.points.end(), timeSeconds,
        [](double time, const AutomationPoint& point) {
          return time < point.timeSeconds;
        });
    const auto& right = *upper;
    const auto& left = *(upper - 1);
    if (left.curve == AutomationCurve::step)
      return left.value;
    const double duration = right.timeSeconds - left.timeSeconds;
    if (duration <= 0.0)
      return right.value;
    double t = (timeSeconds - left.timeSeconds) / duration;
    if (left.curve == AutomationCurve::smooth)
      t = t * t * (3.0 - 2.0 * t);
    return left.value + (right.value - left.value) * t;
  }

  juce::var toVar() const {
    auto root = new juce::DynamicObject();
    root->setProperty("format", "NOVA_HD_AUTOMATION");
    root->setProperty("version", 1);
    juce::Array<juce::var> trackArray;
    for (const auto& track : tracks) {
      auto item = new juce::DynamicObject();
      item->setProperty("id", static_cast<int>(track.id));
      item->setProperty("parameter", static_cast<int>(track.parameter));
      item->setProperty("target", track.targetIndex);
      item->setProperty("secondary", track.secondaryIndex);
      item->setProperty("name", track.name);
      item->setProperty("enabled", track.enabled);
      item->setProperty("muted", track.muted);
      item->setProperty("solo", track.solo);
      juce::Array<juce::var> points;
      for (const auto& point : track.points) {
        auto pointObject = new juce::DynamicObject();
        pointObject->setProperty("time", point.timeSeconds);
        pointObject->setProperty("value", point.value);
        pointObject->setProperty("curve", static_cast<int>(point.curve));
        points.add(juce::var(pointObject));
      }
      item->setProperty("points", points);
      trackArray.add(juce::var(item));
    }
    root->setProperty("tracks", trackArray);
    return juce::var(root);
  }

  bool loadFromVar(const juce::var& value) {
    auto* root = value.getDynamicObject();
    if (root == nullptr ||
        root->getProperty("format").toString() != "NOVA_HD_AUTOMATION" ||
        static_cast<int>(root->getProperty("version")) != 1)
      return false;
    const auto* trackArray = root->getProperty("tracks").getArray();
    if (trackArray == nullptr || trackArray->size() > kMaxTracks)
      return false;

    std::vector<AutomationTrack> restored;
    restored.reserve(static_cast<std::size_t>(trackArray->size()));
    std::uint32_t maxId = 0;
    for (const auto& valueTrack : *trackArray) {
      auto* item = valueTrack.getDynamicObject();
      if (item == nullptr)
        return false;
      const int parameter = static_cast<int>(item->getProperty("parameter"));
      AutomationTrack track;
      track.id = static_cast<std::uint32_t>(
          static_cast<int>(item->getProperty("id")));
      track.parameter = static_cast<AutomationParameter>(parameter);
      track.targetIndex = static_cast<int>(item->getProperty("target"));
      track.secondaryIndex = static_cast<int>(item->getProperty("secondary"));
      track.name = item->getProperty("name").toString().trim().substring(0, 40);
      track.enabled = static_cast<bool>(item->getProperty("enabled"));
      track.muted = static_cast<bool>(item->getProperty("muted"));
      track.solo = static_cast<bool>(item->getProperty("solo"));
      const auto* points = item->getProperty("points").getArray();
      if (track.id == 0 || parameter < 0 || parameter > 12 ||
          track.targetIndex < 0 || track.secondaryIndex < 0 ||
          track.name.isEmpty() || points == nullptr ||
          points->size() > kMaxPointsPerTrack)
        return false;
      for (const auto& valuePoint : *points) {
        auto* pointObject = valuePoint.getDynamicObject();
        if (pointObject == nullptr)
          return false;
        const double time = pointObject->getProperty("time");
        const double pointValue = pointObject->getProperty("value");
        const int curve = static_cast<int>(pointObject->getProperty("curve"));
        if (!std::isfinite(time) || time < 0.0 ||
            time > kMaxTimelineSeconds || !std::isfinite(pointValue) ||
            curve < 0 || curve > 2 ||
            (!track.points.empty() &&
             time <= track.points.back().timeSeconds))
          return false;
        track.points.push_back(
            {time, pointValue, static_cast<AutomationCurve>(curve)});
      }
      maxId = std::max(maxId, track.id);
      restored.push_back(std::move(track));
    }
    tracks = std::move(restored);
    nextId = maxId + 1;
    if (nextId == 0)
      nextId = 1;
    return true;
  }

private:
  static juce::String defaultTrackName(AutomationParameter parameter,
                                       int target, int secondary) {
    static constexpr const char* names[] = {
        "CH FADER", "AUX MASTER", "MATRIX MASTER", "MAIN MASTER",
        "CH PAN", "AUX SEND", "INPUT GAIN", "EQ FREQ", "EQ GAIN",
        "EQ Q", "COMP THRESHOLD", "COMP RATIO", "GATE THRESHOLD"};
    auto name = juce::String(names[static_cast<int>(parameter)]) + " " +
                juce::String(target + 1);
    if (parameter == AutomationParameter::auxSend)
      name += " / AUX " + juce::String(secondary + 1);
    if (parameter == AutomationParameter::eqFrequency ||
        parameter == AutomationParameter::eqGain ||
        parameter == AutomationParameter::eqQ)
      name += " / BAND " + juce::String(secondary + 1);
    return name;
  }

  std::vector<AutomationTrack> tracks;
  std::uint32_t nextId = 1;
};

class AutomationTransport final {
public:
  void play() noexcept {
    if (playing)
      return;
    playing = true;
    anchor = Clock::now();
  }

  void pause() noexcept {
    if (!playing)
      return;
    position += elapsed();
    playing = false;
  }

  void stop() noexcept {
    playing = false;
    position = 0.0;
  }

  void seek(double seconds) noexcept {
    if (!std::isfinite(seconds))
      return;
    position = std::clamp(seconds, 0.0, AutomationModel::kMaxTimelineSeconds);
    if (playing)
      anchor = Clock::now();
  }

  double getPosition() const noexcept {
    return std::clamp(position + elapsed(), 0.0,
                      AutomationModel::kMaxTimelineSeconds);
  }

  bool isPlaying() const noexcept { return playing; }

private:
  using Clock = std::chrono::steady_clock;

  double elapsed() const noexcept {
    if (!playing)
      return 0.0;
    return std::chrono::duration<double>(Clock::now() - anchor).count();
  }

  Clock::time_point anchor = Clock::now();
  double position = 0.0;
  bool playing = false;
};

} // namespace nova::graviton
