package com.lilkalimu.hakoniwa

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.text.SpannableStringBuilder
import android.text.Spanned
import android.text.style.ForegroundColorSpan
import android.util.Log
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.io.InputStreamReader
import java.io.OutputStream
import java.util.concurrent.Executors
import kotlin.concurrent.thread

/**
 * Startet und steuert die virtuelle Maschine.
 *
 * QEMU läuft als eigener Prozess (libqemu.so aus der APK). In die VM wird eines von zwei
 * Programmen geladen: "Mein OS" (dein eigenes, beginnt bei null) oder das Beispiel.
 * Was das Programm auf die Textausgabe schreibt, kommt hier als Text an; Tastatureingaben
 * gehen den umgekehrten Weg. Die VM läuft weiter, wenn man den Bildschirm dreht.
 */
object VmController {

    const val TAG = "Hakoniwa"
    const val PROMPT = "hakoniwa> "

    enum class State { STOPPED, RUNNING }
    enum class Kind { VM, SYSTEM, ERROR }

    /** Die Programme, die in der VM laufen können (Dateien in assets/). */
    enum class Os(val asset: String, val label: String, val id: String) {
        MEIN_OS("mein-os.elf", "Mein OS", "mein-os"),
        BEISPIEL("beispiel.elf", "Beispiel", "beispiel");

        companion object {
            fun fromId(id: String?): Os? = entries.firstOrNull { it.id == id }
        }
    }

    interface Listener {
        fun onOutputChanged()
        fun onStateChanged(state: State)
    }

    private enum class ExitReason { NONE, USER_STOP, RESTART, SWITCH }

    private const val PREFS = "hakoniwa"
    private const val PREF_SYSTEM = "system"
    private const val MAX_CHARS = 80_000
    private const val TRIM_TO_CHARS = 60_000

    private val main = Handler(Looper.getMainLooper())
    private val listeners = LinkedHashSet<Listener>()
    private val writer = Executors.newSingleThreadExecutor()
    private val pendingCommands = ArrayDeque<String>()

    private lateinit var appContext: Context
    private var colorVm = 0
    private var colorSystem = 0
    private var colorError = 0

    /** Gesamter Text des Terminals (nur auf dem Haupt-Thread benutzen). */
    val output = SpannableStringBuilder()

    var state = State.STOPPED
        private set

    /** Welches Programm in der VM läuft bzw. beim nächsten Start geladen wird. */
    var system = Os.MEIN_OS
        private set

    /** true, wenn die VM zuletzt über "Stopp" beendet wurde. */
    var stoppedByUser = false
        private set

    private var process: Process? = null
    private var stdin: OutputStream? = null
    private var generation = 0
    private var exitReason = ExitReason.NONE

    fun init(context: Context) {
        if (::appContext.isInitialized) return
        appContext = context.applicationContext
        colorVm = appContext.getColor(R.color.vm_text)
        colorSystem = appContext.getColor(R.color.sys_text)
        colorError = appContext.getColor(R.color.err_text)
        val saved = appContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            .getString(PREF_SYSTEM, null)
        system = Os.fromId(saved) ?: Os.MEIN_OS
    }

    fun addListener(listener: Listener) {
        listeners.add(listener)
    }

    fun removeListener(listener: Listener) {
        listeners.remove(listener)
    }

    /**
     * Wählt das Programm für die VM. Läuft die VM gerade, startet sie sofort mit dem
     * neuen Programm neu; sonst gilt die Auswahl für den nächsten Start.
     */
    fun selectSystem(context: Context, newSystem: Os) {
        init(context)
        if (newSystem == system) return
        system = newSystem
        appContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            .edit().putString(PREF_SYSTEM, newSystem.id).apply()
        Log.i(TAG, "APP: System gewählt: ${newSystem.id}")
        notifyState()
        val p = process
        if (p != null) {
            exitReason = ExitReason.SWITCH
            pendingCommands.clear()
            p.destroy()
        }
    }

