import os

path = r"Source/GUI/MidasTouringConsoleSurface.h"
with open(path, "r", encoding="utf-8") as f:
    content = f.read()

# 1. Update MidasFader paint to render realistic brushed aluminum metallic cap matching reference image
old_fader_paint = """    // Drop Shadow
    g.setColour(juce::Colour(0x95000000));
    g.fillRoundedRectangle(capRect.translated(0.0f, 3.0f), 4.0f);

    // Metallic Touring Gradient
    juce::ColourGradient capGrad(juce::Colour(0xff2d3b4e), capX, capY,
                                 juce::Colour(0xff0e141d), capX, capY + capH, false);
    capGrad.addColour(0.35, juce::Colour(0xff222e3d));
    capGrad.addColour(0.65, juce::Colour(0xff161f2c));
    g.setGradientFill(capGrad);
    g.fillRoundedRectangle(capRect, 3.5f);

    // Top Bevel Highlight
    g.setColour(juce::Colour(0xff556b87));
    g.drawHorizontalLine(static_cast<int>(capY + 1.0f), capX + 2.0f, capX + capW - 2.0f);

    // Cap Outline
    g.setColour(juce::Colour(0xff1e2a3a));
    g.drawRoundedRectangle(capRect, 3.5f, 1.0f);

    // Signature Glowing Center Stripe
    const float lineY = capY + capH * 0.5f;
    const float lineW = capW - 8.0f;
    const float lineX = capX + 4.0f;

    if (isMaster) {
      g.setColour(juce::Colour(0x70ff9f0a));
      g.fillRect(lineX - 1.0f, lineY - 2.0f, lineW + 2.0f, 4.0f);
      g.setColour(midas_colors::kAccentGold);
      g.fillRect(lineX, lineY - 1.0f, lineW, 2.0f);
    } else {
      g.setColour(juce::Colour(0x60ffffff));
      g.fillRect(lineX - 1.0f, lineY - 2.0f, lineW + 2.0f, 4.0f);
      g.setColour(juce::Colours::white);
      g.fillRect(lineX, lineY - 1.0f, lineW, 2.0f);
    }"""

new_fader_paint = """    // Deep Drop Shadow behind metallic cap
    g.setColour(juce::Colour(0xb8000000));
    g.fillRoundedRectangle(capRect.translated(0.0f, 3.5f), 4.0f);

    // Multi-stage Brushed Aluminum Metallic Gradient (Touring Spec)
    juce::ColourGradient capGrad(juce::Colour(0xffadbccb), capX, capY,
                                 juce::Colour(0xff4b5866), capX, capY + capH, false);
    capGrad.addColour(0.08, juce::Colour(0xffc2cedc)); // top chamfer highlight
    capGrad.addColour(0.35, juce::Colour(0xff8696a6)); // upper body
    capGrad.addColour(0.48, juce::Colour(0xff576675)); // groove upper bevel
    capGrad.addColour(0.52, juce::Colour(0xff3f4c59)); // groove center well
    capGrad.addColour(0.65, juce::Colour(0xff687888)); // groove lower bevel
    capGrad.addColour(0.85, juce::Colour(0xff525f6e)); // lower body
    capGrad.addColour(0.98, juce::Colour(0xff343e49)); // bottom chamfer shadow
    g.setGradientFill(capGrad);
    g.fillRoundedRectangle(capRect, 3.5f);

    // Tactile Ergonomic Finger Indentation Bevels
    const float grooveY = capY + capH * 0.5f;
    g.setColour(juce::Colour(0x60000000));
    g.drawHorizontalLine(static_cast<int>(grooveY - 3.0f), capX + 2.0f, capX + capW - 2.0f);
    g.setColour(juce::Colour(0x35ffffff));
    g.drawHorizontalLine(static_cast<int>(grooveY + 4.0f), capX + 2.0f, capX + capW - 2.0f);

    // Top Bevel Specular Line
    g.setColour(juce::Colour(0xd0ffffff));
    g.drawHorizontalLine(static_cast<int>(capY + 1.0f), capX + 3.0f, capX + capW - 3.0f);

    // Lateral Chamfers / Rim Highlight
    g.setColour(juce::Colour(0x60ffffff));
    g.drawVerticalLine(static_cast<int>(capX + 1.0f), capY + 3.0f, capY + capH - 3.0f);
    g.setColour(juce::Colour(0x35000000));
    g.drawVerticalLine(static_cast<int>(capX + capW - 1.0f), capY + 3.0f, capY + capH - 3.0f);

    // Cap Outline
    g.setColour(juce::Colour(0xff222a33));
    g.drawRoundedRectangle(capRect, 3.5f, 1.0f);

    // Signature Glowing Center Stripe (Crisp White for Channel Faders, Amber for Master)
    const float lineY = capY + capH * 0.5f;
    const float lineW = capW - 8.0f;
    const float lineX = capX + 4.0f;

    if (isMaster) {
      g.setColour(juce::Colour(0x70ffb300));
      g.fillRect(lineX - 1.0f, lineY - 2.0f, lineW + 2.0f, 4.0f);
      g.setColour(juce::Colour(0xffffb300));
      g.fillRect(lineX, lineY - 1.0f, lineW, 2.0f);
    } else {
      g.setColour(juce::Colour(0x70ffffff));
      g.fillRect(lineX - 1.0f, lineY - 2.0f, lineW + 2.0f, 4.0f);
      g.setColour(juce::Colours::white);
      g.fillRect(lineX, lineY - 1.0f, lineW, 2.0f);
    }"""

if old_fader_paint in content:
    content = content.replace(old_fader_paint, new_fader_paint)
    print("MidasFader paint updated to brushed aluminum metallic style.")
