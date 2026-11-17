#pragma once
#include "GUI/NovaConsoleSurfaceModel.h"
#include <functional>
#if __has_include(<juce_gui_basics/juce_gui_basics.h>)
 #include <juce_gui_basics/juce_gui_basics.h>
 namespace nova::ui {
 // Responsive touchscreen surface. The host owns engine synchronization.
 class NovaConsoleSurfaceJuce final : public juce::Component {
 public:
  explicit NovaConsoleSurfaceJuce(NovaConsoleSurfaceModel& model):model_(model){setWantsKeyboardFocus(true);}
  std::function<void(int,float)> onFaderChanged;
  std::function<void(int,bool)> onMuteChanged;
  std::function<void(int)> onChannelSelected;
  void paint(juce::Graphics& g) override {
   g.fillAll(juce::Colour(0xff111820)); const int w=getWidth(),h=getHeight();
   const float stripW=static_cast<float>(w)/kSurfaceFaders;
   for(int i=0;i<kSurfaceFaders;++i){const float x=i*stripW;auto&s=model_.scribble(i);
    g.setColour(juce::Colour(0xff202b35));g.fillRoundedRectangle(x+2,4,stripW-4,static_cast<float>(h)-8,4);
    g.setColour(s.selected?juce::Colour(0xff36c5c0):juce::Colour(0xffaab7c4));
    g.drawText(juce::String(s.name.data()),static_cast<int>(x+3),8,static_cast<int>(stripW-6),22,juce::Justification::centred);
    if(s.solo){g.setColour(juce::Colour(0xffffc857));g.fillEllipse(x+stripW-15.0f,10.0f,8.0f,8.0f);}
    const float meterH=juce::jmap(juce::jlimit(-60.0f,0.0f,s.peakDb),-60.0f,0.0f,0.0f,static_cast<float>(h)*0.42f);
    g.setColour(s.peakDb>-3.0f?juce::Colour(0xffef5350):s.peakDb>-12.0f?juce::Colour(0xffffb74d):juce::Colour(0xff34c77b));
    g.fillRect(x+stripW*0.25f,36.0f,stripW*0.16f,meterH);
    const float faderY=static_cast<float>(h)*0.52f;g.setColour(juce::Colours::darkgrey);g.fillRoundedRectangle(x+stripW*0.62f,faderY,stripW*0.12f,static_cast<float>(h)*0.35f,3);
    const float knobY=juce::jmap(model_.fader(i),-60.0f,10.0f,static_cast<float>(h)*0.84f,static_cast<float>(h)*0.54f);
    g.setColour(juce::Colours::white);g.fillRoundedRectangle(x+stripW*0.48f,knobY,stripW*0.40f,10.0f,3);
    auto muteBounds=juce::Rectangle<float>(x+stripW*.18f,static_cast<float>(h-30),stripW*.64f,22.0f);
    g.setColour(s.muted?juce::Colour(0xffb8323c):juce::Colour(0xff303c46));g.fillRoundedRectangle(muteBounds,3.0f);
    g.setColour(juce::Colours::white);g.drawText("MUTE",muteBounds.toNearestInt(),juce::Justification::centred);
   }
  }
  void resized() override {}
  void mouseDown(const juce::MouseEvent&e) override {
   const int f=juce::jlimit(0,kSurfaceFaders-1,static_cast<int>(e.position.x*kSurfaceFaders/std::max(1,getWidth())));
   const int channel=model_.channelForFader(f);
   if(channel<0)return;
   for(int i=0;i<kSurfaceFaders;++i)model_.setSelected(i,i==f);
   if(onChannelSelected)onChannelSelected(channel);
   if(e.position.y>=getHeight()-34||e.mods.isRightButtonDown()){
    auto&s=model_.scribble(f);s.muted=!s.muted;
    if(onMuteChanged)onMuteChanged(channel,s.muted);
    repaint();return;
   }
   dragFader_=f;mouseDrag(e);repaint();
  }
  void mouseDrag(const juce::MouseEvent&e) override {
   if(dragFader_<0)return;
   const float norm=juce::jlimit(0.0f,1.0f,1.0f-e.position.y/std::max(1,getHeight()));
   const float db=-60.0f+norm*70.0f;
   model_.setFader(dragFader_,db);
   const int channel=model_.channelForFader(dragFader_);
   if(channel>=0&&onFaderChanged)onFaderChanged(channel,db);
   repaint();
  }
  void mouseUp(const juce::MouseEvent&) override {dragFader_=-1;}
 private:NovaConsoleSurfaceModel&model_;int dragFader_=-1;
 };
 }
#endif
