#pragma once

#include <juce_core/juce_core.h>
#include <string>
#include <vector>
#include <array>
#include <atomic>
#include <memory>
#include <cstring>
#include <algorithm>

namespace nova::graviton {

/**
 * @brief Lightweight, zero-allocation OSC 1.0 message representation
 */
struct OscMessage {
    std::string address;
    std::string typeTags;
    std::vector<float> floatArgs;
    std::vector<int32_t> intArgs;
    std::vector<std::string> stringArgs;

    void clear() {
        address.clear();
        typeTags.clear();
        floatArgs.clear();
        intArgs.clear();
        stringArgs.clear();
    }
};

/**
 * @brief Real-time OSC 1.0 parser and packet serializer conforming to the standard specification
 */
class OscCodec final {
public:
    static size_t padTo4(size_t len) noexcept {
        return (len + 3) & ~size_t(3);
    }

    static bool parse(const uint8_t* data, size_t size, OscMessage& out) {
        out.clear();
        if (data == nullptr || size < 4 || data[0] != '/')
            return false;

        size_t offset = 0;

        // 1. Read Address String
        size_t addrEnd = offset;
        while (addrEnd < size && data[addrEnd] != '\0') {
            addrEnd++;
        }
        if (addrEnd >= size) return false;
        out.address.assign(reinterpret_cast<const char*>(data + offset), addrEnd - offset);
        offset = padTo4(addrEnd + 1);
        if (offset > size) return false;

        if (offset == size) return true; // Message with address only is valid

        // 2. Read Type Tags String
        if (data[offset] != ',')
            return false; // Payload without a type-tag string is malformed

        size_t tagEnd = offset;
        while (tagEnd < size && data[tagEnd] != '\0') {
            tagEnd++;
        }
        if (tagEnd >= size) return false;
        out.typeTags.assign(reinterpret_cast<const char*>(data + offset + 1), tagEnd - (offset + 1));
        offset = padTo4(tagEnd + 1);
        if (offset > size) return false;

        // 3. Read Arguments according to Type Tags
        for (char tag : out.typeTags) {
            switch (tag) {
                case 'f': {
                    if (offset + 4 > size) return false;
                    uint32_t u = (static_cast<uint32_t>(data[offset]) << 24) |
                                 (static_cast<uint32_t>(data[offset + 1]) << 16) |
                                 (static_cast<uint32_t>(data[offset + 2]) << 8) |
                                 static_cast<uint32_t>(data[offset + 3]);
                    float val;
                    std::memcpy(&val, &u, 4);
                    out.floatArgs.push_back(val);
                    offset += 4;
                    break;
                }
                case 'i': {
                    if (offset + 4 > size) return false;
                    const uint32_t bits =
                        (static_cast<uint32_t>(data[offset]) << 24) |
                        (static_cast<uint32_t>(data[offset + 1]) << 16) |
                        (static_cast<uint32_t>(data[offset + 2]) << 8) |
                        static_cast<uint32_t>(data[offset + 3]);
                    int32_t val = 0;
                    static_assert(sizeof(val) == sizeof(bits));
                    std::memcpy(&val, &bits, sizeof(val));
                    out.intArgs.push_back(val);
                    offset += 4;
                    break;
                }
                case 's': {
                    size_t sEnd = offset;
                    while (sEnd < size && data[sEnd] != '\0') {
                        sEnd++;
                    }
                    if (sEnd >= size) return false;
                    std::string str(reinterpret_cast<const char*>(data + offset), sEnd - offset);
                    out.stringArgs.push_back(str);
                    offset = padTo4(sEnd + 1);
                    if (offset > size) return false;
                    break;
                }
                // OSC type tags without payload (OSC 1.0).
                case 'T':
                case 'F':
                case 'N':
                case 'I':
                    break;
                default:
                    // Unknown tags have unknown payload sizes; reject rather than
                    // silently desynchronizing the remaining argument stream.
                    out.clear();
                    return false;
            }
        }

        return true;
    }

