#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

namespace nova {

class SpatialRadarDisplay final : public juce::Component {
public:
  std::function<void(float newX, float newY)> onPositionChanged;

  SpatialRadarDisplay() { setRepaintsOnMouseActivity(true); }

  void setPosition(float x, float y, float z) {
    posX = std::clamp(x, -1.0f, 1.0f);
    posY = std::clamp(y, -1.0f, 1.0f);
    posZ = std::clamp(z, 0.0f, 1.0f);
    repaint();
  }

  void setMode(nova::graviton::SurroundMode m) {
    mode = m;
    repaint();
  }

  void setSpeakerGains(const nova::graviton::SurroundSpeakerGains &gains) {
    speakerGains = gains;
    repaint();
  }

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat().reduced(15.0f);
    g.fillAll(juce::Colour(0xff161b22));

    // Outer boundary
    g.setColour(juce::Colour(0xff30363d));
    g.drawRoundedRectangle(bounds, 12.0f, 1.5f);

    const auto center = bounds.getCentre();
    const float radius =
        std::min(bounds.getWidth(), bounds.getHeight()) * 0.42f;

    // Concentric distance rings
    g.setColour(juce::Colour(0xff21262d));
    g.drawEllipse(center.x - radius, center.y - radius, radius * 2.0f,
                  radius * 2.0f, 1.5f);
    g.drawEllipse(center.x - radius * 0.75f, center.y - radius * 0.75f,
                  radius * 1.5f, radius * 1.5f, 1.0f);
    g.drawEllipse(center.x - radius * 0.5f, center.y - radius * 0.5f, radius,
                  radius, 1.0f);
    g.drawEllipse(center.x - radius * 0.25f, center.y - radius * 0.25f,
                  radius * 0.5f, radius * 0.5f, 1.0f);

    // Crosshairs
    g.setColour(juce::Colour(0xff30363d));
    static const float kCrosshairDashes[] = {4.0f, 4.0f};
    g.drawDashedLine(juce::Line<float>(center.x - radius * 1.05f, center.y,
                                       center.x + radius * 1.05f, center.y),
                     kCrosshairDashes, 2, 1.0f);
    g.drawDashedLine(juce::Line<float>(center.x, center.y - radius * 1.05f,
                                       center.x, center.y + radius * 1.05f),
                     kCrosshairDashes, 2, 1.0f);

