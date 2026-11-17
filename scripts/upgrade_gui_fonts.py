import re

file_path = "Source/GUI/MidasTouringConsoleSurface.h"

with open(file_path, "r", encoding="utf-8") as f:
    content = f.read()

# 1. Add namespace midas_ui after midas_colors
ui_namespace = """
namespace midas_ui {
// Global font scale factor: default 1.30f ensures all console text is large, crisp, and readable on high-res displays
inline float fontScale = 1.30f;

inline juce::Font getFont(float baseSize, bool bold = false, bool italic = false) {
  const float scaled = std::max(10.0f, baseSize * fontScale);
  auto f = juce::Font(juce::FontOptions(scaled, bold ? juce::Font::bold : juce::Font::plain));
  if (italic)
    f = f.italicised();
  return f;
}
} // namespace midas_ui
"""

if "namespace midas_ui" not in content:
    target = "} // namespace midas_colors"
    content = content.replace(target, target + "\n" + ui_namespace)

# 2. Replace pattern for italic Font
pattern_italic = re.compile(r'juce::Font\s*\(\s*juce::FontOptions\s*\(\s*([0-9\.]+)f\s*,\s*juce::Font::(bold|plain)\s*\)\s*\)\.italicised\(\)')
def repl_italic(m):
    is_bold = "true" if m.group(2) == "bold" else "false"
    return f"midas_ui::getFont({m.group(1)}f, {is_bold}, true)"

content = pattern_italic.sub(repl_italic, content)

# 3. Replace pattern for normal Font
pattern_normal = re.compile(r'juce::Font\s*\(\s*juce::FontOptions\s*\(\s*([0-9\.]+)f\s*,\s*juce::Font::(bold|plain)\s*\)\s*\)')
def repl_normal(m):
    is_bold = "true" if m.group(2) == "bold" else "false"
    return f"midas_ui::getFont({m.group(1)}f, {is_bold})"

content = pattern_normal.sub(repl_normal, content)

# 4. Handle line 2870 special ternary case
content = re.sub(
    r'juce::Font\s*\(\s*juce::FontOptions\s*\(\s*8\.0f\s*,\s*isActive\s*\?\s*juce::Font::bold\s*:\s*juce::Font::plain\s*\)\s*\)',
    'midas_ui::getFont(8.0f, isActive)',
    content
)

# 5. Add btnTextScale to MidasTopStatusBar
if "btnTextScale" not in content:
    # Add member to MidasTopStatusBar
    target_member = "MidasTactileButton btnAiModal, btnMCloudModal, btnDspRack, btnGangModal;"
    replacement_member = "MidasTactileButton btnAiModal, btnMCloudModal, btnDspRack, btnGangModal, btnTextScale;"
    content = content.replace(target_member, replacement_member)

    # In MidasTopStatusBar constructor
    target_ctor = 'btnGangModal("GANG FADER", MidasTactileButton::GlowMode::Cyan) {'
    replacement_ctor = 'btnGangModal("GANG FADER", MidasTactileButton::GlowMode::Cyan),\n        btnTextScale("TEXT: 130%", MidasTactileButton::GlowMode::Cyan) {\n    addAndMakeVisible(btnTextScale);\n    btnTextScale.setClickingTogglesState(false);'
    content = content.replace(target_ctor, replacement_ctor)

    # In MidasTopStatusBar::resized()
    target_resized = "btnGangModal.setBounds(static_cast<int>(bx), 5, 86, 20);"
    replacement_resized = """btnGangModal.setBounds(static_cast<int>(bx), 5, 86, 20);
    bx += 90.0f;
    btnTextScale.setBounds(static_cast<int>(bx), 5, 88, 20);

    btnTextScale.onClick = [this] {
      if (midas_ui::fontScale < 1.15f) {
        midas_ui::fontScale = 1.30f;
        btnTextScale.setButtonText("TEXT: 130%");
      } else if (midas_ui::fontScale < 1.40f) {
        midas_ui::fontScale = 1.50f;
        btnTextScale.setButtonText("TEXT: 150%");
      } else if (midas_ui::fontScale < 1.60f) {
        midas_ui::fontScale = 1.00f;
        btnTextScale.setButtonText("TEXT: 100%");
      } else {
        midas_ui::fontScale = 1.15f;
        btnTextScale.setButtonText("TEXT: 115%");
      }
      if (auto *top = getTopLevelComponent())
        top->repaint();
      else if (auto *parent = getParentComponent())
        parent->repaint();
      else
        repaint();
    };"""
    content = content.replace(target_resized, replacement_resized)

with open(file_path, "w", encoding="utf-8") as f:
    f.write(content)

print("Upgrade complete! Replaced occurrences and added btnTextScale.")

