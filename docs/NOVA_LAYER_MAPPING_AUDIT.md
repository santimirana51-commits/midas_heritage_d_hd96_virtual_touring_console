# NOVA HD Layer Mapping Audit Report

## Audit Scope
Audited: Main.cpp, NovaConsoleSurfaceModel.h, NovaConsoleSurfaceJuce.h
Goal: Identify empty layers, channel mapping issues, and boundary conditions.

## Architecture Overview

Two parallel mixer surface implementations exist:

1. **NovaConsoleSurfaceModel** (`NovaConsoleSurfaceModel.h`): Defines a 28-fader console model with 8 layers and 6 banks, supporting 144 logical channels.
   - `kSurfaceFaders = 28`, `kSurfaceLayers = 8`, `kSurfaceBanks = 6`, `kSurfaceChannels = 144`
   - `LayerState.channelMap[28]` maps fader index → channel index per layer
   - Model stores layer/bank state independently of the UI

2. **MainComponent** (`Main.cpp`): Renders the actual UI with 28 fader strips and bank navigation.
   - Uses simple offset banking: `channelIndex = bank * 28 + slot`
   - Does NOT use NovaConsoleSurfaceModel's `channelMap`, `selectLayer()`, or `channelForFader()`
   - Model state is effectively inert for the UI

## Issue 1: Model ChannelMap Out-of-Bounds Initialization

**File**: `NovaConsoleSurfaceModel.h`, line 16

**Problem**: The constructor initializes `channelMap[s] = l*kSurfaceFaders + s` for all 8 layers without checking channel boundaries.

**Initialization code**:
```cpp
for(int l=0;l<kSurfaceLayers;++l){
  layers_[l].id=l;
  for(int s=0;s<kSurfaceFaders;++s)
    layers_[l].channelMap[s]=l*kSurfaceFaders+s;
}
```

**Mapping result**:
| Layer | Fader 0→Channel | Fader 27→Channel | Valid? |
|-------|-----------------|------------------|--------|
| 0     | 0               | 27               | ✓      |
| 1     | 28              | 55               | ✓      |
| 2     | 56              | 83               | ✓      |
| 3     | 84              | 111              | ✓      |
| 4     | 112             | 139              | ✓      |
| 5     | 140             | 167              | ✗ (144-167 out of bounds) |
| 6     | 168             | 195              | ✗ (all out of bounds) |
| 7     | 196             | 223              | ✗ (all out of bounds) |

**Impact**: Layers 5, 6, 7 have channel mappings that exceed the 144-channel limit (indices 0-143). Any code that uses `channelForFader()` with these layers will receive invalid channel indices.

## Issue 2: Model Not Integrated with UI

**File**: `Main.cpp`, `MainComponent::syncSurface()` and `MainComponent constructor`

**Problem**: The MainComponent does not use the NovaConsoleSurfaceModel's layer/bank navigation system.

**Evidence**:
- `MainComponent::setBank()` (line 3423) uses simple offset: `bank = jlimit(0, (kLogicalChannels-1)/kSurfaceFaders, requested)` → max bank 5
- `MainComponent::syncSurface()` (line 3440) computes: `channelIndex = bank * kSurfaceFaders + slot`
- Model methods never called: `selectLayer()`, `selectBank()`, `channelForFader()`
- `model_` is constructed but its layer/bank state is never changed from defaults

**Result**: The NovaConsoleSurfaceModel's layer/bank system exists but is not used by the UI. The model's `activeLayer_` and `activeBank_` remain at 0.

## Issue 3: Last Bank Has Empty Faders

**File**: `Main.cpp`, `syncSurface()` lines 3443-3448

**Problem**: When bank 5 is selected (inputs 129-144), only 16 of 28 faders map to valid channels.

**Bank 5 range**: inputs 129-144 (16 channels)
- Fader 0→channel 129 ✓
- Fader 1→channel 130 ✓
- ...
- Fader 15→channel 144 ✓ (note: channel 144 = index 143 if 1-based, let me verify)

Actually, let me recalculate:
- kLogicalChannels = 144, channels indexed 0-143
- Bank 5: `bank * 28 = 5*28 = 140`, so channel indices 140-167
- But jlimit clamps to kLogicalChannels=144, so valid: 140, 141, 142, 143 (4 channels only!)

Wait, let me re-read the code:
```cpp
const int channelIndex = bank * nova::ui::kSurfaceFaders + slot;
if (channelIndex >= kLogicalChannels) {
  model.mapFader(0, slot, -1);
  // set to empty state
  continue;
}
```

So for bank 5:
- slot 0: channelIndex = 140 → valid (≤143)
- slot 1: channelIndex = 141 → valid
- slot 2: channelIndex = 142 → valid
- slot 3: channelIndex = 143 → valid
- slot 4: channelIndex = 144 → out of bounds → empty
- ...
- slot 27: channelIndex = 167 → out of bounds → empty

So bank 5 has only 4 valid faders ( slots 0-3 → channels 140-143 ), and 24 faders appear empty.