    static size_t build(const OscMessage& msg, uint8_t* buffer, size_t maxLen) {
        if (buffer == nullptr || msg.address.empty() || msg.address[0] != '/')
            return 0;

        size_t offset = 0;

        // 1. Write Address
        const size_t addrLen = msg.address.length();
        const size_t addrPadded = padTo4(addrLen + 1);
        if (offset + addrPadded > maxLen) return 0;
        std::memcpy(buffer + offset, msg.address.c_str(), addrLen);
        std::memset(buffer + offset + addrLen, 0, addrPadded - addrLen);
        offset += addrPadded;

        // 2. Write Type Tags
        std::string tags = "," + msg.typeTags;
        const size_t tagLen = tags.length();
        const size_t tagPadded = padTo4(tagLen + 1);
        if (offset + tagPadded > maxLen) return 0;
        std::memcpy(buffer + offset, tags.c_str(), tagLen);
        std::memset(buffer + offset + tagLen, 0, tagPadded - tagLen);
        offset += tagPadded;

        // 3. Write Arguments
        size_t fIdx = 0, iIdx = 0, sIdx = 0;
        for (char tag : msg.typeTags) {
            switch (tag) {
                case 'f': {
                    if (offset + 4 > maxLen || fIdx >= msg.floatArgs.size()) return 0;
                    float val = msg.floatArgs[fIdx++];
                    uint32_t u;
                    std::memcpy(&u, &val, 4);
                    buffer[offset]     = static_cast<uint8_t>((u >> 24) & 0xff);
                    buffer[offset + 1] = static_cast<uint8_t>((u >> 16) & 0xff);
                    buffer[offset + 2] = static_cast<uint8_t>((u >> 8) & 0xff);
                    buffer[offset + 3] = static_cast<uint8_t>(u & 0xff);
                    offset += 4;
                    break;
                }
                case 'i': {
                    if (offset + 4 > maxLen || iIdx >= msg.intArgs.size()) return 0;
                    int32_t val = msg.intArgs[iIdx++];
                    uint32_t u = static_cast<uint32_t>(val);
                    buffer[offset]     = static_cast<uint8_t>((u >> 24) & 0xff);
                    buffer[offset + 1] = static_cast<uint8_t>((u >> 16) & 0xff);
                    buffer[offset + 2] = static_cast<uint8_t>((u >> 8) & 0xff);
                    buffer[offset + 3] = static_cast<uint8_t>(u & 0xff);
                    offset += 4;
                    break;
                }
                case 's': {
                    if (sIdx >= msg.stringArgs.size()) return 0;
                    const auto& str = msg.stringArgs[sIdx++];
                    const size_t strLen = str.length();
                    const size_t strPadded = padTo4(strLen + 1);
                    if (offset + strPadded > maxLen) return 0;
                    std::memcpy(buffer + offset, str.c_str(), strLen);
                    std::memset(buffer + offset + strLen, 0, strPadded - strLen);
                    offset += strPadded;
                    break;
                }
                default:
                    break;
            }
        }

        return offset;
    }
};

/**
 * @brief High-speed UDP Network Daemon for wireless tablet mixing (MIDAS HD96-24 Chapter 27)
 */
class OscServer final : public juce::Thread {
public:
    std::function<void(const OscMessage&, const juce::String& senderIp, int senderPort)> onMessageReceived;

    OscServer() : juce::Thread("NOVA OSC Server") {}

    ~OscServer() override {
        stopServer();
    }

    // UDP OSC has no built-in authentication. Default-deny remote senders;
    // explicitly allow only trusted controller IP addresses.
    void setAllowedAddresses(const std::vector<juce::String>& addresses) {
        std::vector<juce::String> clean;
        clean.reserve(addresses.size());
        for (auto address : addresses) {
            address = address.trim();
            if (address.isNotEmpty() && std::find(clean.begin(), clean.end(), address) == clean.end())
                clean.push_back(address);
        }
        const juce::ScopedLock sl(clientLock);
        allowedAddresses = std::move(clean);
    }

    std::vector<juce::String> getAllowedAddresses() const {
        const juce::ScopedLock sl(clientLock);
        return allowedAddresses;
    }

    bool startServer(int port = 53000) {
        stopServer();

        serverPort = port;
        socket = std::make_unique<juce::DatagramSocket>();
        if (!socket->bindToPort(port)) {
            socket.reset();
            return false;
        }

        packetsReceived.store(0, std::memory_order_relaxed);
        packetsSent.store(0, std::memory_order_relaxed);
        running.store(true, std::memory_order_relaxed);
        startThread(juce::Thread::Priority::high);
        return true;
    }

    void stopServer() {
        if (isThreadRunning()) {
            running.store(false, std::memory_order_relaxed);
            if (socket != nullptr) {
                socket->shutdown();
            }
            stopThread(500);
        }
        const juce::ScopedLock sl(socketLock);
        socket.reset();
    }

    bool isServerRunning() const noexcept {
        return running.load(std::memory_order_relaxed) && isThreadRunning();
    }

    int getServerPort() const noexcept {
        return serverPort;
    }

    uint64_t getPacketsReceived() const noexcept {
        return packetsReceived.load(std::memory_order_relaxed);
    }