else:
    print("WARNING: old_fader_paint not found!")

# 2. Add drawConsoleScrew helper if not present
screw_func = """namespace midas_ui {
inline void drawConsoleScrew(juce::Graphics &g, float cx, float cy, float radius = 3.5f) {
  g.setColour(juce::Colour(0xff121822));
  g.fillEllipse(cx - radius - 0.5f, cy - radius - 0.5f, (radius + 0.5f) * 2.0f, (radius + 0.5f) * 2.0f);
  juce::ColourGradient grad(juce::Colour(0xff6e7e90), cx - radius * 0.5f, cy - radius * 0.5f,
                            juce::Colour(0xff364454), cx + radius * 0.5f, cy + radius * 0.5f, false);
  g.setGradientFill(grad);
  g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
  g.setColour(juce::Colour(0x60ffffff));
  g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 0.6f);
  g.setColour(juce::Colour(0xff161e28));
  g.drawLine(cx - radius * 0.6f, cy, cx + radius * 0.6f, cy, 1.0f);
  g.drawLine(cx, cy - radius * 0.6f, cx, cy + radius * 0.6f, 1.0f);
}
} // namespace midas_ui
"""

if "drawConsoleScrew" not in content:
    # insert before class MidasTactileButton
    pos = content.find("class MidasTactileButton")
    if pos != -1:
        content = content[:pos] + screw_func + "\n" + content[pos:]
        print("drawConsoleScrew added.")
    else:
        print("WARNING: class MidasTactileButton not found!")

# 3. Add screws to MidasMasterStrip::paint
old_master_border = """    const auto accentCol = isMonitorMode ? midas_colors::kAccentCyan : midas_colors::kAccentGold;
    g.setColour(accentCol);
    g.drawRoundedRectangle(cardRect, 8.0f, 1.5f);"""

new_master_border = """    const auto accentCol = isMonitorMode ? midas_colors::kAccentCyan : midas_colors::kAccentGold;
    g.setColour(accentCol);
    g.drawRoundedRectangle(cardRect, 8.0f, 1.5f);

    // Realistic Touring Chassis Screws
    midas_ui::drawConsoleScrew(g, w - 7.0f, 8.0f, 3.2f);
    midas_ui::drawConsoleScrew(g, w - 7.0f, h - 8.0f, 3.2f);"""

if old_master_border in content:
    content = content.replace(old_master_border, new_master_border)
    print("Master strip screws added.")
else:
    print("WARNING: old_master_border not found!")

# 4. Clean UTF-8 infinity symbol & zero tick line in Master Strip scale
old_scale_table = """    static const ScaleMark marks[] = {
      { 10.0f, "+10", midas_colors::kAccentRed },
      {  0.0f,   "0", midas_colors::kAccentGold },
      { -5.0f,  "-5", juce::Colour(0xff7a8fa6) },
      {-10.0f, "-10", juce::Colour(0xff7a8fa6) },
      {-20.0f, "-20", juce::Colour(0xff7a8fa6) },
      {-30.0f, "-30", juce::Colour(0xff7a8fa6) },
      {-90.0f, "-âˆž", juce::Colour(0xff7a8fa6) }
    };

    g.setFont(midas_ui::getFont(7.5f, true));
    for (const auto &m : marks) {
      const float norm = (m.db <= -90.0f) ? 0.0f : (m.db >= 10.0f ? 1.0f : (m.db >= 0.0f ? 0.75f + (m.db / 10.0f) * 0.25f : std::pow((m.db + 90.0f) / 90.0f, 2.2f) * 0.75f));
      const float y = (faderBottom - 8.0f) - norm * trackH;
      g.setColour(m.col);
      g.drawText(m.label, static_cast<int>(faderRight + 3.0f), static_cast<int>(y - 6.0f), 24, 12, juce::Justification::centredLeft);
    }"""

new_scale_table = """    static const ScaleMark marks[] = {
      { 10.0f, "+10", midas_colors::kAccentRed },
      {  0.0f,   "0", midas_colors::kAccentGold },
      { -5.0f,  "-5", juce::Colour(0xff7a8fa6) },
      {-10.0f, "-10", juce::Colour(0xff7a8fa6) },
      {-20.0f, "-20", juce::Colour(0xff7a8fa6) },
      {-30.0f, "-30", juce::Colour(0xff7a8fa6) },
      {-90.0f, "-inf", juce::Colour(0xff7a8fa6) }
    };

    g.setFont(midas_ui::getFont(7.5f, true));
    for (const auto &m : marks) {
      const float norm = (m.db <= -90.0f) ? 0.0f : (m.db >= 10.0f ? 1.0f : (m.db >= 0.0f ? 0.75f + (m.db / 10.0f) * 0.25f : std::pow((m.db + 90.0f) / 90.0f, 2.2f) * 0.75f));
      const float y = (faderBottom - 8.0f) - norm * trackH;
      g.setColour(m.col);
      if (std::abs(m.db) < 0.1f) {
        g.drawLine(faderRight - 2.0f, y, faderRight + 2.0f, y, 1.5f);
      }
      g.drawText(m.label, static_cast<int>(faderRight + 3.0f), static_cast<int>(y - 6.0f), 24, 12, juce::Justification::centredLeft);
    }"""

if old_scale_table in content:
    content = content.replace(old_scale_table, new_scale_table)
    print("Scale markings updated with zero tick and clean label.")
else:
    # Try finding just -âˆž
    if "-âˆž" in content:
        content = content.replace("-âˆž", "-inf")
        print("Replaced corrupted UTF-8 infinity with -inf.")

with open(path, "w", encoding="utf-8") as f:
    f.write(content)
print("Done refining surface to reference mockup.")