But wait, the bank label says "129 - 144" which suggests 16 channels. Let me re-read:
```cpp
bankLabel.setText("INPUTS  " +
                      juce::String(bank * kSurfaceFaders + 1) +  // bank 5: 129
                      " - " +
                      juce::String(juce::jmin(kLogicalChannels, (bank + 1) * kSurfaceFaders)),  // min(144, 168) = 144
                  ...
```
So the label shows "129 - 144" = 16 channels, but only channels 140-143 (indices 0-based, channels 141-144 in 1-based) are actually valid. There's a mismatch between the displayed range and the actual valid channels.

Actually wait - the channel index computation is `bank * 28 + slot`. For bank 5:
- slot 0 → 140 → channel 141 (1-based)
- slot 1 → 141 → channel 142 (1-based)
- slot 2 → 142 → channel 143 (1-based)
- slot 3 → 143 → channel 144 (1-based)
- slot 4 → 144 → out of bounds

So in 1-based numbering, channels 129-144 would be bank 4 (4*28+1=113... no).

Let me recalculate properly:
- Bank 0: slots 0-27 → channel indices 0-27 → 1-based channels 1-28
- Bank 1: slots 0-27 → channel indices 28-55 → 1-based channels 29-56
- Bank 2: slots 0-27 → channel indices 56-83 → 1-based channels 57-84
- Bank 3: slots 0-27 → channel indices 84-111 → 1-based channels 85-112
- Bank 4: slots 0-27 → channel indices 112-139 → 1-based channels 113-140
- Bank 5: slots 0-27 → channel indices 140-167 → only 140-143 valid → 1-based channels 141-144 (4 channels)

But the label shows "129 - 144" for bank 5, which doesn't match. Let me re-read the label code:
```cpp
juce::String(bank * nova::ui::kSurfaceFaders + 1)  // bank 5: 5*28+1 = 141
juce::String(juce::jmin(kLogicalChannels, (bank + 1) * nova::ui::kSurfaceFaders))  // min(144, 168) = 144
```

Oh wait, I misread. The label shows `bank * kSurfaceFaders + 1` which for bank 5 = 141, and the end is 144. So the label shows "141 - 144" which is 4 channels, matching the 4 valid faders. But earlier I saw "INPUTS  129 - 144"... Let me check again.

Actually, re-reading the code:
```cpp
bankLabel.setText("INPUTS  " +
                      juce::String(bank * nova::ui::kSurfaceFaders + 1) +
                      " - " +
                      juce::String(juce::jmin(
                          kLogicalChannels,
                          (bank + 1) * nova::ui::kSurfaceFaders)),
                  juce::dontSendNotification);
```

For bank=0: "1 - 28"
For bank=1: "29 - 56"
For bank=2: "57 - 84"
For bank=3: "85 - 112"
For bank=4: "113 - 140"
For bank=5: "141 - 144" (since jmin(144, 168) = 144)

But wait, looking at the earlier search results, the bank label was shown as "129 - 144" in some contexts. Let me check if there's different banking logic.

Hmm, actually I think the discrepancy might be from a different part of the code or from the AUX bus banking which uses `kVisibleAuxSends = 4`. Let me not get confused and focus on the main fader bank issue.

The key point: Bank 5 shows "141 - 144" in the label (4 channels), and only faders 0-3 map to valid channels 140-143 (0-based). Faders 4-27 are empty.

## Issue 4: Channel Index Boundary Risks

**Potential risk**: Code that doesn't properly check `channelIndex >= kLogicalChannels` could access `engine.channels[]` out of bounds.

**Evidence**: The `syncSurface()` function correctly checks `if (channelIndex >= kLogicalChannels)`, but other code paths might not.

**Example**: `NovaConsoleSurfaceJuce::mouseDown()` calls `model.channelForFader(f)` which returns `layers_[activeLayer_].channelMap[f]`. If activeLayer ≥ 5, this could return a channel index ≥ 144, and then the code checks `if(channel<0)return;` which would silently discard it, but the channel index would be invalid.

## Summary of Findings

| # | Issue | Severity | affected files |
|---|-------|----------|----------------|
| 1 | Model channelMap out-of-bounds for layers 5-7 | High | NovaConsoleSurfaceModel.h |
| 2 | Model layer/bank system not integrated with UI | Medium | Main.cpp, NovaConsoleSurfaceModel.h |
| 3 | Last bank (bank 5) has 24/28 empty faders | Medium | Main.cpp |
| 4 | Channel index boundary risk in mouse event handling | Low | NovaConsoleSurfaceJuce.h, NovaConsoleSurfaceModel.h |

## Recommendations

1. **Fix model initialization**: Limit channelMap values to < kSurfaceChannels (144)
2. **Integrate model with UI**: Either use the model's layer/bank system in MainComponent, or remove the inert model state
3. **Fix last bank display**: Either reduce fader count for the last bank, or wrap/channell mapping
4. **Add boundary assertions**: Assert channel indices < kLogicalChannels in debug builds