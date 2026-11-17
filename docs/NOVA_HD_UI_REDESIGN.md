# NOVA HD Complete UI Redesign Specification

## PROJECT OVERVIEW

**Project**: NOVA HD Professional Digital Mixing Console  
**Redesign Goal**: Premium professional live performance mixing console interface for 21-inch full-colour touchscreen  
**Design Inspiration**: High-end touring consoles (MIDAS HD96 influence) with original NOVA HD branding  
**Target Display**: 21-inch TFT touchscreen, multi-touch supported  
**Theme**: Professional dark interface, high contrast, touch-friendly controls  

---

## VISUAL DESIGN SYSTEM

### Colour Palette
All colours derived from existing `NovaLookAndFeel.h` with expanded Nova HD branding:

| Role | Colour | Hex | Usage |
|------|--------|-----|-------|
| **Background** | Novo Dark | `#202123` | Main window, panels, screens background |
| **Panel** | Panel Dark | `#292a2c` | Channel strips, sub-panels, areas of distinction |
| **Control** | Control Mid | `#343638` | Sliders, knobs, interactive elements |
| **Border** | Border | `#424446` | Element borders, separators |
| **Accent** | Nova Blue | `#49a5c8` | Highlights, active elements, focus state |
| **Text** | Body Text | `#d5d7d8` | General UI text, labels |
| **Meter Hot** | Meter Fault | `#ef5350` | Peak indicators > -3dB, error states |
| **Meter OK** | Meter Warm | `#ffb74d` | Peak indicators > -12dB, moderate levels |
| **Meter Good** | Meter OK | `#34c77b` | Peak indicators ≤ -12dB, healthy levels |
| **Selection** | Active Selection | `#36c5c0` | Selected channel, focus state |
| **Silent** | Muted/Solo | `#b8323c` | Muted channels, solo active |
| **Dugan** | Dugan Purple | `#a0c4ff` | Automixer active channels |
| **VCA** | VCA Group | `#ff6d41` | VCA group assignment |

### Typography
- **Font Family**: `Roboto` (variable weight) or `Nunito` for rounded aesthetic
- **Sizes**:
  - Display/Section Headers: 24px bold
  - Panel Headers: 18px semi-bold
  - Control Labels: 14px regular
  - Meter Values: 12px medium
  - Small Captions: 11px regular
- **Line Height**: 1.4 for body, 1.2 for labels
- **Letter Spacing**: 0.02em default, 0.04em for headers

### Spark System (Spacing)
- **Touch Gap**: 8px minimum between interactive elements
- **Component Padding**: 12px internal padding on all panels
- **Grid Column**: 12-column grid for complex layouts
- **Base Unit**: 8px vertical rhythm
- **Screen Margins**: 16px from display edges, 8px between columns

### Touch Target Requirements
- **Minimum Size**: 44×44 px (pressed state), 48×48 px recommended
- **Spacing**: Minimum 8px between touch targets
- **Edge Targets**: Full-width bottom action bars, 48px high
- **Multi-touch**: Support 2+ simultaneous touches without ghosting

### Visual Hierarchy
1. **Primary action** (faders, knobs in touch) — highest contrast, largest targets
2. **Secondary controls** (buttons, switches) — medium contrast, moderate targets
3. **Tertiary information** (meters, labels) — lower contrast, smaller text
4. **Status/feedback** (meters, indicators) — accent colour for attention

---

## SCREEN 1 — MAIN MIXER

### Overview
The primary mixing screen occupying the main display area. Shows 28 channel fader strips across the width of the 21-inch display, with bank navigation to access all 144 channels.

### Layout
- **Top Row**: Channel strip labels (names, colours)
- **Middle Section**: 28 fader strips arranged vertically (or horizontally in landscape)
- **Bottom Row**: Main L/R master section + bank navigation

### Channel Strip (per fader)
Each of the 28 visible channels displays:

| Control | Position | Description |
|---------|----------|-------------|
| **Channel Name** | Top-left | Full name, colour-coded by instrument type |
| **Input Meter** | Top-right, small bar | Input signal peak detection |
| **Peak Indicator** | Above fader | Red/amber/green peak hold indicator |
| **Fader** | Center-left | Vertical slider, motorized emulation |
| **Fader Value** | Bottom-left | Numerical dB display (e.g., "-12.0 dB") |
| **Pan Knob** | Top-right | Dual-circle concentric pan control |
| **Mute Button** | Bottom-left, large | Red when active, toggles mute |
| **Solo Button** | Bottom-right, large | Yellow when active, toggles solo |
| **AUX Send Knob** | Right side, horizontal row | 4 visible sends (AUX 1-4), remainder accessible via bank |
| **Matrix Send Knob** | Far right, horizontal row | 2 visible matrix sends |

### Master Section (Bottom Row)
| Control | Description |
|---------|-------------|
| **L/R Fader** | Main master fader, dual-channel (L/R combined or separate) |
| **Master Meter** | Large bar graph showing main L/R output |
| **Master Peak** | Peak hold indicator for main output |
| **Phantom Power** | Global toggle (48V) for channels 1-8 |
| **High-Pass Global** | Global HPF frequency selector |
| **Bank Navigation** | Left/right arrows to scroll through 144 channels (8 layers × 6 banks, 28 visible per page) |
| **Layer Selector** | Display showing current layer (1-8) and bank (1-6) |

### Navigation
- **Bank Left/Right**: Scroll by 28 channels (one page)
- **Layer Up/Down**: Scroll by 36 channels (144/8), shifts channel map
- **Touch Drag**: Vertical drag on fader to adjust level
- **Pinch/Expand**: Zoom bank navigation (optional)
- **Long-press**: Channel properties menu access

### Interaction Patterns
- **Fader Touch**: Vertical drag adjusts level, shows tool-tip with dB value
- **Mute/Solo Tap**: Toggles state, visual feedback with colour change
- **Pan Knob**: Drag within circle to set pan position (-1 left to +1 right)
- **AUX/Matrix Knobs**: Drag horizontally to set send level (-∞ to +4dB)
- **Bank Wrap**: At first/last bank, disabled or wrap-around

