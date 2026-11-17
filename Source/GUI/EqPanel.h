#pragma once
#include "GUI/DynamicEqPanel.h"

namespace nova {

/**
 * @brief Unified Equalizer Panel
 *
 * Integrates full Parametric EQ (PEQ) and Dynamic EQ into a single cohesive
 * 3-area architecture based on the TDR NOVA Parallel Dynamic Equalizer blueprint:
 *   - Area 1: Wideband RTA & Interactive Response Graph (10 Hz - 40 kHz, -12 to +12 dB)
 *   - Area 2: Band Selector (I, II, III, IV, Solo, Order by Freq, Bypass, GR Delta)
 *   - Area 3: Technical Parameter Controls (HP/LP Filters, Filter Shapes, PEQ, Dynamic EQ, Output & Mix)
 */
class EqPanel : public DynamicEqPanel {
public:
  using DynamicEqPanel::DynamicEqPanel;
};

} // namespace nova