    // Cardinal Labels
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xff8b949e));
    g.drawText("FRONT (+Y)", static_cast<int>(center.x - 50.0f),
               static_cast<int>(center.y - radius - 22.0f), 100, 18,
               juce::Justification::centred);
    g.drawText("REAR (-Y)", static_cast<int>(center.x - 50.0f),
               static_cast<int>(center.y + radius + 4.0f), 100, 18,
               juce::Justification::centred);
    g.drawText("LEFT (-X)", static_cast<int>(center.x - radius - 70.0f),
               static_cast<int>(center.y - 9.0f), 65, 18,
               juce::Justification::centredRight);
    g.drawText("RIGHT (+X)", static_cast<int>(center.x + radius + 5.0f),
               static_cast<int>(center.y - 9.0f), 65, 18,
               juce::Justification::centredLeft);

    // Speaker Positions and Halos
    struct SpkDef {
      nova::graviton::SurroundSpeaker spk;
      float sx, sy;
      const char *label;
      bool isHeight;
    };

    static const SpkDef spkDefs[] = {
        {nova::graviton::SurroundSpeaker::L, -0.707f, 0.707f, "L", false},
        {nova::graviton::SurroundSpeaker::R, 0.707f, 0.707f, "R", false},
        {nova::graviton::SurroundSpeaker::C, 0.000f, 1.000f, "C", false},
        {nova::graviton::SurroundSpeaker::LFE, -0.250f, 0.880f, "SUB", false},
        {nova::graviton::SurroundSpeaker::Lss, -1.000f, 0.000f, "Lss", false},
        {nova::graviton::SurroundSpeaker::Rss, 1.000f, 0.000f, "Rss", false},
        {nova::graviton::SurroundSpeaker::Lsr, -0.707f, -0.707f, "Lsr", false},
        {nova::graviton::SurroundSpeaker::Rsr, 0.707f, -0.707f, "Rsr", false},
        {nova::graviton::SurroundSpeaker::TFL, -0.500f, 0.500f, "TFL", true},
        {nova::graviton::SurroundSpeaker::TFR, 0.500f, 0.500f, "TFR", true},
        {nova::graviton::SurroundSpeaker::TRL, -0.500f, -0.500f, "TRL", true},
        {nova::graviton::SurroundSpeaker::TRR, 0.500f, -0.500f, "TRR", true}};

    for (const auto &sd : spkDefs) {
      const size_t sIdx = static_cast<size_t>(sd.spk);
      const float spkGain = static_cast<float>(speakerGains[sIdx]);

      // Screen coordinate for speaker: Note that screen Y is downward, so Front
      // (+Y) is -radius * sy
      const float sx = center.x + sd.sx * radius;
      const float sy = center.y - sd.sy * radius;

      // Check if active in current mode
      bool activeInMode = false;
      switch (mode) {
      case nova::graviton::SurroundMode::Stereo:
        activeInMode = (sd.spk == nova::graviton::SurroundSpeaker::L ||
                        sd.spk == nova::graviton::SurroundSpeaker::R);
        break;
      case nova::graviton::SurroundMode::LCR:
        activeInMode = (sd.spk == nova::graviton::SurroundSpeaker::L ||
                        sd.spk == nova::graviton::SurroundSpeaker::R ||
                        sd.spk == nova::graviton::SurroundSpeaker::C);
        break;
      case nova::graviton::SurroundMode::Quad_4_0:
        activeInMode = (sd.spk == nova::graviton::SurroundSpeaker::L ||
                        sd.spk == nova::graviton::SurroundSpeaker::R ||
                        sd.spk == nova::graviton::SurroundSpeaker::Lsr ||
                        sd.spk == nova::graviton::SurroundSpeaker::Rsr);
        break;
      case nova::graviton::SurroundMode::Surround_5_1:
        activeInMode = (sd.spk == nova::graviton::SurroundSpeaker::L ||
                        sd.spk == nova::graviton::SurroundSpeaker::R ||
                        sd.spk == nova::graviton::SurroundSpeaker::C ||
                        sd.spk == nova::graviton::SurroundSpeaker::LFE ||
                        sd.spk == nova::graviton::SurroundSpeaker::Lss ||
                        sd.spk == nova::graviton::SurroundSpeaker::Rss);
        break;
      case nova::graviton::SurroundMode::Surround_7_1:
        activeInMode = !sd.isHeight;
        break;
      case nova::graviton::SurroundMode::Immersive_7_1_4:
        activeInMode = true;
        break;
      }

      const float nodeR = sd.isHeight ? 11.0f : 14.0f;

      // Glow halo proportional to gain
      if (activeInMode && spkGain > 0.01f) {
        const float haloR = nodeR + spkGain * 14.0f;
        g.setColour(juce::Colour(sd.isHeight ? 0xffa371f7 : 0xff1f6feb)
                        .withAlpha(std::clamp(spkGain * 0.65f, 0.15f, 0.85f)));
        g.fillEllipse(sx - haloR, sy - haloR, haloR * 2.0f, haloR * 2.0f);
      }

      // Speaker body
      g.setColour(activeInMode ? juce::Colour(0xff21262d)
                               : juce::Colour(0xff161b22));
      g.fillEllipse(sx - nodeR, sy - nodeR, nodeR * 2.0f, nodeR * 2.0f);

      g.setColour(activeInMode ? (sd.isHeight ? juce::Colour(0xffbc8cff)
                                              : juce::Colour(0xff58a6ff))
                               : juce::Colour(0xff484f58));
      g.drawEllipse(sx - nodeR, sy - nodeR, nodeR * 2.0f, nodeR * 2.0f,
                    sd.isHeight ? 1.2f : 1.8f);

      // Label
      g.setFont(juce::Font(
          juce::FontOptions(sd.isHeight ? 9.0f : 10.0f, juce::Font::bold)));
      g.setColour(activeInMode ? juce::Colour(0xfff0f3f6)
                               : juce::Colour(0xff484f58));
      g.drawText(sd.label, static_cast<int>(sx - nodeR),
                 static_cast<int>(sy - nodeR), static_cast<int>(nodeR * 2.0f),
                 static_cast<int>(nodeR * 2.0f), juce::Justification::centred);
    }

    // Draggable Spatial Audio Object Puck
    const float px = center.x + posX * radius;
    const float py = center.y - posY * radius;
    const float puckR = 15.0f + posZ * 7.0f; // Puck grows with height Z

    // Outer glow
    g.setColour(juce::Colour(0xffffa657).withAlpha(0.35f));
    g.fillEllipse(px - puckR * 1.5f, py - puckR * 1.5f, puckR * 3.0f,
                  puckR * 3.0f);

    // Puck body
    g.setColour(juce::Colour(0xfff0883e));
    g.fillEllipse(px - puckR, py - puckR, puckR * 2.0f, puckR * 2.0f);
    g.setColour(juce::Colour(0xffffffff));
    g.drawEllipse(px - puckR, py - puckR, puckR * 2.0f, puckR * 2.0f, 2.0f);

    // Crosshair in puck
    g.setColour(juce::Colour(0xff161b22));
    g.drawLine(px - puckR * 0.6f, py, px + puckR * 0.6f, py, 1.5f);
    g.drawLine(px, py - puckR * 0.6f, px, py + puckR * 0.6f, 1.5f);

    // Puck Coordinate Badge
    juce::String badge;
    badge << "X: " << juce::String(posX, 2) << "  Y: " << juce::String(posY, 2)
          << "  Z: " << juce::String(posZ, 2);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.setColour(juce::Colour(0xfff0883e));
    g.drawText(badge, static_cast<int>(px - 60.0f),
               static_cast<int>(py - puckR - 20.0f), 120, 18,
               juce::Justification::centred);
  }

  void mouseDown(const juce::MouseEvent &e) override { handleMouse(e); }
  void mouseDrag(const juce::MouseEvent &e) override { handleMouse(e); }

private:
  float posX = 0.0f, posY = 1.0f, posZ = 0.0f;
  nova::graviton::SurroundMode mode =
      nova::graviton::SurroundMode::Surround_5_1;
  nova::graviton::SurroundSpeakerGains speakerGains{};

  void handleMouse(const juce::MouseEvent &e) {
    auto bounds = getLocalBounds().toFloat().reduced(15.0f);
    const auto center = bounds.getCentre();
    const float radius =
        std::min(bounds.getWidth(), bounds.getHeight()) * 0.42f;

    if (radius > 1.0f) {
      float nx = (e.position.x - center.x) / radius;
      float ny = (center.y - e.position.y) / radius;

      posX = std::clamp(nx, -1.0f, 1.0f);
      posY = std::clamp(ny, -1.0f, 1.0f);
      repaint();

      if (onPositionChanged)
        onPositionChanged(posX, posY);
    }
  }
};

} // namespace nova
