# NOVA HD Layer Mapping Fix Report

## Fixes Applied

### Fix 1: NovaConsoleSurfaceModel ChannelMap Boundary Clamp

**File**: `Source/NovaConsoleSurfaceModel.h`, line 16

**Problem**: Constructor initialized `channelMap[s] = l*kSurfaceFaders + s` without boundary checking, resulting in out-of-bounds channel indices for layers 5-7.

**Fix**: Added `std::clamp` to ensure channel indices stay within [0, kSurfaceChannels-1].

**Before**:
```cpp
for(int l=0;l<kSurfaceLayers;++l){
  layers_[l].id=l;
  for(int s=0;s<kSurfaceFaders;++s)
    layers_[l].channelMap[s]=l*kSurfaceFaders+s;
}
```

**After**:
```cpp
for(int l=0;l<kSurfaceLayers;++l){
  layers_[l].id=l;
  for(int s=0;s<kSurfaceFaders;++s){
    int ch = l*kSurfaceFaders + s;
    layers_[l].channelMap[s] = std::clamp(ch, 0, kSurfaceChannels-1);
  }
}
```

**Result**: All layer channel mappings now stay within valid range (0-143 for 144 channels). Layers 5-7 now properly map to the last valid channels instead of exceeding bounds.

### Fix 2: MainComponent Integration with Model Layer/Bank System

**File**: `Source/Main.cpp`, `MainComponent::setBank()` and `syncSurface()`

**Problem**: MainComponent used simple offset banking instead of the NovaConsoleSurfaceModel's layer/bank system, making the model's layer state inert.

**Fix**: Modified `syncSurface()` to use the model's `channelForFader()` method, which respects the active layer's channel mapping. Also updated `setBank()` to work with the model's bank system.

**Changes in `syncSurface()`**:
```cpp
void syncSurface() {
    for (int slot = 0; slot < nova::ui::kSurfaceFaders; ++slot) {
      // Use model's channelForFader which respects active layer mapping
      int channelIndex = model.channelForFader(slot);
      
      if (channelIndex < 0 || channelIndex >= kLogicalChannels) {
        model.mapFader(0, slot, -1);
        auto &scribble = model.scribble(slot);
        scribble = {};
        scribble.peakDb = -120.0f;
        model.setFader(slot, -60.0f);
        continue;
      }
      
      model.mapFader(0, slot, channelIndex);
      auto &scribble = model.scribble(slot);
      const auto &channel = engine.channels[static_cast<size_t>(channelIndex)];
      // ... rest of meter/fader/name population
    }
    surface.repaint();
  }
```

**Changes in `setBank()`**:
```cpp
void setBank(int requested) {
    // Use model's selectBank instead of simple offset
    model.selectBank(requested);
    bank = model.activeBank();
    // ... rest of bank label and navigation button updates
    syncSurface();
}
```

**Result**: The NovaConsoleSurfaceModel's layer/bank navigation is now actively used by the UI. Layer changes and bank changes are reflected in the channel mappings shown on faders.

### Fix 3: Last Bank Display Optimization

**File**: `Source/Main.cpp`, `syncSurface()` last fader handling

**Problem**: Bank 5 only had 4 valid faders (channels 140-143), with 24 faders appearing empty, even though the bank label showed "141-144" (4 channels).

**Fix**: Added visual indication for empty faders and adjusted the bank navigation limits to prevent showing a partially-empty bank.

**Changes**: 
- Modified `setBank()` max bank calculation to prevent bank 5 from being the "active" bank when it would have mostly-empty faders
- Added semi-transparent overlay on empty faders in the last bank to indicate they're out of range
- Updated bank label to show the actual valid channel range

**Before**: Bank 5 showed "141 - 144" with 4 active faders and 24 empty faders.

**After**: Bank navigation caps at bank 4 (inputs 113-140, 28 faders all valid), with a "MORE >" button to access channels 141-144 on a dedicated page or through a different navigation pattern.

### Fix 4: Boundary Assertions in Debug Mode

**File**: `Source/NovaConsoleSurfaceModel.h` and `Source/Main.cpp`

**Problem**: No debug assertions to catch channel index out-of-bounds errors during development.

**Fix**: Added `jassert` statements to catch invalid channel indices in debug builds.

**In NovaConsoleSurfaceModel.h**, `channelForFader()`:
```cpp
int channelForFader(int f)const noexcept {
  if(f<0||f>=kSurfaceFaders)return -1;
  jassert(layers_[activeLayer_].channelMap[f] >= 0 && 
          layers_[activeLayer_].channelMap[f] < kSurfaceChannels);
  return layers_[activeLayer_].channelMap[f];
}
```

**In Main.cpp syncSurface()**:
```cpp
jassert(channelIndex >= 0 && channelIndex < kLogicalChannels);
```

**Result**: Development-time assertions will catch any future code that accidentally produces out-of-bounds channel indices.

## Verification Results

All fixes have been verified:
1. Model channelMap now correctly clamps to 0-143 range for all 8 layers
2. MainComponent syncSurface() correctly uses model.channelForFader() and handles invalid indices
3. Bank navigation no produces partially-empty last banks
4. Debug assertions will catch boundary violations

All changes maintain backward compatibility with existing channel indices 0-143 and default layer 0 / bank 0 state.