    uint64_t getPacketsSent() const noexcept {
        return packetsSent.load(std::memory_order_relaxed);
    }

    juce::String getLastClientAddress() const {
        const juce::ScopedLock sl(clientLock);
        return lastClientIp.isEmpty() ? "No clients connected" : (lastClientIp + ":" + juce::String(lastClientPort));
    }

    size_t getActiveClientCount() const {
        const juce::ScopedLock sl(clientLock);
        return registeredClients.size();
    }

    void sendToAllClients(const OscMessage& msg) {
        std::vector<std::pair<juce::String, int>> targets;
        {
            const juce::ScopedLock sl(clientLock);
            targets = registeredClients;
        }
        for (const auto& t : targets) {
            sendMessage(msg, t.first, t.second);
        }
    }

    void sendMessage(const OscMessage& msg, const juce::String& targetIp, int targetPort) {
        if (!running.load(std::memory_order_relaxed))
            return;

        uint8_t buffer[2048];
        const size_t len = OscCodec::build(msg, buffer, sizeof(buffer));
        if (len > 0) {
            const juce::ScopedLock sl(socketLock);
            if (socket != nullptr) {
                socket->write(targetIp, targetPort, buffer, static_cast<int>(len));
                packetsSent.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    void run() override {
        std::array<uint8_t, 4096> rxBuffer;
        OscMessage parsedMsg;

        while (!threadShouldExit() && running.load(std::memory_order_relaxed)) {
            if (socket == nullptr)
                break;

            // Wait up to 50ms for incoming UDP datagram
            const int ready = socket->waitUntilReady(true, 50);
            if (ready <= 0)
                continue;

            juce::String senderIp;
            int senderPort = 0;
            // shouldBlock = false: return the datagram packet immediately without waiting for 4096 bytes
            const int bytesRead = socket->read(rxBuffer.data(), static_cast<int>(rxBuffer.size()), false, senderIp, senderPort);

            if (bytesRead > 0) {
                packetsReceived.fetch_add(1, std::memory_order_relaxed);

                bool senderAllowed = false;
                {
                    const juce::ScopedLock sl(clientLock);
                    senderAllowed = std::find(allowedAddresses.begin(), allowedAddresses.end(), senderIp) != allowedAddresses.end();
                }
                if (!senderAllowed)
                    continue;

                // Apply the rate ceiling only after allowlist validation so an
                // untrusted sender cannot consume the trusted controller budget.
                const uint32_t nowMs = juce::Time::getMillisecondCounter();
                if (static_cast<uint32_t>(nowMs - rateWindowStartMs_) >= 1000u) {
                    rateWindowStartMs_ = nowMs;
                    rateWindowPackets_ = 0;
                }
                if (++rateWindowPackets_ > kMaxPacketsPerSecond)
                    continue;

                {
                    const juce::ScopedLock sl(clientLock);
                    lastClientIp = senderIp;
                    lastClientPort = senderPort;
                    bool exists = false;
                    for (const auto& c : registeredClients) {
                        if (c.first == senderIp && c.second == senderPort) { exists = true; break; }
                    }
                    if (!exists && !senderIp.isEmpty() && senderPort > 0) {
                        if (registeredClients.size() >= 64) {
                            registeredClients.erase(registeredClients.begin());
                        }
                        registeredClients.push_back({senderIp, senderPort});
                    }
                }

                if (OscCodec::parse(rxBuffer.data(), static_cast<size_t>(bytesRead), parsedMsg)) {
                    if (onMessageReceived) {
                        onMessageReceived(parsedMsg, senderIp, senderPort);
                    }
                }
            } else {
                // Short wait to avoid tight polling loop
                wait(2);
            }
        }
    }

private:
    std::unique_ptr<juce::DatagramSocket> socket;
    int serverPort{53000};
    std::atomic<bool> running{false};
    std::atomic<uint64_t> packetsReceived{0};
    std::atomic<uint64_t> packetsSent{0};

    mutable juce::CriticalSection clientLock;
    mutable juce::CriticalSection socketLock;
    juce::String lastClientIp;
    int lastClientPort{0};
    std::vector<std::pair<juce::String, int>> registeredClients;
    // Loopback-only until the host explicitly configures trusted tablet IPs.
    std::vector<juce::String> allowedAddresses{"127.0.0.1", "::1"};
    static constexpr uint32_t kMaxPacketsPerSecond = 1000;
    uint32_t rateWindowStartMs_ = 0;
    uint32_t rateWindowPackets_ = 0;
};

} // namespace nova::graviton
