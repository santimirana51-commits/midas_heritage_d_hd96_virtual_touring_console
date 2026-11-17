#pragma once

#include "Routing/ScalableBusGraph.h"
#include <cmath>
#include <iomanip>
#include <istream>
#include <limits>
#include <sstream>
#include <string>

namespace nova
{
// Versioned, deterministic text codec for scalable bus configuration.
// Call encode/decode only on the control thread, never from the audio callback.
inline constexpr const char* kBusConfigFileMarker = "NOVA_HD_BUSCFG";
inline constexpr int kBusConfigFileVersion = 1;

inline bool isValidBusConfig(const ScalableBusGraphConfig& config) noexcept
{
    for (int channel = 0; channel < kBusGraphChannels; ++channel)
    {
        for (int aux = 0; aux < kBusGraphAuxCount; ++aux)
        {
            const float gain = config.sendGain[static_cast<std::size_t>(channel)][static_cast<std::size_t>(aux)];
            if (!std::isfinite(gain) || gain < 0.0f || gain > 16.0f)
                return false;
        }
    }

    for (float gain : config.auxMasterGain)
        if (!std::isfinite(gain) || gain < 0.0f || gain > 16.0f)
            return false;

    for (const auto& route : config.matrix)
    {
        if (!std::isfinite(route.masterGain) || route.masterGain < 0.0f || route.masterGain > 16.0f)
            return false;
        switch (route.kind)
        {
            case MatrixSourceKind::MainMono:
                break;
            case MatrixSourceKind::AuxBus:
                if (route.sourceIndex < 0 || route.sourceIndex >= kBusGraphAuxCount)
                    return false;
                break;
            case MatrixSourceKind::AuxRangeSum:
                if (route.sourceIndex < 0 || route.sourceIndex >= kBusGraphAuxCount
                    || route.sourceEndExclusive <= route.sourceIndex
                    || route.sourceEndExclusive > kBusGraphAuxCount)
                    return false;
                break;
            default:
                return false;
        }
    }
    return true;
}

inline std::string encodeBusConfig(const ScalableBusGraphConfig& config)
{
    if (!isValidBusConfig(config))
        return {};

    std::ostringstream out;
    out << kBusConfigFileMarker << ' ' << kBusConfigFileVersion << '\n';
    out << std::setprecision(std::numeric_limits<float>::max_digits10);
    for (int channel = 0; channel < kBusGraphChannels; ++channel)
        for (int aux = 0; aux < kBusGraphAuxCount; ++aux)
        {
            const auto c = static_cast<std::size_t>(channel);
            const auto a = static_cast<std::size_t>(aux);
            const float gain = config.sendGain[c][a];
            if (gain > 0.0f)
                out << "SEND " << channel << ' ' << aux << ' ' << gain << ' '
                    << (config.sendPreFader[c][a] ? 1 : 0) << '\n';
        }
    for (int aux = 0; aux < kBusGraphAuxCount; ++aux)
    {
        const auto a = static_cast<std::size_t>(aux);
        out << "AUX " << aux << ' ' << config.auxMasterGain[a] << ' '
            << (config.auxMuted[a] ? 1 : 0) << '\n';
    }
    for (int matrix = 0; matrix < kBusGraphMatrixCount; ++matrix)
    {
        const auto& route = config.matrix[static_cast<std::size_t>(matrix)];
        out << "MATRIX " << matrix << ' ' << static_cast<int>(route.kind) << ' '
            << route.sourceIndex << ' ' << route.sourceEndExclusive << ' '
            << route.masterGain << ' ' << (route.muted ? 1 : 0) << '\n';
    }
    return out.str();
}

// Transactional decode: destination is only modified after the entire document
// has parsed and passed validation. Unknown records and duplicate indexes fail closed.
inline bool decodeBusConfig(const std::string& text, ScalableBusGraphConfig& destination,
                            std::string* error = nullptr)
{
    auto fail = [error](const char* message)
    {
        if (error != nullptr) *error = message;
        return false;
    };

    std::istringstream input(text);
    std::string marker;
    int version = 0;
    if (!(input >> marker >> version) || marker != kBusConfigFileMarker)
        return fail("Invalid bus configuration marker");
    if (version != kBusConfigFileVersion)
        return fail("Unsupported bus configuration version");

    ScalableBusGraphConfig parsed;
    std::array<std::array<bool, kBusGraphAuxCount>, kBusGraphChannels> seenSend {};
    std::array<bool, kBusGraphAuxCount> seenAux {};
    std::array<bool, kBusGraphMatrixCount> seenMatrix {};
    std::string record;
    while (input >> record)
    {
        if (record == "SEND")
        {
            int channel = -1, aux = -1, pre = -1;
            float gain = 0.0f;
            if (!(input >> channel >> aux >> gain >> pre)) return fail("Malformed SEND record");
            if (channel < 0 || channel >= kBusGraphChannels || aux < 0 || aux >= kBusGraphAuxCount
                || pre < 0 || pre > 1 || !std::isfinite(gain) || gain <= 0.0f || gain > 16.0f)
                return fail("SEND record out of range");
            auto& seen = seenSend[static_cast<std::size_t>(channel)][static_cast<std::size_t>(aux)];
            if (seen) return fail("Duplicate SEND record");
            seen = true;
            parsed.sendGain[static_cast<std::size_t>(channel)][static_cast<std::size_t>(aux)] = gain;
            parsed.sendPreFader[static_cast<std::size_t>(channel)][static_cast<std::size_t>(aux)] = (pre != 0);
        }
        else if (record == "AUX")
        {
            int aux = -1, muted = -1;
            float gain = 0.0f;
            if (!(input >> aux >> gain >> muted)) return fail("Malformed AUX record");
            if (aux < 0 || aux >= kBusGraphAuxCount || muted < 0 || muted > 1
                || !std::isfinite(gain) || gain < 0.0f || gain > 16.0f)
                return fail("AUX record out of range");
            const auto index = static_cast<std::size_t>(aux);
            if (seenAux[index]) return fail("Duplicate AUX record");
            seenAux[index] = true;
            parsed.auxMasterGain[index] = gain;
            parsed.auxMuted[index] = muted != 0;
        }
        else if (record == "MATRIX")
        {
            int matrix = -1, kind = -1, source = -1, end = -1, muted = -1;
            float gain = 0.0f;
            if (!(input >> matrix >> kind >> source >> end >> gain >> muted))
                return fail("Malformed MATRIX record");
            if (matrix < 0 || matrix >= kBusGraphMatrixCount || kind < 0 || kind > 2
                || muted < 0 || muted > 1 || !std::isfinite(gain) || gain < 0.0f || gain > 16.0f)
                return fail("MATRIX record out of range");
            const auto index = static_cast<std::size_t>(matrix);
            if (seenMatrix[index]) return fail("Duplicate MATRIX record");
            seenMatrix[index] = true;
            auto& route = parsed.matrix[index];
            route.kind = static_cast<MatrixSourceKind>(kind);
            route.sourceIndex = source;
            route.sourceEndExclusive = end;
            route.masterGain = gain;
            route.muted = muted != 0;
        }
        else
            return fail("Unknown bus configuration record");
    }

    for (bool seen : seenAux) if (!seen) return fail("Missing AUX record");
    for (bool seen : seenMatrix) if (!seen) return fail("Missing MATRIX record");
    if (!isValidBusConfig(parsed)) return fail("Bus configuration failed validation");
    destination = parsed;
    if (error != nullptr) error->clear();
    return true;
}
} // namespace nova
