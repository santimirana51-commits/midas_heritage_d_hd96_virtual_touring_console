#pragma once
#include "Mixer/MixerEngine.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <functional>

namespace nova {

struct UserFaderLayout {
  juce::String name;
  std::vector<int> channels;
};

class UserFaderLayoutEditor final : public juce::Component {
public:
  using Layouts = std::array<UserFaderLayout, 8>;

  UserFaderLayoutEditor(Layouts layoutsToEdit,
                        std::function<void(const Layouts &)> onLayoutsChanged)
      : layouts(std::move(layoutsToEdit)),
        layoutsChanged(std::move(onLayoutsChanged)) {
    layoutSelector.addItemList(
        {"USER LAYOUT 1", "USER LAYOUT 2", "USER LAYOUT 3", "USER LAYOUT 4",
         "USER LAYOUT 5", "USER LAYOUT 6", "USER LAYOUT 7", "USER LAYOUT 8"},
        1);
    layoutSelector.setSelectedId(1, juce::dontSendNotification);
    layoutSelector.onChange = [this] {
      syncSelectedLayout();
      refreshMembers();
    };
    addAndMakeVisible(layoutSelector);

    layoutName.setTextToShowWhenEmpty("Layout name", juce::Colour(0xff9aa4af));
    layoutName.onTextChange = [this] {
      layouts[static_cast<size_t>(selectedLayout())].name =
          layoutName.getText().trim().substring(0, 32);
      notifyChanged();
    };
    addAndMakeVisible(layoutName);

    for (int channel = 1; channel <= kLogicalChannels; ++channel)
      channelToAdd.addItem("CH " + juce::String(channel), channel);
    channelToAdd.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(channelToAdd);

    addChannel.setButtonText("ADD");
    addChannel.onClick = [this] {
      auto &channels = layouts[static_cast<size_t>(selectedLayout())].channels;
      const int channel = channelToAdd.getSelectedId() - 1;
      if (channel < 0 || channel >= kLogicalChannels)
        return;
      if (std::find(channels.begin(), channels.end(), channel) !=
          channels.end()) {
        status.setText("Channel is already in this layout",
                       juce::dontSendNotification);
        return;
      }
      channels.push_back(channel);
      status.setText("Channel added", juce::dontSendNotification);
      refreshMembers();
      notifyChanged();
    };
    addAndMakeVisible(addChannel);

    memberSelector.onChange = [this] { updateMoveButtons(); };
    addAndMakeVisible(memberSelector);

    moveUp.setButtonText("MOVE UP");
    moveUp.onClick = [this] { moveSelected(-1); };
    addAndMakeVisible(moveUp);
    moveDown.setButtonText("MOVE DOWN");
    moveDown.onClick = [this] { moveSelected(1); };
    addAndMakeVisible(moveDown);
    removeChannel.setButtonText("REMOVE");
    removeChannel.onClick = [this] {
      auto &channels = layouts[static_cast<size_t>(selectedLayout())].channels;
      const int index = memberSelector.getSelectedId() - 1;
      if (index < 0 || index >= static_cast<int>(channels.size()))
        return;
      channels.erase(channels.begin() + index);
      refreshMembers(juce::jmax(0, index - 1));
      status.setText("Channel removed", juce::dontSendNotification);
      notifyChanged();
    };
    addAndMakeVisible(removeChannel);

    clearLayout.setButtonText("CLEAR LAYOUT");
    clearLayout.onClick = [this] {
      layouts[static_cast<size_t>(selectedLayout())].channels.clear();
      refreshMembers();
      status.setText("Layout cleared", juce::dontSendNotification);
      notifyChanged();
    };
    addAndMakeVisible(clearLayout);

    status.setColour(juce::Label::textColourId, juce::Colour(0xffc7d0d9));
    addAndMakeVisible(status);
    syncSelectedLayout();
    refreshMembers();
    setSize(520, 250);
  }

