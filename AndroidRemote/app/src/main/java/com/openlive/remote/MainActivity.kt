package com.openlive.remote

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.graphics.Typeface
import android.os.Bundle
import android.text.InputType
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.widget.*
import androidx.appcompat.app.AppCompatActivity

/**
 * Authentic Vertical Mixing Console Fader with real-time vertical drag tracking
 */
class VerticalFaderView(
    context: Context,
    private val capColor: Int = Color.parseColor("#38BDF8")
) : View(context) {

    var value: Float = 0.8571f
        set(v) {
            field = v.coerceIn(0f, 1f)
            postInvalidate()
        }

    var onFaderChanged: ((Float) -> Unit)? = null

    private val trackPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.parseColor("#0B0F17")
        style = Paint.Style.FILL
    }
    private val slotPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.parseColor("#030712")
        style = Paint.Style.STROKE
        strokeWidth = 10f
    }
    private val slotCenterPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.BLACK
        style = Paint.Style.STROKE
        strokeWidth = 3f
    }
    private val capPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = capColor
        style = Paint.Style.FILL
    }
    private val capBorderPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.parseColor("#CBD5E1")
        style = Paint.Style.STROKE
        strokeWidth = 2f
    }
    private val notchPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        strokeWidth = 4f
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val w = width.toFloat()
        val h = height.toFloat()
        val pad = 30f
        val usableH = h - 2 * pad
        val capH = 44f
        val capW = (w - 12f).coerceAtLeast(24f)

        // Background track slot
        canvas.drawRoundRect(8f, pad, w - 8f, h - pad, 6f, 6f, trackPaint)
        canvas.drawLine(w / 2f, pad + 10f, w / 2f, h - pad - 10f, slotPaint)
        canvas.drawLine(w / 2f, pad + 10f, w / 2f, h - pad - 10f, slotCenterPaint)

        // Fader cap: value = 0 is bottom (h - pad), value = 1 is top (pad)
        val capY = (h - pad) - (value * usableH)
        val capX = (w - capW) / 2f
        val capRect = RectF(capX, capY - capH / 2f, capX + capW, capY + capH / 2f)
        canvas.drawRoundRect(capRect, 6f, 6f, capPaint)
        canvas.drawRoundRect(capRect, 6f, 6f, capBorderPaint)
        // Center white tactile notch
        canvas.drawLine(capX + 6f, capY, capX + capW - 6f, capY, notchPaint)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.action) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_MOVE -> {
                parent?.requestDisallowInterceptTouchEvent(true)
                val pad = 30f
                val usableH = height - 2 * pad
                if (usableH > 0f) {
                    val y = event.y.coerceIn(pad, height - pad)
                    val norm = 1.0f - ((y - pad) / usableH)
                    value = norm
                    onFaderChanged?.invoke(norm)
                }
                return true
            }
        }
        return super.onTouchEvent(event)
    }
}

class MainActivity : AppCompatActivity() {

    private lateinit var oscClient: HeritageOscClient
    private lateinit var statusText: TextView
    private lateinit var ipInput: EditText
    private lateinit var portInput: EditText
    private lateinit var bankLabel: TextView
    private lateinit var stripsContainer: LinearLayout