    fun start(context: Context) {
        init(context)
        if (process != null) return
        stoppedByUser = false
        exitReason = ExitReason.NONE

        val app = appContext
        val qemu = File(app.applicationInfo.nativeLibraryDir, "libqemu.so")
        if (!qemu.exists()) {
            appendLine("Fehler: QEMU fehlt in dieser App-Version.", Kind.ERROR)
            Log.e(TAG, "APP: libqemu.so fehlt in ${qemu.parent}")
            return
        }

        val kernel = try {
            copyKernel(app, system)
        } catch (e: IOException) {
            appendLine("Fehler: ${system.label} konnte nicht geladen werden (${e.message}).", Kind.ERROR)
            return
        }

        val command = listOf(
            qemu.absolutePath,
            "-M", "virt,gic-version=2",
            "-cpu", "cortex-a72",
            "-smp", "1",
            "-m", "128M",
            "-nodefaults",
            "-display", "none",
            "-serial", "stdio",
            "-kernel", kernel.absolutePath,
        )

        appendLine("${system.label} wird gestartet …", Kind.SYSTEM)
        Log.i(TAG, "APP: Start (${system.id}): ${command.joinToString(" ")}")

        val started = try {
            val builder = ProcessBuilder(command).directory(app.filesDir)
            builder.environment()["TMPDIR"] = app.cacheDir.absolutePath
            builder.environment()["HOME"] = app.filesDir.absolutePath
            builder.start()
        } catch (e: IOException) {
            appendLine("Fehler beim Starten: ${e.message}", Kind.ERROR)
            Log.e(TAG, "APP: Start fehlgeschlagen", e)
            return
        }

        val gen = ++generation
        process = started
        stdin = started.outputStream
        setState(State.RUNNING)

        val outReader = pump(started.inputStream, gen, Kind.VM, "VM")
        val errReader = pump(started.errorStream, gen, Kind.ERROR, "QEMU")
        thread(name = "vm-wait", isDaemon = true) {
            val code = try {
                started.waitFor()
            } catch (e: InterruptedException) {
                -1
            }
            // Erst die restliche Ausgabe lesen, dann das Ende melden
            outReader.join(2000)
            errReader.join(2000)
            main.post { onExited(gen, code) }
        }
    }

    fun stop() {
        val p = process ?: return
        exitReason = ExitReason.USER_STOP
        stoppedByUser = true
        p.destroy()
    }

    fun restart(context: Context) {
        init(context)
        val p = process
        if (p == null) {
            start(context)
            return
        }
        exitReason = ExitReason.RESTART
        p.destroy()
    }

    /** Schickt eine Zeile an die VM, als hätte man sie getippt und Enter gedrückt. */
    fun sendLine(text: String): Boolean {
        val out = stdin ?: return false
        val bytes = (text + "\r").toByteArray(Charsets.UTF_8)
        writer.execute {
            try {
                out.write(bytes)
                out.flush()
            } catch (e: IOException) {
                Log.w(TAG, "APP: Eingabe nicht gesendet: ${e.message}")
            }
        }
        return true
    }

    /** Befehle für automatische Tests: werden nacheinander gesendet, sobald die VM bereit ist. */
    fun queueCommands(commands: List<String>) {
        pendingCommands.addAll(commands)
        if (state == State.RUNNING && endsWithPrompt()) {
            sendNextQueuedCommand()
        }
    }

    fun clearOutput() {
        output.clear()
        notifyOutput()
    }

    // -----------------------------------------------------------------------

    private fun copyKernel(context: Context, which: Os): File {
        val target = File(context.filesDir, which.asset)
        context.assets.open(which.asset).use { input ->
            target.outputStream().use { output -> input.copyTo(output) }
        }
        return target
    }