  void paint(juce::Graphics &graphics) override {
    graphics.fillAll(juce::Colour(0xff20262d));
    graphics.setColour(juce::Colour(0xfff0f3f6));
    graphics.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    graphics.drawText("CUSTOM FADER LAYOUT", 12, 8, getWidth() - 24, 24,
                      juce::Justification::centredLeft);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(12);
    area.removeFromTop(30);
    auto row = area.removeFromTop(28);
    layoutSelector.setBounds(row.removeFromLeft(180).reduced(2));
    layoutName.setBounds(row.reduced(2));
    area.removeFromTop(8);
    row = area.removeFromTop(28);
    channelToAdd.setBounds(row.removeFromLeft(330).reduced(2));
    addChannel.setBounds(row.reduced(2));
    area.removeFromTop(4);
    memberSelector.setBounds(area.removeFromTop(32).reduced(2));
    area.removeFromTop(4);
    row = area.removeFromTop(28);
    moveUp.setBounds(row.removeFromLeft(100).reduced(2));
    moveDown.setBounds(row.removeFromLeft(110).reduced(2));
    removeChannel.setBounds(row.removeFromLeft(100).reduced(2));
    clearLayout.setBounds(row.reduced(2));
    status.setBounds(area.removeFromTop(24).reduced(2));
  }

private:
  int selectedLayout() const noexcept {
    return juce::jlimit(0, static_cast<int>(layouts.size()) - 1,
                        layoutSelector.getSelectedId() - 1);
  }

  void syncSelectedLayout() {
    const auto &layout = layouts[static_cast<size_t>(selectedLayout())];
    layoutName.setText(layout.name.isNotEmpty()
                           ? layout.name
                           : "USER LAYOUT " +
                                 juce::String(selectedLayout() + 1),
                       juce::dontSendNotification);
    status.setText(juce::String(layout.channels.size()) +
                       " channels — order shown is fader order",
                   juce::dontSendNotification);
  }

  void refreshMembers(int selectedIndex = 0) {
    memberSelector.clear(juce::dontSendNotification);
    const auto &channels =
        layouts[static_cast<size_t>(selectedLayout())].channels;
    for (size_t index = 0; index < channels.size(); ++index)
      memberSelector.addItem(juce::String(index + 1) + ". CH " +
                                 juce::String(channels[index] + 1),
                             static_cast<int>(index + 1));
    if (!channels.empty())
      memberSelector.setSelectedId(
          juce::jlimit(1, static_cast<int>(channels.size()), selectedIndex + 1),
          juce::dontSendNotification);
    updateMoveButtons();
  }

  void updateMoveButtons() {
    const int index = memberSelector.getSelectedId() - 1;
    const auto &channels =
        layouts[static_cast<size_t>(selectedLayout())].channels;
    moveUp.setEnabled(index > 0 && index < static_cast<int>(channels.size()));
    moveDown.setEnabled(index >= 0 &&
                        index + 1 < static_cast<int>(channels.size()));
    removeChannel.setEnabled(index >= 0 &&
                             index < static_cast<int>(channels.size()));
    clearLayout.setEnabled(!channels.empty());
  }

  void moveSelected(int direction) {
    auto &channels = layouts[static_cast<size_t>(selectedLayout())].channels;
    const int index = memberSelector.getSelectedId() - 1;
    const int next = index + direction;
    if (index < 0 || next < 0 || index >= static_cast<int>(channels.size()) ||
        next >= static_cast<int>(channels.size()))
      return;
    std::swap(channels[static_cast<size_t>(index)],
              channels[static_cast<size_t>(next)]);
    refreshMembers(next);
    notifyChanged();
  }

  void notifyChanged() {
    if (layoutsChanged)
      layoutsChanged(layouts);
  }

  Layouts layouts;
  std::function<void(const Layouts &)> layoutsChanged;
  juce::ComboBox layoutSelector, channelToAdd, memberSelector;
  juce::TextEditor layoutName;
  juce::TextButton addChannel, moveUp, moveDown, removeChannel, clearLayout;
  juce::Label status;
};

} // namespace nova