    private var currentBank = 0 // 0..17 -> 8 channels per bank (144 total)
    private val faderValues = FloatArray(144) { 0.8571f } // ~0.0 dB normalized (-60..+10)
    private val muteStates = BooleanArray(144) { false }
    private val soloStates = BooleanArray(144) { false }
    private var talkbackOn = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        oscClient = HeritageOscClient { msg ->
            runOnUiThread { statusText.text = msg }
        }

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.parseColor("#0D1117"))
            setPadding(16, 12, 16, 12)
        }

        // 1. Top Connection & Bank Bar
        val topBar = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }

        val title = TextView(this).apply {
            text = "HERITAGE HD96 REMOTE"
            setTextColor(Color.parseColor("#38BDF8"))
            textSize = 15f
            typeface = Typeface.DEFAULT_BOLD
            setPadding(0, 0, 12, 0)
        }
        topBar.addView(title)

        ipInput = EditText(this).apply {
            setText("192.168.100.8")
            hint = "PC IP"
            setTextColor(Color.WHITE)
            setHintTextColor(Color.GRAY)
            setBackgroundColor(Color.parseColor("#161B22"))
            setPadding(12, 8, 12, 8)
        }
        topBar.addView(ipInput, LinearLayout.LayoutParams(240, LinearLayout.LayoutParams.WRAP_CONTENT))

        portInput = EditText(this).apply {
            setText("53000")
            inputType = InputType.TYPE_CLASS_NUMBER
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.parseColor("#161B22"))
            setPadding(12, 8, 12, 8)
        }
        topBar.addView(portInput, LinearLayout.LayoutParams(110, LinearLayout.LayoutParams.WRAP_CONTENT).apply {
            leftMargin = 8
        })

        val btnConnect = Button(this).apply {
            text = "PING"
            setBackgroundColor(Color.parseColor("#10B981"))
            setTextColor(Color.BLACK)
            setOnClickListener {
                oscClient.configure(
                    ipInput.text.toString(),
                    portInput.text.toString().toIntOrNull() ?: 53000
                )
                statusText.text = "PINGING PC MIXER..."
                oscClient.ping()
            }
        }
        topBar.addView(btnConnect, LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT).apply {
            leftMargin = 8
        })

        val btnPrevBank = Button(this).apply {
            text = "< BANK"
            setOnClickListener {
                if (currentBank > 0) {
                    currentBank--
                    rebuildStrips()
                }
            }
        }
        topBar.addView(btnPrevBank, LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT).apply {
            leftMargin = 16
        })

        bankLabel = TextView(this).apply {
            setTextColor(Color.parseColor("#FACC15"))
            typeface = Typeface.DEFAULT_BOLD
            gravity = Gravity.CENTER
            setPadding(12, 0, 12, 0)
        }
        topBar.addView(bankLabel)

        val btnNextBank = Button(this).apply {
            text = "BANK >"
            setOnClickListener {
                if (currentBank < 17) {
                    currentBank++
                    rebuildStrips()
                }
            }
        }
        topBar.addView(btnNextBank)

        val btnTalkback = Button(this).apply {
            text = "TALKBACK"
            setBackgroundColor(Color.parseColor("#334155"))
            setTextColor(Color.WHITE)
            setOnClickListener {
                talkbackOn = !talkbackOn
                setBackgroundColor(if (talkbackOn) Color.parseColor("#EF4444") else Color.parseColor("#334155"))
                oscClient.configure(ipInput.text.toString(), portInput.text.toString().toIntOrNull() ?: 53000)
                oscClient.sendInt("/talkback/on", if (talkbackOn) 1 else 0)
            }
        }
        topBar.addView(btnTalkback, LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT).apply {
            leftMargin = 16
        })

        root.addView(topBar)

        statusText = TextView(this).apply {
            text = "Status: Tap PING untuk memverifikasi koneksi OSC ke PC mixer"
            setTextColor(Color.parseColor("#94A3B8"))
            textSize = 12f
            setPadding(0, 6, 0, 8)
        }
        root.addView(statusText)

        // 2. Main Mixer Row: 8 Channel Strips + Master L/R Strip
        val mixerRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
        }

        stripsContainer = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
        }
        mixerRow.addView(stripsContainer, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.MATCH_PARENT, 8f))

        // Master Fader Strip
        mixerRow.addView(buildMasterStrip(), LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.MATCH_PARENT, 1.25f).apply {
            leftMargin = 10
        })

        root.addView(mixerRow, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f))
        setContentView(root)
        rebuildStrips()
    }

    private fun normToDb(norm: Float): Float = -60f + norm * 70f

    private fun rebuildStrips() {
        stripsContainer.removeAllViews()
        val startCh = currentBank * 8
        bankLabel.text = "CH ${startCh + 1}-${startCh + 8}"

        for (i in 0 until 8) {
            val chIdx = startCh + i
            val chNum = chIdx + 1
            val strip = LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
                gravity = Gravity.CENTER_HORIZONTAL
                setBackgroundColor(Color.parseColor("#161B22"))
                setPadding(6, 8, 6, 8)
            }

            val lbl = TextView(this).apply {
                text = "CH $chNum"
                setTextColor(Color.parseColor("#38BDF8"))
                typeface = Typeface.DEFAULT_BOLD
                gravity = Gravity.CENTER
            }
            strip.addView(lbl)

            val dbText = TextView(this).apply {
                text = String.format("%.1f dB", normToDb(faderValues[chIdx]))
                setTextColor(Color.WHITE)
                textSize = 11f
                gravity = Gravity.CENTER
            }
            strip.addView(dbText)

            val btnMute = Button(this).apply {
                text = "MUTE"
                textSize = 10f
                setBackgroundColor(if (muteStates[chIdx]) Color.parseColor("#EF4444") else Color.parseColor("#1E293B"))
                setTextColor(Color.WHITE)
                setOnClickListener {
                    muteStates[chIdx] = !muteStates[chIdx]
                    setBackgroundColor(if (muteStates[chIdx]) Color.parseColor("#EF4444") else Color.parseColor("#1E293B"))
                    oscClient.configure(ipInput.text.toString(), portInput.text.toString().toIntOrNull() ?: 53000)
                    oscClient.sendInt("/ch/$chNum/mix/mute", if (muteStates[chIdx]) 1 else 0)
                }
            }
            strip.addView(btnMute, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 80).apply {
                topMargin = 4
            })

            val btnSolo = Button(this).apply {
                text = "SOLO"
                textSize = 10f
                setBackgroundColor(if (soloStates[chIdx]) Color.parseColor("#FACC15") else Color.parseColor("#1E293B"))
                setTextColor(if (soloStates[chIdx]) Color.BLACK else Color.WHITE)
                setOnClickListener {
                    soloStates[chIdx] = !soloStates[chIdx]
                    setBackgroundColor(if (soloStates[chIdx]) Color.parseColor("#FACC15") else Color.parseColor("#1E293B"))
                    setTextColor(if (soloStates[chIdx]) Color.BLACK else Color.WHITE)
                    oscClient.configure(ipInput.text.toString(), portInput.text.toString().toIntOrNull() ?: 53000)
                    oscClient.sendInt("/ch/$chNum/mix/solo", if (soloStates[chIdx]) 1 else 0)
                }
            }
            strip.addView(btnSolo, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 80).apply {
                topMargin = 3
            })

            // Vertical Interactive Console Fader
            val fader = VerticalFaderView(this, Color.parseColor("#38BDF8")).apply {
                value = faderValues[chIdx]
                onFaderChanged = { norm ->
                    faderValues[chIdx] = norm
                    dbText.text = String.format("%.1f dB", normToDb(norm))
                    oscClient.configure(ipInput.text.toString(), portInput.text.toString().toIntOrNull() ?: 53000)
                    oscClient.sendFloat("/ch/$chNum/mix/fader", norm)
                }
            }
            strip.addView(fader, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f).apply {
                topMargin = 8
            })

            stripsContainer.addView(strip, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.MATCH_PARENT, 1f).apply {
                rightMargin = 4
            })
        }
    }

    private fun buildMasterStrip(): LinearLayout {
        val strip = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
            setBackgroundColor(Color.parseColor("#1E293B"))
            setPadding(8, 8, 8, 8)
        }
        val lbl = TextView(this).apply {
            text = "MAIN L/R"
            setTextColor(Color.parseColor("#10B981"))
            typeface = Typeface.DEFAULT_BOLD
            gravity = Gravity.CENTER
        }
        strip.addView(lbl)

        val dbText = TextView(this).apply {
            text = "0.0 dB"
            setTextColor(Color.WHITE)
            textSize = 11f
            gravity = Gravity.CENTER
        }
        strip.addView(dbText)

        val spacer = View(this)
        strip.addView(spacer, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 163))

        // Vertical Master Console Fader (Gold accent)
        val mainFader = VerticalFaderView(this, Color.parseColor("#F59E0B")).apply {
            value = 0.8571f // ~0.0 dB
            onFaderChanged = { norm ->
                dbText.text = String.format("%.1f dB", normToDb(norm))
                oscClient.configure(ipInput.text.toString(), portInput.text.toString().toIntOrNull() ?: 53000)
                oscClient.sendFloat("/main/fader", norm)
            }
        }
        strip.addView(mainFader, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f).apply {
            topMargin = 8
        })
        return strip
    }

    override fun onDestroy() {
        oscClient.close()
        super.onDestroy()
    }
}