### Visual Design Details
- **Channel Colours**: Assigned from `kMidasColors` palette based on instrument icon
- **Selected Channel**: Highlighted with accent colour `#36c5c0` border
- **Muted Channel**: Dimmed with `#b8323c` overlay
- **Soloed Channel**: Highlighted with `#ffb74d` with "SOLO" badge
- **Fader Background**: `#343638` with thumb `#49a5c8`
- **Meter Bars**: Bar graph with colour transition green→amber→red at -6dB/-3dB

---

## SCREEN 2 — CHANNEL PROCESSING

### Overview
Deep editing screen for individual channel processing. Accessible via fader strip "Edit" button or touch gesture on channel name.

### Layout (Full-width on 21-inch display)
- **Header**: Channel name, number, close/X button
- **Left Column**: Input section (gain, phantom, HPF)
- **Right Channel**: Dynamics processing (gate, compressor)
- **Bottom Section**: EQ and output controls
- **Footer**: Save/Close buttons

### Input Section
| Control | Parameter | Range | Default |
|---------|-----------|-------|---------|
| **Input Gain** | Preamp trim | -∞ to +60 dB | 0 dB |
| **Phantom Power** | 48V toggle | On/Off | Off |
| **Polarity Invert** | Phase flip | On/Off | Off |
| **High-Pass Filter** | Frequency | 20 Hz to 20 kHz | 80 Hz |
| **HPF Slope** | Roll-off | 12 dB/oct or 24 dB/oct | 12 dB/oct |

### Dynamics Section (Gate & Compressor)
**Gate Section**:
| Control | Parameter | Range | Default |
|---------|-----------|-------|---------|
| **Gate Threshold** | Detection level | -60 to 0 dB | -45 dB |
| **Gate Attack** | Release time | 0.1 ms to 100 ms | 2.0 ms |
| **Gate Release** | Recovery time | 10 ms to 500 ms | 120 ms |
| **Gate Ratio** | Expansion ratio | 1:1 to 10:1 | 1:1 (open) |
| **Gate Key Input** | Sidechain source | Internal/External | Internal |

**Compressor Section**:
| Control | Parameter | Range | Default |
|---------|-----------|-------|---------|
| **Compressor Threshold** | Compression start | -∞ to +20 dB | -18 dB |
| **Compressor Ratio** | Compression ratio | 1:1 to 20:1 | 4:1 |
| **Compressor Attack** | Attack time | 0.1 ms to 100 ms | 10 ms |
| **Compressor Release** | Release time | 10 ms to 500 ms | 120 ms |
| **Compressor Knee** | Knee width | 0 to 24 dB | 3 dB |
| **Compressor Mode** | Operation mode | Above/Below/Link | Corrective |

### EQ Section (4-Band Parametric)
| Band | Frequency | Gain | Q | Type |
|------|-----------|------|---|------|
| **Band 1** | 100 Hz | 0 dB | 1.0 | Peaking |
| **Band 2** | 500 Hz | 0 dB | 1.0 | Peaking |
| **Band 3** | 2 kHz | 0 dB | 1.0 | Peaking |
| **Band 4** | 8 kHz | 0 dB | 1.0 | Peaking |

| Control | Parameter | Range | Display |
|---------|-----------|-------|---------|
| **Frequency Knob** | Each band freq | 20 Hz to 20 kHz | Large interactive dial |
| **Gain Knob** | Each band gain | -∞ to +∞ dB | Large interactive dial |
| **Q Knob** | Each band Q | 0.1 to 10.0 | Large interactive dial |
| **High-Pass/Low-Pass** | Global HPF/LPF | 20 Hz to 20 kHz | Toggle + frequency display |

### Dynamics Link Section
| Control | Description |
|---------|-------------|
| **Post-EQ Dynamics** | Toggle: dynamics before or after EQ |
| **Link Channels** | Link gain reduction across selected channels |
| **Dugan Automixer** | Toggle + group assignment + weight control |
| **De-Esser** | Frequency control (2-10 kHz), reduction amount |

### Output Section
| Control | Parameter | Range |
|---------|-----------|-------|
| **Input Delay** | Delay time | 0.0 ms to 500.0 ms |
| **Stereo Link** | Link with stereo pair | On/Off |
| **Panning** | Pan position | Hard left (-1) to hard right (+1) |
| **High-Pass Output** | Post-fader HPF | On/Off, frequency |
| **Low-Pass Output** | Post-fader LPF | On/Off, frequency |

### Interaction Patterns
- **Knob Drag**: Rotary drag adjusts parameter, value displayed numerically
- **Slider Touch**: Horizontal drag adjusts gain/threshold
- **Toggle Switch**: Tap to on/off, visual state change
- **Value Field**: Tap to edit numeric value with +/- steps
- **Preserve**: Changes maintain backward compatibility with existing parameter interfaces

### Visual Design
- **Section Dividers**: Subtle lines with accent colour `#49a5c8`
- **Active Section**: Current section highlighted with accent border
- **Parameter Rings**: Interactive dials with centre value display
- **Meter Integration**: Small input/output meters within the screen

---

## SCREEN 3 — PARAMETRIC EQ

### Overview
Full-screen parametric EQ editor with interactive frequency response graph. For detailed EQ editing of any selected channel.

### Layout
- **Top Header**: Channel name, close button, "Edit EQ" label
- **Main Area**: Interactive frequency response graph (60% of screen height)
- **Bottom Controls**: EQ band controls (4 bands + HPF/LPF)
- **Footer**: Presets menu, save/cancel

### Frequency Response Graph
- **Width**: Full display width (21-inch screen, ~1680px)
- **Height**: ~400px (scaled proportionally)
- **X-Axis**: Frequency range 20 Hz to 20 kHz, logarithmic scale
- **Y-Axis**: Gain range -∞ to +12 dB (or -∞ to +24 dB)
- **Grid**: Subtle background grid at 1/3 octave intervals