    private fun pump(stream: InputStream, gen: Int, kind: Kind, logPrefix: String): Thread =
        thread(name = "vm-$logPrefix", isDaemon = true) {
            val reader = InputStreamReader(stream, Charsets.UTF_8)
            val buffer = CharArray(4096)
            val line = StringBuilder()
            while (true) {
                val count = try {
                    reader.read(buffer)
                } catch (e: IOException) {
                    -1
                }
                if (count < 0) break
                val chunk = String(buffer, 0, count)
                for (c in chunk) {
                    when (c) {
                        '\n' -> {
                            Log.i(TAG, "$logPrefix: $line")
                            line.setLength(0)
                        }
                        '\r' -> Unit
                        else -> line.append(c)
                    }
                }
                // Angefangene Zeile ohne Zeilenende (z. B. ein einzelner Buchstabe) auch protokollieren
                if (line.isNotEmpty()) Log.v(TAG, "$logPrefix…: $line")
                main.post { appendFromVm(chunk, kind, gen) }
            }
            if (line.isNotEmpty()) Log.i(TAG, "$logPrefix: $line")
        }

    private fun onExited(gen: Int, code: Int) {
        if (gen != generation) return
        process = null
        stdin = null
        val reason = exitReason
        exitReason = ExitReason.NONE
        Log.i(TAG, "APP: VM beendet (Code $code, Grund $reason)")
        setState(State.STOPPED)

        when (reason) {
            ExitReason.RESTART -> {
                appendLine("", Kind.SYSTEM)
                appendLine("— Neustart —", Kind.SYSTEM)
                start(appContext)
            }
            ExitReason.SWITCH -> {
                output.clear()
                start(appContext)
            }
            ExitReason.USER_STOP -> {
                pendingCommands.clear()
                appendLine("VM gestoppt.", Kind.SYSTEM)
            }
            ExitReason.NONE -> {
                pendingCommands.clear()
                if (code == 0) {
                    appendLine("VM ausgeschaltet. Tippe auf Start für einen neuen Start.", Kind.SYSTEM)
                } else {
                    appendLine("VM beendet (Code $code).", Kind.ERROR)
                }
            }
        }
    }

    private fun appendFromVm(chunk: String, kind: Kind, gen: Int) {
        if (gen != generation) return
        val start = output.length
        for (c in chunk) {
            when {
                c == '\r' -> Unit
                c == '\b' -> {
                    val len = output.length
                    if (len > start && output[len - 1] != '\n') output.delete(len - 1, len)
                }
                c == '\n' || c == '\t' || c >= ' ' -> output.append(c)
                else -> Unit
            }
        }
        colorize(start, kind)
        trim()
        notifyOutput()
        if (kind == Kind.VM && pendingCommands.isNotEmpty() && endsWithPrompt()) {
            sendNextQueuedCommand()
        }
    }

    private fun appendLine(text: String, kind: Kind) {
        if (output.isNotEmpty() && output[output.length - 1] != '\n') output.append('\n')
        val start = output.length
        output.append(text).append('\n')
        colorize(start, kind)
        trim()
        notifyOutput()
    }

    private fun colorize(start: Int, kind: Kind) {
        if (output.length <= start) return
        val color = when (kind) {
            Kind.VM -> colorVm
            Kind.SYSTEM -> colorSystem
            Kind.ERROR -> colorError
        }
        output.setSpan(ForegroundColorSpan(color), start, output.length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
    }

    private fun trim() {
        if (output.length <= MAX_CHARS) return
        var cut = output.length - TRIM_TO_CHARS
        while (cut < output.length && output[cut - 1] != '\n') cut++
        output.delete(0, cut)
    }

    private fun endsWithPrompt(): Boolean {
        val len = output.length
        if (len < PROMPT.length) return false
        for (i in PROMPT.indices) {
            if (output[len - PROMPT.length + i] != PROMPT[i]) return false
        }
        return true
    }

    private fun sendNextQueuedCommand() {
        val next = pendingCommands.removeFirstOrNull() ?: return
        Log.i(TAG, "APP: Testbefehl: $next")
        sendLine(next)
    }

    private fun setState(newState: State) {
        state = newState
        notifyState()
    }

    private fun notifyState() {
        for (listener in listeners.toList()) listener.onStateChanged(state)
    }

    private fun notifyOutput() {
        for (listener in listeners.toList()) listener.onOutputChanged()
    }
}
