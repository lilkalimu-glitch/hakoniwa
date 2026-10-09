package com.lilkalimu.hakoniwa

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.util.TypedValue
import android.view.KeyEvent
import android.view.View
import android.view.WindowInsets
import android.view.inputmethod.EditorInfo
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast

class MainActivity : Activity(), VmController.Listener {

    companion object {
        /** Für automatische Tests: Befehle, getrennt mit ";" */
        const val EXTRA_TEST_COMMANDS = "hakoniwa.test.commands"

        private val QUICK_COMMANDS = listOf(
            "hilfe", "info", "zeit", "speicher", "rechne 6 * 7", "hallo", "absturz", "neustart", "aus",
        )
    }

    private lateinit var terminal: TextView
    private lateinit var scroll: ScrollView
    private lateinit var input: EditText
    private lateinit var status: TextView
    private lateinit var startButton: Button
    private lateinit var stopButton: Button
    private lateinit var restartButton: Button
    private var redrawPending = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.setDecorFitsSystemWindows(false)
        setContentView(R.layout.activity_main)

        terminal = findViewById(R.id.terminal)
        scroll = findViewById(R.id.scroll)
        input = findViewById(R.id.input)
        status = findViewById(R.id.status)
        startButton = findViewById(R.id.start)
        stopButton = findViewById(R.id.stop)
        restartButton = findViewById(R.id.restart)

        applyInsets(findViewById(R.id.root))
        createQuickCommands(findViewById(R.id.chips))

        findViewById<Button>(R.id.send).setOnClickListener { sendInput() }
        input.setOnEditorActionListener { _, actionId, event ->
            val enter = event != null && event.keyCode == KeyEvent.KEYCODE_ENTER &&
                event.action == KeyEvent.ACTION_DOWN
            if (actionId == EditorInfo.IME_ACTION_SEND || actionId == EditorInfo.IME_ACTION_DONE || enter) {
                sendInput()
                true
            } else {
                false
            }
        }
        startButton.setOnClickListener { VmController.start(this) }
        stopButton.setOnClickListener { VmController.stop() }
        restartButton.setOnClickListener { VmController.restart(this) }

        VmController.init(this)
        handleIntent(intent, firstStart = savedInstanceState == null)
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        handleIntent(intent, firstStart = false)
    }

    override fun onStart() {
        super.onStart()
        VmController.addListener(this)
        redraw()
        onStateChanged(VmController.state)
    }

    override fun onStop() {
        VmController.removeListener(this)
        super.onStop()
    }

    // --- VmController.Listener ---------------------------------------------

    override fun onOutputChanged() {
        if (redrawPending) return
        redrawPending = true
        terminal.postOnAnimation {
            redrawPending = false
            redraw()
        }
    }

    override fun onStateChanged(state: VmController.State) {
        val running = state == VmController.State.RUNNING
        status.setText(if (running) R.string.status_on else R.string.status_off)
        status.setTextColor(getColor(if (running) R.color.mint else R.color.text_dim))
        status.setBackgroundResource(if (running) R.drawable.bg_status_on else R.drawable.bg_status_off)
        setEnabled(startButton, !running)
        setEnabled(stopButton, running)
    }

    // -----------------------------------------------------------------------

    private fun handleIntent(intent: Intent?, firstStart: Boolean) {
        val testCommands = intent?.getStringExtra(EXTRA_TEST_COMMANDS)
        if (testCommands != null) {
            VmController.queueCommands(testCommands.split(';').map { it.trim() }.filter { it.isNotEmpty() })
        }
        // Beim Öffnen der App startet die VM automatisch.
        val shouldStart = firstStart || testCommands != null
        if (shouldStart && VmController.state == VmController.State.STOPPED && !VmController.stoppedByUser) {
            VmController.start(this)
        }
    }

    private fun sendInput() {
        if (VmController.state != VmController.State.RUNNING) {
            Toast.makeText(this, R.string.vm_not_running, Toast.LENGTH_SHORT).show()
            return
        }
        VmController.sendLine(input.text.toString())
        input.setText("")
    }

    private fun sendQuickCommand(command: String) {
        if (VmController.state != VmController.State.RUNNING) {
            Toast.makeText(this, R.string.vm_not_running, Toast.LENGTH_SHORT).show()
            return
        }
        VmController.sendLine(command)
    }

    private fun redraw() {
        terminal.text = VmController.output
        scroll.post { scroll.scrollTo(0, terminal.bottom) }
    }

    private fun setEnabled(button: Button, enabled: Boolean) {
        button.isEnabled = enabled
        button.alpha = if (enabled) 1f else 0.35f
    }

    private fun createQuickCommands(container: LinearLayout) {
        val padH = dp(12)
        val padV = dp(7)
        for (command in QUICK_COMMANDS) {
            container.addView(chip(command, R.color.text) { sendQuickCommand(command) }.apply {
                setPadding(padH, padV, padH, padV)
            })
        }
        container.addView(chip(getString(R.string.btn_clear), R.color.text_dim) {
            VmController.clearOutput()
        }.apply { setPadding(padH, padV, padH, padV) })
    }

    private fun chip(text: String, colorRes: Int, onClick: () -> Unit): TextView =
        TextView(this).apply {
            this.text = text
            setTextColor(getColor(colorRes))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
            typeface = android.graphics.Typeface.MONOSPACE
            setBackgroundResource(R.drawable.bg_chip)
            isClickable = true
            isFocusable = false
            setOnClickListener { onClick() }
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT,
            ).apply { marginEnd = dp(8) }
        }

    private fun applyInsets(root: View) {
        val base = dp(16)
        root.setOnApplyWindowInsetsListener { view, insets ->
            val bars = insets.getInsets(WindowInsets.Type.systemBars() or WindowInsets.Type.displayCutout())
            val ime = insets.getInsets(WindowInsets.Type.ime())
            view.setPadding(
                base + bars.left,
                base + bars.top,
                base + bars.right,
                base + maxOf(bars.bottom, ime.bottom),
            )
            scroll.post { scroll.scrollTo(0, terminal.bottom) }
            WindowInsets.CONSUMED
        }
    }

    private fun dp(value: Int): Int =
        TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value.toFloat(), resources.displayMetrics).toInt()
}