### Graph Interaction
| Interaction | Effect |
|-------------|--------|
| **Drag Curve** | Adjust selected band's gain at touched frequency |
| **Tap Frequency** | Sets selected band's frequency at tapped point |
| **Pinch Zoom** | Zoom X-axis frequency range |
| **Two-Finger Drag** | Scroll frequency range left/right |
| **Band Selector** | Tap band number (1-4) to select for editing |

### Graph Visualization
- **Current Response**: Solid line showing current EQ curve
- **Flat Reference**: Dashed line at 0 dB (unity gain)
- **Last Response**: Faded line showing previous EQ setting
- **Frequency Markers**: Vertical ticks at 20 Hz, 50 Hz, 100 Hz, 200 Hz, 500 Hz, 1 kHz, 2 kHz, 5 kHz, 10 kHz, 20 kHz
- **Gain Shading**: Area under curve filled with semi-transparent accent colour

### EQ Band Controls (Bottom Section)
Four parametric bands + High-Pass + Low-Pass:

| Parameter | Control Type | Description |
|-----------|-------------|-------------|
| **Frequency** | Rotary dial + value field | 20 Hz to 20 kHz, large click increments |
| **Gain** | Rotary dial + value field | -∞ to +∞ dB, +∞ represented as "∞" |
| **Q** | Rotary dial + value field | 0.1 to 10.0, logarithmic scaling |
| **Type** | Button group | Peaking (default), Low-Shelf, High-Shelf, Low-Pass, High-Pass |

### HPF/LPF Controls
| Control | Parameter | Range |
|---------|-----------|-------|
| **High-Pass Filter** | Frequency | 20 Hz to 20 kHz, 12 or 24 dB/oct |
| **Low-Pass Filter** | Frequency | 20 Hz to 20 kHz, 12 or 24 dB/oct |

### Real-Time Spectrum Display
- **Below Graph**: Small spectrum analyzer showing input signal frequency content
- **Update Rate**: 60 fps (vs audio callback 48/96/192 kHz)
- **Range**: 20 Hz to 20 kHz, logarithmic
- **Peak Hold**: Option to enable peak hold display
- **Link to Graph**: Tap frequency in spectrum to set EQ band frequency

### Preset Management
| Control | Description |
|---------|-------------|
| **Load Preset** | Browse and load saved EQ curves |
| **Save Preset** | Save current EQ curve as named preset |
| **Factory Presets** | EQ curves for common sources (vocals, drums, bass, piano, etc.) |
| **Name Field** | Text input for preset naming |

### Interaction Patterns
- **Graph Drag**: Adjust gain at specific frequency
- **Frequency Tap**: Set band frequency by tapping graph
- **Knob Rotation**: Drag rotary controls to adjust parameters
- **Preset Select**: Tap to load/save EQ presets
- **A/B Comparison**: Toggle between current and saved preset

### Visual Design
- **Graph Area**: Dark background `#111820` with grid lines `#424446`
- **Current Curve**: `#36c5c0` (Nova accent) solid line
- **Reference Line**: `#d5d7d8` dashed line at 0 dB
- **Frequency Ticks**: `#888` colour, every 1/3 octave
- **Band Selectors**: Active band highlighted with `#49a5c8` ring
- **Preset Buttons**: Background `#343638`, active `#49a5c8`

---

## SCREEN 4 — ROUTING MATRIX

### Overview
Complex routing matrix screen showing 144 input channels routing to 123 mix buses, AUX sends, matrix routing, VCA groups, and population groups.

### Layout
- **Top Header**: "ROUTING MATRIX" + channel count summary
- **Main Area**: Three-zone layout
  - **Left Zone**: 144 input channels (collapsible)
  - **Center Zone**: Routing matrix (123 mix buses × 144 inputs)
  - **Right Zone**: AUX sends + Matrix sends + VCA groups
- **Bottom**: Navigation footer, VCA assignments

### Left Zone — Input Channels (Collapsible)
- **Height**: Collapsible to 1/4 screen width
- **Content**: 144 channel strips, each showing:
  - Channel name/number
  - Colour coding
  - Current routing assignments (checkboxes/indicators)
  - VCA group membership
  - Population group membership

### Center Zone — Mix Bus Matrix
- **Configuration**: 123 mix buses × 144 input channels
- **Visualization**: Grid where:
  - **Rows**: Mix buses (1-123)
  - **Columns**: Input channels (1-144)
  - **Cell Content**: Send level indicator (0 to +10 dB, or -∞)
  - **Interaction**: Tap/click to set send level, drag to adjust
  - **Highlight**: Active sends highlighted with accent colour

### Routing Cell Interaction
| Interaction | Effect |
|-------------|--------|
| **Tap Cell** | Toggle send on/off at default level |
| **Drag Horizontal** | Adjust send level from -∞ to +10 dB |
| **Drag Vertical** | Move to different bus row |
| **Long-Press** | Copy routing to clipboard, or open detailed edit |
| **Right-Click** | Open context menu: "Route to Bus", "Remove Routing", "Select All Similar" |

### Right Zone — AUX and Matrix Sends + VCA Groups
**AUX Sends Section** (top of right zone):
- **Configuration**: 96 AUX buses × 144 channels
- **Display**: First 8 AUX buses visible, scrollable
- **Per-AUX Controls**: 
  - Send level per channel (or global fader)
  - Pre/post fader toggle
  - Solo after fader toggle

**Matrix Sends Section** (middle of right zone):
- **Configuration**: 24 matrix buses × 144 channels
- **Display**: First 4 matrix buses visible, scrollable
- **Per-Matrix Controls**:
  - Send level per channel
  - Panning control per matrix bus (L/R/Sides/Top/Bottom)
  - Phase inversion toggle

**VCA Groups Section** (bottom of right zone):
- **Configuration**: 24 VCA groups
- **Each VCA Group Shows**:
  - Group name/number
  - Fader control (linked to all channels in group)
  - Member channels list (checkboxes)
  - Solo/mute affects on group
  - Pop group assignment

### Population Groups
- **Display**: "POP 1-24" indicators on channel strips
- **Function**: Groups of channels that move together (drum kit, backing vocals, etc.)
- **Interaction**: Tap POP group label to select/all member channels
- **Visual**: Badge on channel strip showing POP group number

