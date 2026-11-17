package com.openlive.remote

import java.io.ByteArrayOutputStream
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.Executors

data class OscMessage(
    val address: String,
    val typeTags: String = "",
    val floatArgs: List<Float> = emptyList(),
    val intArgs: List<Int> = emptyList(),
    val stringArgs: List<String> = emptyList()
)

object OscCodec {
    private fun writePaddedString(out: ByteArrayOutputStream, value: String) {
        val bytes = value.toByteArray(Charsets.UTF_8)
        out.write(bytes)
        out.write(0)
        val total = bytes.size + 1
        val pad = (4 - (total % 4)) % 4
        for (i in 0 until pad) out.write(0)
    }

    fun build(msg: OscMessage): ByteArray {
        val out = ByteArrayOutputStream(256)
        writePaddedString(out, msg.address)
        writePaddedString(out, "," + msg.typeTags)

        var fIdx = 0
        var iIdx = 0
        var sIdx = 0
        val bb4 = ByteBuffer.allocate(4).order(ByteOrder.BIG_ENDIAN)

        for (tag in msg.typeTags) {
            when (tag) {
                'f' -> {
                    bb4.clear()
                    bb4.putFloat(msg.floatArgs.getOrElse(fIdx++) { 0f })
                    out.write(bb4.array())
                }
                'i' -> {
                    bb4.clear()
                    bb4.putInt(msg.intArgs.getOrElse(iIdx++) { 0 })
                    out.write(bb4.array())
                }
                's' -> {
                    writePaddedString(out, msg.stringArgs.getOrElse(sIdx++) { "" })
                }
            }
        }
        return out.toByteArray()
    }

    fun parse(data: ByteArray, length: Int): OscMessage? {
        if (length < 4) return null
        var offset = 0

        fun readPaddedString(): String? {
            if (offset >= length) return null
            var end = offset
            while (end < length && data[end].toInt() != 0) end++
            if (end >= length) return null
            val s = String(data, offset, end - offset, Charsets.UTF_8)
            val total = (end - offset) + 1
            offset += total + ((4 - (total % 4)) % 4)
            return s
        }

        val address = readPaddedString() ?: return null
        val tagsRaw = readPaddedString() ?: return null
        if (!tagsRaw.startsWith(",")) return null
        val tags = tagsRaw.substring(1)

        val floats = mutableListOf<Float>()
        val ints = mutableListOf<Int>()
        val strings = mutableListOf<String>()

        for (tag in tags) {
            when (tag) {
                'f' -> {
                    if (offset + 4 > length) break
                    floats.add(ByteBuffer.wrap(data, offset, 4).order(ByteOrder.BIG_ENDIAN).float)
                    offset += 4
                }
                'i' -> {
                    if (offset + 4 > length) break
                    ints.add(ByteBuffer.wrap(data, offset, 4).order(ByteOrder.BIG_ENDIAN).int)
                    offset += 4
                }
                's' -> {
                    val s = readPaddedString() ?: break
                    strings.add(s)
                }
            }
        }
        return OscMessage(address, tags, floats, ints, strings)
    }
}

class HeritageOscClient(
    private val onInfoReceived: (String) -> Unit
) {
    private val txExecutor = Executors.newSingleThreadExecutor()
    @Volatile private var targetIp: String = "192.168.1.100"
    @Volatile private var targetPort: Int = 53000
    @Volatile private var socket: DatagramSocket? = null

    fun configure(ip: String, port: Int) {
        targetIp = ip.trim()
        targetPort = port.coerceIn(1, 65535)
    }

    fun ping() {
        txExecutor.execute {
            try {
                if (socket == null || socket?.isClosed == true) {
                    socket = DatagramSocket().apply { soTimeout = 1200 }
                }
                val sock = socket ?: return@execute
                val payload = OscCodec.build(OscMessage("/ping"))
                val addr = InetAddress.getByName(targetIp)
                sock.send(DatagramPacket(payload, payload.size, addr, targetPort))

                val rx = ByteArray(1024)
                val pkt = DatagramPacket(rx, rx.size)
                sock.receive(pkt)
                val parsed = OscCodec.parse(pkt.data, pkt.length)
                if (parsed != null && parsed.address == "/info") {
                    val name = parsed.stringArgs.getOrElse(0) { "OPENLIVE" }
                    val ver = parsed.stringArgs.getOrElse(2) { "" }
                    val ch = parsed.intArgs.getOrElse(0) { 144 }
                    onInfoReceived("CONNECTED: $name $ver ($ch ch)")
                } else {
                    onInfoReceived("CONNECTED (${pkt.address.hostAddress})")
                }
            } catch (e: Exception) {
                onInfoReceived("TIMEOUT / NO REPLY (${e.message ?: "UDP"})")
            }
        }
    }

    fun sendFloat(address: String, value: Float) {
        send(OscMessage(address, "f", floatArgs = listOf(value)))
    }

    fun sendInt(address: String, value: Int) {
        send(OscMessage(address, "i", intArgs = listOf(value)))
    }

    private fun send(msg: OscMessage) {
        txExecutor.execute {
            try {
                if (socket == null || socket?.isClosed == true) {
                    socket = DatagramSocket().apply { soTimeout = 1200 }
                }
                val sock = socket ?: return@execute
                val payload = OscCodec.build(msg)
                val addr = InetAddress.getByName(targetIp)
                sock.send(DatagramPacket(payload, payload.size, addr, targetPort))
            } catch (_: Exception) {
            }
        }
    }

    fun close() {
        txExecutor.execute {
            try {
                socket?.close()
            } catch (_: Exception) {
            }
            socket = null
        }
        txExecutor.shutdown()
    }
}
