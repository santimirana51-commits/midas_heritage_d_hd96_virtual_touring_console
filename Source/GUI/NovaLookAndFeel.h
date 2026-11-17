#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace nova {

class NovaLookAndFeel final : public juce::LookAndFeel_V4 {
public:
  NovaLookAndFeel() {
    const auto background = juce::Colour(0xff202123);
    const auto panel = juce::Colour(0xff292a2c);
    const auto control = juce::Colour(0xff343638);
    const auto border = juce::Colour(0xff424446);
    const auto accent = juce::Colour(0xff49a5c8);
    const auto text = juce::Colour(0xffd5d7d8);
    setColour(juce::ResizableWindow::backgroundColourId,
              background);
    setColour(juce::Label::textColourId, text);
    setColour(juce::TextButton::buttonColourId, control);
    setColour(juce::TextButton::buttonOnColourId, accent);
    setColour(juce::TextButton::textColourOffId, text);
    setColour(juce::TextButton::textColourOnId, background);
    setColour(juce::ComboBox::backgroundColourId, panel);
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::outlineColourId, border);
    setColour(juce::ComboBox::arrowColourId, accent);
    setColour(juce::Slider::trackColourId, accent);
    setColour(juce::Slider::thumbColourId, juce::Colour(0xffe6e8e9));
    setColour(juce::Slider::rotarySliderOutlineColourId,
              border);
    setColour(juce::Slider::rotarySliderFillColourId, accent);
    setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,
              accent);
    setColour(juce::PopupMenu::highlightedTextColourId, background);
    setColour(juce::TextEditor::backgroundColourId, panel);
    setColour(juce::TextEditor::textColourId, text);
    setColour(juce::TextEditor::outlineColourId, border);
  }

  void drawButtonBackground(juce::Graphics &g, juce::Button &button,
                            const juce::Colour &backgroundColour,
                            bool highlighted, bool down) override {
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto colour = button.getToggleState()
                      ? button.findColour(juce::TextButton::buttonOnColourId)
                      : backgroundColour;
    if (down)
      colour = colour.darker(0.16f);
    else if (highlighted)
      colour = colour.brighter(0.12f);
    g.setColour(colour);
    g.fillRect(bounds);
    g.setColour(button.getToggleState() ? colour.brighter(0.2f)
                                        : juce::Colour(0xff424446));
    g.drawRect(bounds, 1.0f);
  }
};

} // namespace nova
