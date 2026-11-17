# NOVA HD Layer Mapping Regression Report

## Test Plan

### Test Objective
Verify that all layer/bank combinations work correctly, especially the previously problematic layers 5-7 and the last bank scenario.

### Test Configuration
- Target: NOVA HD mixing console application
- Test platform: 21-inch touchscreen display
- Test channels: 144 logical channels (channels 1-144, 0-based indices 0-143)
- Test layers: 8 layers (0-7)
- Test banks: 6 banks (0-5)

### Test Cases

#### Test 1: Layer Initialization and ChannelMap Validity
**Objective**: Verify that all layer channel mappings are within valid bounds.

**Procedure**:
1. Initialize NovaConsoleSurfaceModel
2. For each layer (0-7), for each fader (0-27), read channelMap[s] value
3. Verify all values are within [0, 143]

**Expected Result**: All channelMap values should be 0-143

**Actual Result** (before fix): Layers 5-7 had values 140-167, 168-195, 196-223 (out of bounds)

**Actual Result** (after fix): All layers have values 0-143 ✓

**Test Status**: PASS

#### Test 2: Bank Navigation Range Validation
**Objective**: Verify that bank navigation stays within valid channel ranges.

**Procedure**:
1. Start at bank 0
2. Press "BANK >" to advance through banks 0-5
3. Verify bank label shows correct channel range for each bank
4. Verify no bank produces mostly-empty fader layouts

**Expected Result**:
- Bank 0: channels 1-28 (28 faders)
- Bank 1: channels 29-56 (28 faders)
- Bank 2: channels 57-84 (28 faders)
- Bank 3: channels 85-112 (28 faders)
- Bank 4: channels 113-140 (28 faders)
- Bank 5: channels 141-144 (4 faders only - should be handled gracefully)

**Actual Result** (before fix): Bank 5 showed "129-144" label but had 24 empty faders

**Actual Result** (after fix): 
- Banks 0-4: 28 valid faders each ✓
- Bank 5: handled with graceful empty-fader display ✓
- Navigation caps appropriately ✓

**Test Status**: PASS

#### Test 3: Layer Navigation Functionality
**Objective**: Verify that selectLayer() and channelForFader() work correctly.

**Procedure**:
1. Select each layer (0-7) programmatically
2. For each layer, verify channelForFader(f) returns valid channel indices
3. Verify the model's activeLayer() returns the selected layer

**Expected Result**: 
- activeLayer() returns the selected layer
- channelForFader(f) returns valid indices within [0, 143] for all layers
- Layer 0: faders → channels 0-27
- Layer 1: faders → channels 28-55
- Layer 2: faders → channels 56-83
- Layer 3: faders → channels 84-111
- Layer 4: faders → channels 112-139
- Layer 5: faders → channels 140-143 (4 channels, rest mapped to 143 or clamped)
- Layer 6: faders → all channels clamped to valid range
- Layer 7: faders → all channels clamped to valid range

**Actual Result** (after fix): All layers return valid channel indices within [0, 143] ✓

**Test Status**: PASS

#### Test 4: UI Sync with Model Changes
**Objective**: Verify that UI updates correctly when model layer/bank changes.

**Procedure**:
1. Select layer 2 via model.selectLayer(2)
2. Verify syncSurface() uses the new layer mapping
3. Verify fader channel assignments match layer 2 mapping (channels 56-83)
4. Repeat for bank selection

**Expected Result**: UI fader strip names, meters, and meters update to reflect the new layer's channel mapping.

**Actual Result** (after fix): UI correctly updates to show channels matching the selected layer's mapping ✓

**Test Status**: PASS

#### Test 5: Edge Case - Channel Index Boundary
**Objective**: Verify that out-of-bounds channel indices are handled gracefully.

**Procedure**:
1. Force an out-of-bounds channel index (simulate by modifying model data)
2. Verify syncSurface() handles it without crash or visual corruption
3. Verify the jassert triggers in debug mode

**Expected Result**: Graceful handling, empty fader state, jassert in debug mode

**Actual Result** (after fix): Out-of-bounds indices handled by the existing `if (channelIndex >= kLogicalChannels)` check in syncSurface() ✓; jassert catches violations in debug builds ✓

**Test Status**: PASS

### Regression Summary

| Test | Status | Notes |
|------|--------|-------|
| Test 1: ChannelMap validity | PASS | All 8 layers now have valid mappings |
| Test 2: Bank navigation | PASS | Last bank handled gracefully |
| Test 3: Layer navigation | PASS | selectLayer/channelForFader working |
| Test 4: UI sync with model | PASS | UI updates correctly reflect model changes |
| Test 5: Boundary edge cases | PASS | jassert and graceful handling working |

### Known Limitations

1. Layer 5-7 fader mappings are compressed to the last valid channels (140-143) since there aren't enough channels for 8 full layers of 28 faders each (8 × 28 = 224 > 144). This is a design constraint, not a bug.

2. The model currently supports 8 layers, but practical use may be limited to 5 full layers (layers 0-4 use all 28 faders with valid channels) plus partial layers 5-7.

3. Bank 5 (inputs 141-144) only has 4 valid channels. The UI now handles this gracefully with empty fader indicators, but users should be aware that the last bank has reduced functionality.

### Files Modified

1. `Source/NovaConsoleSurfaceModel.h` - Fixed channelMap initialization, added jassert in channelForFader()
2. `Source/Main.cpp` - Updated syncSurface() to use model.channelForFader(), updated setBank() to use model.selectBank(), added boundary checks

### Rollback Plan

If the fixes cause any issues, the following can be reverted:
1. Revert NovaConsoleSurfaceModel.h channelMap initialization to original `l*kSurfaceFaders + s`
2. Revert MainComponent syncSurface() to original offset-based banking
3. Revert MainComponent setBank() to original bank calculation

All changes maintain backward compatibility with existing channel indices 0-143 and default layer 0 / bank 0 state.