### Master Section (Bottom of Screen)
| Control | Description |
|---------|-------------|
| **L/R Assign** | Main L/R bus selection from available mix buses |
| **Master Fader** | Overall output level control |
| **Master Meter** | Main L/R output meter with peak hold |
| **VCA Master** | Linked VCA group master fader |
| **Navigation** | Scroll through 123 mix buses (banks of 28) |

### Navigation
- **Bus Banks**: Scroll through 123 buses in chunks of ~28 (mobile) or full view (desktop)
- **Channel Filter**: Search/filter by channel name, colour, VCA group
- **Save Routing**: Save current routing configuration as recallable setup
- **Recall**: Load saved routing configuration

### Visual Design
- **Matrix Grid**: Dark background `#111820` with subtle grid
- **Active Cell**: `#36c5c0` accent when selected/edited
- **Send Level**: Colour gradient from green (-∞) through amber to red (+10 dB)
- **VCA Members**: Highlighted with `#ff6d41` when VCA group fader is adjusted
- **Population Badges**: `#a0c4ff` (Dugan purple) on channel strips

### Interaction Patterns
- **Tap-Bus**: Assign channel to mix bus
- **Drag-Level**: Set send level by dragging in cell
- **Multi-Select**: Ctrl+tap to select multiple channels/buses
- **Copy/Paste**: Copy routing from one channel to another
- **Search**: Filter channels by name, VCA, POP group

---

## SCREEN 5 — EFFECTS

### Overview
Multi-effects rack screen for managing sends/returns to external effects processors, and internal effect plugins.

### Layout
- **Top Header**: "EFFECTS RACK" + current preset name
- **Main Area**: Three-column layout
  - **Left**: Effect type selector and parameters
  - **Center**: Active effects rack chain visualization
  - **Right**: Send/return level controls
- **Footer**: Preset browser, save/cancel

### Effect Types
| Effect | Parameters | DSP Source |
|--------|-----------|-----------|
| **Reverb** | Room size, damping, wet/dry, early reflections, density, width, freeze | GravitonConvolutionReverb.h / GravitonHybridReverb.h |
| **Delay** | Time (0.1 ms to 500 ms), feedback (0% to 90%), wet/dry, low-pass filter | GravitonDVR2Reverb.h delay line |
| **Chorus** | Rate (0.01 to 20 Hz), depth (0% to 100%), wet/dry, delay (0.1 ms to 200 ms), feedback | GravitonSCFChorus.h |
| **Flanger** | Rate (0.01 to 20 Hz), depth (0% to 100%), wet/dry, delay (0.1 ms to 200 ms), feedback | Internal DSP |
| **Multi-Effects** | Combination of above, adjustable order | GravitonEffectsRack.h |

### Left Column — Effect Type Selector
| Control | Description |
|---------|-------------|
| **Effect Browser** | Scrollable list of effect types (Reverb, Delay, Chorus, Flanger, Distortion, Filter, Pitch) |
| **Preset Selector** | Factory/user presets for each effect type |
| **Effect On/Off** | Toggle switch for each effect in chain |
| **Chain Order** | Drag-and-drop to reorder effects in signal chain |

### Center Channel — Effects Rack Chain Visualization
- **Display**: Horizontal chain of active effects
- **Each Effect Shows**:
  - Effect type icon
  - Parameter summary (e.g., "Reverb: Large Hall")
  - Bypass toggle (left-click)
  - Edit parameter (right-click or double-tap)
  - Remove from chain (drag out or trash icon)
- **Signal Flow**: Left-to-right visual signal path
- **Total DSP Usage**: Percentage of available DSP resources

### Right Column — Send/Return Levels
**Effect Send Section**:
| Control | Description |
|---------|-------------|
| **Effect Send Knob** | Per-channel send level to effects rack (-∞ to +6 dB) |
| **Pre/Post-Fader** | Toggle button |
| **Post/Pre-EQ** | Toggle button |
| **Stereo Link** | Link stereo channels |

**Return Section**:
| Control | Description |
|---------|-------------|
| **Effect Return Fader** | Master return level from effects rack (-∞ to +6 dB) |
| **Return Balance** | Left/Right balance for stereo returns |
| **Direct Out** | Bypass effects, send directly to mixer output |
| **Dry/Wet Mix** | Global dry/wet mix for all effects |

### Effect Parameters (detailed editing)
When an effect is selected in the chain, its parameters appear in a detailed view:

**Reverb Parameters**:
- Room Size: 0% to 100% (visual: small room to large hall)
- Damping: 0% to 100% (high/mid/low frequency absorption)
- Wet/Dry Mix: -∞ to 0 dB
- Early Reflections: 0% to 100% (strength)
- Density: 0.1 to 10.0 (note: perceptual density)
- Width: 0% (mono) to 200% (stereo expand)
- Freeze: On/Off (hold current reverb tail)

**Delay Parameters**:
- Time: 0.1 ms to 500 ms (or sync to tempo: 1/1, 1/2, 1/4, 1/8, 1/16)
- Feedback: -90% to +90% (negative = inverted)
- Wet/Dry: -∞ to 0 dB
- Low-Pass Filter: 20 Hz to 20 kHz
- Modulation: Rate, depth, sync to tempo

**Chorus Parameters**:
- Rate: 0.01 Hz to 20 Hz, or sync to tempo
- Depth: 0% to 100%
- Wet/Dry: -∞ to 0 dB
- Delay Time: 0.1 ms to 200 ms, or sync to tempo
- Feedback: -90% to +90%
- Sync To Tempo: On/Off, BPM display

**Flanger Parameters**:
- Rate: 0.01 Hz to 20 Hz, or sync to tempo
- Depth: 0% to 100%
- Wet/Dry: -∞ to 0 dB
- Delay Time: 0.1 ms to 200 ms, or sync to tempo
- Feedback: -90% to +90%
- Stereo: On/Off, width control

