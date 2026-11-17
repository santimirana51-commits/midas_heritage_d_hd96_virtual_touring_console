#pragma once
#include <array>
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <string>

namespace nova::ui {
inline constexpr int kSurfaceFaders=28,kSurfaceLayers=4,kSurfaceBanks=2,kSurfaceChannels=32;
struct ScribbleStrip { std::array<char,24> name{}; std::uint32_t colour=0; bool selected=false,muted=false,solo=false; float peakDb=-120.0f; };
struct FaderState { float valueDb=0.0f; bool touched=false,motorEnabled=false; };
struct LayerState { int id=0; std::array<int,kSurfaceFaders> channelMap{}; };
// UI-thread-owned console model; audio metering is copied in from atomic meter snapshots.
class NovaConsoleSurfaceModel final {
public:
  NovaConsoleSurfaceModel() noexcept { for(int l=0;l<kSurfaceLayers;++l){layers_[l].id=l;for(int s=0;s<kSurfaceFaders;++s)layers_[l].channelMap[s]=l*kSurfaceFaders+s;} }
  void selectLayer(int l) noexcept {activeLayer_=std::clamp(l,0,kSurfaceLayers-1);}
  int activeLayer()const noexcept{return activeLayer_;}
  void selectBank(int b) noexcept {activeBank_=std::clamp(b,0,kSurfaceBanks-1);}
  int activeBank()const noexcept{return activeBank_;}
  int channelForFader(int f)const noexcept {if(f<0||f>=kSurfaceFaders)return -1;return layers_[activeLayer_].channelMap[f];}
  void mapFader(int layer,int f,int channel) noexcept {if(layer>=0&&layer<kSurfaceLayers&&f>=0&&f<kSurfaceFaders)layers_[layer].channelMap[f]=std::clamp(channel,-1,kSurfaceChannels-1);}
  void setFader(int f,float db) noexcept {if(f>=0&&f<kSurfaceFaders)faders_[f].valueDb=std::clamp(db,-120.0f,10.0f);}
  float fader(int f)const noexcept{return f>=0&&f<kSurfaceFaders?faders_[f].valueDb:-120.0f;}
  ScribbleStrip& scribble(int f) noexcept{return scribbles_[std::clamp(f,0,kSurfaceFaders-1)];}
  const ScribbleStrip& scribble(int f)const noexcept{return scribbles_[std::clamp(f,0,kSurfaceFaders-1)];}
  void setMeter(int f,float peakDb) noexcept {if(f>=0&&f<kSurfaceFaders)scribbles_[f].peakDb=std::clamp(peakDb,-120.0f,12.0f);}
  void setSelected(int f,bool v) noexcept {if(f>=0&&f<kSurfaceFaders)scribbles_[f].selected=v;}
  void setMotorEnabled(int f,bool v) noexcept {if(f>=0&&f<kSurfaceFaders)faders_[f].motorEnabled=v;}
  bool motorEnabled(int f)const noexcept{return f>=0&&f<kSurfaceFaders&&faders_[f].motorEnabled;}
  const LayerState& layer(int i)const noexcept{return layers_[std::clamp(i,0,kSurfaceLayers-1)];}
private:int activeLayer_=0,activeBank_=0;std::array<LayerState,kSurfaceLayers>layers_{};std::array<FaderState,kSurfaceFaders>faders_{};std::array<ScribbleStrip,kSurfaceFaders>scribbles_{};
};
} // namespace nova::ui
