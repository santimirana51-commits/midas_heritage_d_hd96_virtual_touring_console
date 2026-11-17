#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

namespace nova {

class Meter final : public juce::Component {
public:
  explicit Meter(std::atomic<float> &source) : value(source) {}
  void paint(juce::Graphics &g) override {
    auto r = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff171b20));
    g.fillRoundedRectangle(r, 3.0f);
    const float v =
        juce::jlimit(0.0f, 1.0f, value.load(std::memory_order_relaxed));
    auto fill = r.withHeight(r.getHeight() * v);
    fill.setY(r.getBottom() - fill.getHeight());
    g.setColour(v > 0.90f   ? juce::Colour(0xfff05a67)
                : v > 0.65f ? juce::Colour(0xffffc857)
                            : juce::Colour(0xff48d597));
    g.fillRoundedRectangle(fill, 2.0f);
  }

private:
  std::atomic<float> &value;
};

} // namespace nova