### Preset Management
| Control | Description |
|---------|-------------|
| **Load Preset** | Browse and load effect chain presets |
| **Save Preset** | Save current effect chain as named preset |
| **Factory Presets** | Common effect chains ( vocal chain, drum bus, master bus, etc.) |
| **Name Field** | Text input for preset naming |
| **Preset Browser** | Scrollable list with preview |

### DSP Usage Display
- **Top of Screen**: Total effect DSP usage percentage
- **Per-Effect**: Each effect shows its DSP cost
- **Warning**: Yellow warning when > 70% DSP usage
- **Critical**: Red warning when > 90% DSP usage (some effects disabled)

### Interaction Patterns
- **Drag Effect**: Reorder effects in chain by dragging
- **Tap Bypass**: Toggle effect on/off
- **Knob/Dial Drag**: Adjust effect parameters
- **Preset Save/Load**: Browse/save effect chains
- **Copy Chain**: Duplicate effect chain to another channel

### Visual Design
- **Chain Background**: `#202123` with subtle separator lines `#424446`
- **Active Effect**: Highlighted with `#49a5c8` border
- **Bypass Switch**: `#34c77b` when on, `#6e7681` when off
- **DSP Warning**: Yellow `#ffb74d` at 70%, red `#ef5350` at 90%
- **Preset Buttons**: `#343638` inactive, `#49a5c8` active

---

## SCREEN 6 — METERS

### Overview
Comprehensive metering screen showing all input, output, bus, and analysis meters for real-time monitoring during live performance.

### Layout
- **Top Header**: "METERS" + sample rate, buffer size display
- **Main Area**: Three-zone layout
  - **Left Zone**: Input meters (channels 1-144, or visible subset)
  - **Center Zone**: Bus meters (AUX, Matrix, Master)
  - **Right Zone**: Analysis tools (RTA, Spectrogram, Correlation)
- **Footer**: Meter configuration, save layout

### Left Zone — Input Meters
**Channel Strip Meter** (one of 144 visible at a time, scrollable):
| Element | Description |
|---------|-------------|
| **Channel Name/Number** | Left side of strip |
| **Peak Bar** | Vertical bar showing current peak |
| **Average Level** | Smoothed bar (200ms integration) |
| **Peak Hold** | Holds peak until reset |
| **Colour Coding**: Green (≤-12 dB), Amber (-12 to -6 dB), Red (>-6 dB) |

**Meter Colours** (from design system):
- **Green**: `#34c77b` (peak ≤ -12 dB)
- **Amber**: `#ffb74d` (peak > -12 dB to ≤ -6 dB)
- **Red**: `#ef5350` (peak > -6 dB)

**Meter Layout Options**:
| Layout | Channels Visible | Layout Style |
|--------|-----------------|--------------|
| **Compact** | 24 channels | Small vertical bars, 2 per row |
| **Medium** | 48 channels | Medium bars, 4 per row |
| **Expanded** | 144 channels | Tiny bars, 8 per row, scrollable |
| **Full Screen** | 144 channels | One column, large bars |

### Center Zone — Bus Meters
**Mix Bus Meters** (1-123):
- Each bus shows: Peak bar, average level, over indicator
- Colour coding same as input meters
- L/R channel separation for stereo buses

**AUX Bus Meters** (1-96):
- Same visual style as mix buses
- Highlight when soloed or patched

**Matrix Bus Meters** (1-24):
- Same visual style
- Show main L/R assignment

**Master Meter** (full width):
- Large dual-bar showing L and R channels
- Peak hold indicator
- Overload indicator (red when > 0 dB)

### Right Zone — Analysis Tools

**RTA (Real-Time Analyzer)**:
- **Frequency Range**: 20 Hz to 20 kHz, 1/3 octave bands
- **Update Rate**: 30 fps (conservative for readability)
- **Display Mode**: 
  - **Average**: 100ms smoothing
  - **Peak**: Hold peak values
  - **River**: Waterfall display (time vs frequency)
- **Interaction**:
  - Tap frequency band → isolate/ solo
  - Pinch zoom → adjust frequency range
  - Save snapshot → capture current RTA state

**Spectrogram**:
- **Display**: Time (horizontal) vs Frequency (vertical) vs Level (color)
- **Resolution**: 1024 FFT size, 20ms window
- **Colour Scale**: Green (cold) to Red (hot)
- **Duration**: Last 10 seconds (scrollable)
- **Interaction**:
  - Tap → zoom frequency region
  - Pinch → adjust time/frequency resolution
  - Save screenshot → export current view

**Stereo Correlation Meter**:
- **Display**: Vector scope-style meter
- **Range**: -1.0 (inverted polarity) to +1.0 (coherent)
- **Ball Meter**: Large ball centred at (0,0)
- **Under/Over**: Bars indicating out-of-range conditions
- **Phase Alert**: When correlation < 0.5, show warning

### Meter Configuration
| Control | Description |
|---------|-------------|
| **Meter Layout** | Select input/output/bus/analysis layout |
| **Channel Range** | Select which channel subset visible (1-144) |
| **Peak Hold Time** | Duration peak values held (off, 0.1s, 0.5s, 1s, 2s) |
| **Smoothing** | Meter averaging time (20ms, 100ms, 400ms) |
| **Colour Theme** | Custom meter colour palette (save/user) |
| **DSP Usage** | Show meter update impact on overall DSP |

### Interaction Patterns
- **Tap Channel**: Isolate channel for detailed view
- **Drag Up/Down**: Scroll through channel list
- **Pinch Zoom**: Adjust meter density (more/fewer channels)
- **Long-Press**: Open channel meter calibration
- **Save Layout**: Save current meter configuration as recallable layout

### Visual Design
- **Meter Background**: `#111820` with `#292a2c` panel lines
- **Active Meter**: `#36c5c0` accent on selected channel
- **Peak Hold**: Faded `#ef5350` behind current peak
- **RTA Grid**: `#424446` subtle lines at 1/3 octave
- **Spectrogram**: Colour ramp from `#34c77b` (low) to `#ef5350` (high)
- **Correlation Vector**: `#49a5c8` arrow indicating stereo width/phase

---

## SCREEN 7 — SHOW MANAGEMENT

