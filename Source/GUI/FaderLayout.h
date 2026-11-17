#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace nova {

inline bool isValidFaderLayout(const std::vector<int>& channels,
                               int logicalChannelCount) noexcept
{
    if (logicalChannelCount < 1 ||
        logicalChannelCount > 144 ||
        channels.size() > static_cast<std::size_t>(logicalChannelCount))
        return false;

    std::array<bool, 144> seen {};
    for (const int channel : channels)
    {
        if (channel < 0 || channel >= logicalChannelCount ||
            seen[static_cast<std::size_t>(channel)])
            return false;
        seen[static_cast<std::size_t>(channel)] = true;
    }
    return true;
}

inline int faderLayoutChannelCount(const std::vector<int>& channels,
                                   bool customLayoutActive,
                                   int logicalChannelCount) noexcept
{
    return customLayoutActive
        ? static_cast<int>(channels.size())
        : logicalChannelCount;
}

inline int faderLayoutChannelAt(const std::vector<int>& channels,
                                bool customLayoutActive,
                                int position,
                                int logicalChannelCount) noexcept
{
    const int count = faderLayoutChannelCount(
        channels, customLayoutActive, logicalChannelCount);
    if (position < 0 || position >= count)
        return -1;
    return customLayoutActive ? channels[static_cast<std::size_t>(position)]
                              : position;
}

inline int faderLayoutPageCount(int channelCount, int pageSize) noexcept
{
    if (channelCount <= 0 || pageSize <= 0)
        return 0;
    return (channelCount + pageSize - 1) / pageSize;
}

} // namespace nova