### Overview
Show management screen for recalling shows, scenes, user preferences, and system configuration.

### Layout
- **Top Tab Bar**: Three tabs (Show, Scenes, Settings)
- **Main Area**: Tab content
- **Footer**: Action buttons (Import, Export, Save, Cancel)

### Tab 1 — SHOW
**Show Files Management**:
| Control | Description |
|---------|-------------|
| **Show List** | Browse saved show files (.nova show) |
| **New Show** | Create new show configuration |
| **Open Show** | Load show from file or setlist |
| **Show Info** | Name, description, version, date |
| **Setlist Manager** | Order of songs/performances in show |

**Show Recall**:
- **Recall Scene**: Select scene to recall from current show
- **Selective Recall**: Choose which parameters to recall (gains, dynamics, routing, etc.)
- **Auto-Recall**: Load show on startup

**Show Settings**:
- **Show Name**: Text field
- **Tempo**: BPM for tempo-synced effects
- **Sample Rate**: Project sample rate
- **Buffer Size**: Audio buffer size
- **Author**: Show creator name

### Tab 2 — SCENES
**Scene Recall Browser**:
| Control | Description |
|---------|-------------|
| **Scene List** | Thumbnails or list of all saved scenes |
| **Scene Name** | Name/number identifier |
| **Scene Date/Time** | When scene was saved |
| **Scene Contents** | Checkbox list of recalled parameters |
| **Recall Scene** | One-tap scene recall |
| **Compare Scene** | A/B compare current vs saved scene |

**Scene Parameters** (what can be recalled):
- Channel fader positions
- Mute/solo states
- EQ settings
- Dynamics parameters
- Routing changes
- Effect settings
- VCA group levels
- Master fader position

**Scene Locker**:
- **Lock Parameter**: Prevent specific parameters from being overwritten during recall
- **Lock All**: Lock all parameters, only recall selected
- **Unlock All**: Unlock all parameters

### Tab 3 — SETTINGS
**User Preferences**:
| Category | Controls |
|----------|----------|
| **Appearance** | Theme (dark/light), colour accents, font size |
| **Metering** | Peak hold time, smoothing, meter layout |
| **MIDI Learn** | MIDI controller assignments, learn mode |
| **Touch Settings** | Touch calibration, touch sensitivity, multi-touch mode |
| **Keyboard Shortcuts** (if external keyboard) | Custom key mappings |

**System Configuration**:
| Control | Description |
|---------|-------------|
| **Sample Rate** | 44.1 kHz, 48 kHz, 88.2 kHz, 96 kHz, 192 kHz |
| **Buffer Size** | 32, 64, 128, 256, 512, 1024 samples |
| **Channel Config** | Input/Output channel mapping |
| **Device Selection** | ASIO, WDM, CoreAudio, Windows Audio Driver |
| **Network Settings** | OSC, MIDI over Network, Dante integration |
| **Network Configuration**: OSC settings, MIDI over Ethernet, Dante integration

**About**:
- Application version
- Copyright information
- Third-party licenses (JUCE, DSP libraries, etc.)
- Update checker

### Interaction Patterns
- **Tab Selection**: Tap tab bar to switch between Show/Scenes/Settings
- **Scroll**: List views scroll vertically, pinch to scroll horizontally
- **Tap**: Select item, open detail view
- **Swipe**: Delete/remove show/scenes
- **Two-Finger**: Pinch to zoom (RTA/spectrogram), drag to scroll
- **Text Input**: On-screen keyboard for show names, preset names

### Visual Design
- **Tab Bar**: `#292a2c` with accent `#49a5c8` on active tab
- **List Background**: `#111820` with `#292a2c` alternating row stripes
- **Active Item**: `#36c5c0` highlight
- **Save/Import Buttons**: `#49a5c8` background, `#d5d7d8` text
- **Warning/Delete**: `#ef5350` for destructive actions
- **Save Indicator**: Small dot `#34c77b` when changes unsaved

---

## DSP PARAMETER MAPPING (UI → Engine)

### Existing Parameter Interfaces Preserved
The UI redesign maps to existing DSP parameter interfaces without duplication:

| UI Control | DSP Destination | Source File |
|------------|----------------|-------------|
| **Input Gain** | `ChannelState::inputGainDb` | Main.cpp line 61 |
| **High-Pass Filter** | `ChannelState::highPassEnabled` + `highPassFreqHz` | Main.cpp lines 72-73 |
| **Polarity Invert** | `ChannelState::polarityInverted` | Main.cpp line 114 |
| **Parametric EQ Bands** | `ChannelState::eqFrequency/eqGainDb/eqQ` | Main.cpp lines 91-99 |
| **Gate Threshold** | `ChannelState::gateThresholdDb` | Main.cpp line 81 |
| **Compressor Threshold/Ratio** | `ChannelState::compressorThresholdDb/ratio` | Main.cpp lines 85-86 |
| **Dugan Automixer** | `duganGroup` + `duganWeightDb` + automixer DSP | GravitonDuganAutomixer.h |
| **AUX Send Levels** | `auxSendDb[]` array | Main.cpp line 111 |
| **Matrix Send Levels** | Routing matrix in GravitonMatrixRouting.h | GravitonMatrixRouting.h |
| **Dynamics Parameters** | `dynEqEnabled/ThresholdDb/RangeDb/RatioDb/KneeDb/AttackMs/ReleaseMs/GainDb` | Main.cpp lines 132-158 |
| **Surround Panning** | `surroundX/surroundY/surroundZ` | Main.cpp lines 172-181 |
| **Colour Coding** | `colorRgb` | Main.cpp line 185 |
| **Instrument Icon** | `instrumentIcon` | Main.cpp line 186 |
| **POP Group** | `popGroup` | Main.cpp line 190 |

### New UI Controls → Existing Parameters
| UI Control | Existing DSP Parameter | Notes |
|------------|-----------------------|-------|
| **Phantom Power Toggle** | Already in `ChannelState::polarityInverted` context | Re-purpose polarity UI for phantom power |
| **De-Esser Frequency** | Part of dynamics processing | Map to dynamics threshold/frequency |
| **Limiter Threshold** | Part of master bus processing | Map to master limiter parameters |
| **Effect Send Levels** | `auxSendDb[]` + matrix routing | Already existing, UI re-skin only |
| **RTA Parameters** | Already in audio pipeline | UI wrapper only, no DSP changes |

### Backward Compatibility Notes
1. All existing parameter interfaces unchanged — UI is skin/rewrap only
2. Channel state struct (`ChannelState`) layout preserved exactly
3. All `std::atomic` members retain same types and default values
4. DSP processing order unchanged: input → HPF → PEQ → gate → compressor → fader → AUX/matrix → master
5. Dugan automixer parameters (`duganGroup`, `duganWeightDb`) unchanged
6. Time constants preserved as defaults (5ms/250ms/15ms from earlier fix)
7. CMake `NOVA_MAX_DELAY_MS` configurable delay buffer maintained

### Parameter Touch Policy
- **Real-time UI**: All parameter changes update immediately (no latency)
- **Thread Safety**: All ChannelState members are `std::atomic` for lock-free access
- **Automation**: Parameter changes support MIDI automation and controller touch
- **Undo/Redo**: UI changes support undo history for show recall
- **Safe Updates**: Meter and parameter updates use lock-free patterns

---

## NAVIGATION FLOW

### Primary Navigation Hierarchy
```
Screen 1 (Main Mixer)
  └─ Tap channel → Screen 2 (Channel Processing)
  └─ Tap Master → Master section adjustments
  └─ Tap Bank arrows → Scroll channels
  └─ Tap "Edit" → Screen 2
  └─ Tap "Routings" → Screen 4
  └─ Tap "Effects" → Screen 5
  └─ Tap "Meters" → Screen 6
  └─ Tap "Show" → Screen 7

Screen 2 (Channel Processing)
  └─ Tap "EQ" → Screen 3 (Parametric EQ)
  └─ Tap "Dynamics" → parameters within Screen 2
  └─ Tap "Save" → return to Screen 1
  └─ Tap "Close" → return to Screen 1

Screen 3 (Parametric EQ)
  └─ Tap "Done" → return to Screen 2
  └─ Tap "Load Preset" → browse presets
  └─ Tap "Save Preset" → name and save

Screen 4 (Routing Matrix)
  └─ Tap channel → highlight routing
  └─ Tap bus → assign/remove routing
  └─ Tap "VCA" → toggle VCA groups
  └─ Tap "POP" → toggle population groups
  └─ Tap "Done" → return to Screen 1

Screen 5 (Effects)
  └─ Tap effect → edit parameters
  └─ Tap "Bypass" → toggle effect on/off
  └─ Tap "Done" → return to Screen 1
  └─ Tap "Save Preset" → save effect chain

Screen 6 (Meters)
  └─ Tap meter → isolate channel
  └─ Tap "Layout" → change meter configuration
  └─ Tap "RTA" → open full analysis
  └─ Tap "Close" → return to main view

Screen 7 (Show Management)
  └─ Tap tab → switch Show/Scenes/Settings
  └─ Tap "Recall" → load scene/show
  └─ Tap "Save" → save current state
  └─ Tap "Close" → exit without saving
```

### Screen Transition Effects
- **Cross-fade**: 200ms when navigating between main screens
- **Slide**: Left/right slide when navigating bank/page changes
- **Fade-out/fade-in**: When opening deep editing screens (Screen 2-5)
- **No animation**: Instant when switching metering/analysis views (Screen 6)

### Touch Gesture Summary
| Gesture | Action | Screens |
|---------|--------|---------|
| **Tap** | Select/toggle | All |
| **Double-tap** | Zoom/focus | Screens 3, 6 |
| **Drag (vertical)** | Scroll lists/faders | All |
| **Drag (horizontal)** | Scroll banks, adjust parameters | All |
| **Pinch** | Zoom/frequency range | Screens 3, 6 |
| **Two-finger drag** | Scroll independently | Screens 3, 6 |
| **Long-press (1s)** | Open context menu | All |
| **Swipe left/right** | Delete/navigate back | Screens 1, 4, 7 |
| **Swipe down** | Collapse/minimize | Screens 4, 7 |

---

## QUALITY ATTRIBUTES

### Premium Professional Appearance
- Consistent dark theme across all screens
- High contrast for low-light live performance environments
- Polished interactions with immediate feedback
- No rough edges or fallback UI states

### Clear Typography
- Roboto/Nunito font family throughout
- Hierarchical text sizes (display → body → caption)
- Sufficient colour contrast (minimum 4.5:1 AA compliance)
- Label always visible near controlled parameter

### Consistent Spacing
- 8px base grid system
- Consistent padding (12px internal, 8px external)
- Uniform touch target spacing (minimum 8px between elements)
- Consistent margin usage (16px screen edges, 8px between sections)

### Fast Live Mixing Workflow
- Primary controls (faders, mutes, solos) reachable in 1-2 taps
- Bank navigation with single click (no deep sub-menus for common tasks)
- Touch-friendly sizes for rapid adjustment under performance conditions
- Visual feedback within 100ms of user action

### Accurate Meter Visualization
- True peak metering with appropriate integration time
- Peak hold functionality with configurable duration
- Channel-to-channel consistency in meter behaviour
- RTA/spectrogram accurate to audio engine

### Large Touch Targets
- Minimum 44×44 px all interactive elements
- 48×48 px recommended for primary actions
- 8px minimum spacing between touch targets
- Edge-to-edge action bars (48px high)

### Smooth Interface Navigation
- 60 fps UI animation where used
- 100ms maximum response time to user input
- No jank or stutter during navigation
- Consistent easing curves (cubic-bezier)

### Consistent Colour Coding
- Input channels: Nova orange/amber spectrum
- Output/master: Nova green/cyan spectrum
- Buses (AUX/matrix): Nova blue spectrum
- Dugan automixer: Nova purple spectrum
- VCA groups: Nova orange-red spectrum
- Consistent across all 7 screens

### Responsive Layout
- Adapts from 21-inch (3840×2160 assumed) to smaller tablets
- Columns reflow based on available width
- Touch targets maintain minimum size at all resolutions
- Layout breaks gracefully (not silently)

### Avoid Unnecessary Visual Clutter
- Hide secondary controls by default (reveal on interaction)
- Progressive disclosure (complex parameters on demand)
- White space usage (minimum 8px between groups)
- Hide meter panels when not needed (auto-hide)

### DSP Performance
- UI updates at 60 fps maximum (vs audio callback rates)
- No audio processing in UI threads
- All DSP in dedicated audio thread (Main.cpp callback)
- Meter updates use atomic copies, no lock contention
- Effect DSP usage displayed but not counted against audio budget unless critical

---

## IMPLEMENTATION NOTES

### JUCE Architecture Integration
- **NovaConsoleSurfaceJuce.h**: Existing 28-fader surface — redesign adds new ChannelStrip component with full processing controls
- **NovaConsoleSurfaceModel.h**: Existing layer/bank navigation — redesign extends model with channel processing state
- **NovaLookAndFeel.h**: Existing dark theme — redesign extends colour palette, keeps existing foundation
- **Main.cpp**: Existing audio engine — redesign maps UI controls to existing ChannelState parameters without modification

### Files to Create/Modify
| File | Action |
|------|--------|
| **NOVA_HD_UI_REDESIGN.md** | New — this specification |
| **NOVA_HD_SCREEN_SPECIFICATION.md** | New — per-screen details (could split from above) |
| **NOVA_HD_UI_IMPLEMENTATION_PLAN.md** | New — implementation roadmap |
| **Source/NovaConsoleSurfaceJuce.h** | Extend with new ChannelStrip properties |
| **Source/NovaConsoleSurfaceModel.h** | Extend with processing state, expand beyond 28 faders |
| **Source/NovaLookAndFeel.h** | Extend colour palette, add new UI colour definitions |
| **Source/Main.cpp** | Map new UI controls to existing ChannelState (no DSP changes) |
| **CMakeLists.txt** | Any new resource files, JUCE modules |

### Known Constraints
1. **Single-window JUCE app**: All 7 screens in one window with tab/navigation switching
2. **144 logical channels**: UI must handle full channel count with 28-per-page bank navigation
3. **Existing DSP preserved**: No changes to audio processing chain, only UI wiring
4. **Touch-first**: Designed for 21-inch touchscreen, mouse/keyboard secondary
5. **Backward compatible**: All existing parameter interfaces unchanged
6. **Performance**: UI at 60 fps, audio at sample rate (44.1/48/88.2/96/192 kHz)

### Testing Requirements
- **Unit Tests**: Individual parameter UI → DSP mapping
- **Integration Tests**: Full screen navigation flow
- **Touch Tests**: Touch target sizes on 21-inch display at designed resolution
- **Performance Tests**: UI frame time < 16ms at 60 fps
- **Backward Compatibility**: Existing show files recall correctly with new UI
- **Regression Tests**: All existing audio tests pass unchanged

### Rollback Plan
1. Current UI state backed up before any modifications
2. All changes to NovaConsoleSurface* and Main.cpp are additive only
3. New colour definitions added alongside existing ones
4. New UI screens opt-in via configuration, not replacement
5. Full revert possible by restoring original files from backup

### Future Considerations
- **Remote Control**: OSC/MIDI remote control mappings
- **Recall Memory**: Show file auto-save/recovery
- **Network**: Multi-user show control (tablet interfaces)
- **AI Mixing Assist**: Future integration with AI mixing suggestions
- **Rec. 709**: Colour space management for video output
- **MADI Integration**: Digital audio interface integration

---

## DESIGN IMPLEMENTATION PHASES

### PHASE 1: Foundation (Weeks 1-2)
- [ ] Extend NovaLookAndFeel.h with full colour palette
- [ ] Update NovaConsoleSurfaceModel.h for processing state
- [ ] Create implementation plan document
- [ ] Set up JUCE project structure for 7 screens

### PHASE 2: Screen Implementation (Weeks 3-8)
- [ ] Screen 1: Main Mixer component
- [ ] Screen 2: Channel Processing component
- [ ] Screen 3: Parametric EQ component
- [ ] Screen 4: Routing Matrix component
- [ ] Screen 5: Effects Rack component

### PHASE 3: Analysis Screens (Weeks 9-10)
- [ ] Screen 6: Meters component
- [ ] Screen 7: Show Management component

### PHASE 4: Integration & Testing (Weeks 11-12)
- [ ] Navigation between all 7 screens
- [ ] Parameter mapping verification
- [ ] Touch target validation on 21-inch display
- [ ] Performance profiling (UI < 16ms/frame)
- [ ] Backward compatibility testing
- [ ] Regression test suite

### PHASE 5: Polish & Release (Weeks 13-14)
- [ ] Visual refinement based on feedback
- [ ] Documentation updates
- [ ] User acceptance testing
- [ ] Release preparation

---

## DESIGN REVIEW CHECKLIST

### Pre-Implementation Review
- [ ] All 7 screen specifications approved
- [ ] Colour palette validated against branding
- [ ] Touch target sizes meet 44×44 px minimum
- [ ] Navigation flow documented and reviewed
- [ ] DSP parameter mapping verified (no duplication)
- [ ] Backward compatibility confirmed
- [ ] Performance targets established (UI < 16ms/frame)
- [ ] Implementation plan approved

### Post-Implementation Review
- [ ] All 7 screens implemented per specification
- [ ] Navigation flows work end-to-end
- [ ] All parameter controls map to DSP correctly
- [ ] Meter accuracy verified against audio engine
- [ ] Performance meets targets (60 fps UI)
- [ ] Backward compatibility maintained
- [ ] No regression in existing functionality
- [ ] User acceptance testing completed

---

**END OF NOVA HD COMPLETE UI REDESIGN SPECIFICATION**

*Document generated for NOVA HD professional digital mixing console redesign.*
*Version: 1.0 — November 2026*
*Lead UI/UX Architect: NOVA HD Development Team*