
#Requisitos:  pip install pyqt5 pyserial esptool

#Uso:  python flasher_camioneta.py


import os
import re
import sys
import subprocess
import serial
import serial.tools.list_ports
from datetime import datetime

from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QGroupBox, QPushButton, QLabel, QComboBox, QLineEdit,
    QTextEdit, QProgressBar, QFileDialog, QMessageBox,
)
from PyQt5.QtCore import Qt, QThread, pyqtSignal
from PyQt5.QtGui import QFont, QTextCursor, QPixmap, QPainter


CHIP_TARGET  = "esp32c6"   
BAUD_FLASH   = 115200
BAUD_MONITOR = 115200
FLASH_OFFSET = "0x0"       # 0x0 si exportas el .bin fusionado (bootloader+part+app)

# Este script vive en tools/, un nivel adentro del proyecto. _BASE es la
# carpeta de tools (para archivos que viven junto al script, como la
# imagen de fondo); _PROJECT_ROOT es la carpeta del sketch (donde
# Arduino genera build/), un nivel arriba de tools/.
_BASE = os.path.dirname(os.path.abspath(__file__))
_PROJECT_ROOT = os.path.dirname(_BASE)

# Ruta fija por defecto del firmware. Ajustala a donde exportas el
# binario compilado (Arduino IDE: Sketch > Exportar binario compilado).
#
# IMPORTANTE: usar el ".merged.bin", NO el ".ino.bin" a secas.
# El ".ino.bin" es solo la app (offset 0x10000); si se escribe en 0x0
# (como hace este flasher) pisa el bootloader y la placa no arranca.
# El ".merged.bin" ya trae bootloader + particiones + app combinados
# en una sola imagen del tamaño completo de la flash, lista para 0x0.
DEFAULT_BIN = os.path.join(
    _PROJECT_ROOT, "build", "esp32.esp32.esp32c6", "camioneta.ino.merged.bin"
)

# Imagen de fondo semi-transparente. Vive junto a este script (tools/).
BACKGROUND_IMAGE_PATH = os.path.join(_BASE, "reze.jpg")

# <-- AQUI se ajusta la transparencia: 0.0 = invisible, 1.0 = opaca del todo.
BACKGROUND_OPACITY = 0.00

_bin_override = ""


def get_bin_path() -> str:
    return _bin_override if _bin_override else DEFAULT_BIN


def set_bin_override(path: str):
    global _bin_override
    _bin_override = path


def reset_bin_override():
    global _bin_override
    _bin_override = ""


# ============================================================
#  Colores (mismo estilo que ya usan en su otra herramienta)
# ============================================================
C_BASE    = "#1e1e2e"; C_MANTLE  = "#181825"; C_SURFACE = "#313244"
C_OVERLAY = "#45475a"; C_TEXT    = "#cdd6f4"; C_SUBTEXT = "#6c7086"
C_BLUE    = "#89b4fa"; C_GREEN   = "#a6e3a1"; C_RED     = "#f38ba8"
C_YELLOW  = "#f9e2af"; C_TEAL    = "#94e2d5"

# Fuente monoespaciada que exista tanto en Windows como en Linux. Qt
# recorre la lista y usa la primera que encuentre instalada; setStyleHint
# ademas le dice que si ninguna existe, use el monoespaciado del sistema.
def _mono_font(size: int = 10) -> QFont:
    font = QFont()
    font.setFamilies(["Consolas", "DejaVu Sans Mono", "Liberation Mono", "monospace"])
    font.setStyleHint(QFont.Monospace)
    font.setPointSize(size)
    return font


MONO_CSS_FAMILY = "Consolas, 'DejaVu Sans Mono', 'Liberation Mono', monospace"


def _is_usb_serial(port_info) -> bool:
    """True si el puerto parece un adaptador USB-serial real, no un
    puerto virtual/interno (ej. COM1 de placa madre, puertos Bluetooth
    falsos en Windows, etc).

    - Windows: los adaptadores USB reportan VID/PID; los puertos que no
      son USB no lo tienen.
    - Linux: ademas se filtra por el nombre (ttyUSB*/ttyACM*), que es
      la convencion estandar para USB-serial ahi.
    """
    if port_info.vid is not None:
        return True
    if port_info.hwid and "USB" in port_info.hwid.upper():
        return True
    name = os.path.basename(port_info.device)
    if name.startswith("ttyUSB") or name.startswith("ttyACM"):
        return True
    return False


# ============================================================
#  Worker: flasheo con esptool (no bloquea la GUI)
# ============================================================
class FlashWorker(QThread):
    output   = pyqtSignal(str)
    progress = pyqtSignal(int)
    finished = pyqtSignal(bool, str)

    def __init__(self, port: str, bin_path: str, baud: int = BAUD_FLASH):
        super().__init__()
        self.port = port
        self.bin_path = bin_path
        self.baud = baud

    def run(self):
        cmd = [
            sys.executable, "-m", "esptool",
            "--chip", CHIP_TARGET,
            "--port", self.port,
            "--baud", str(self.baud),
            "write_flash", FLASH_OFFSET, self.bin_path,
        ]
        self.output.emit(f"Ejecutando: {' '.join(cmd)}\n")
        try:
            proc = subprocess.Popen(
                cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, bufsize=1,
            )
            for line in proc.stdout:
                line = line.rstrip()
                self.output.emit(line)
                m = re.search(r'\((\d+)\s*%\)', line)
                if m:
                    self.progress.emit(int(m.group(1)))
            proc.wait()
            if proc.returncode == 0:
                self.progress.emit(100)
                self.finished.emit(True, "Flash completado exitosamente.")
            else:
                self.finished.emit(False, f"esptool termino con codigo {proc.returncode}.")
        except FileNotFoundError:
            self.finished.emit(False, "esptool no encontrado.\nInstalar: pip install esptool")
        except Exception as e:
            self.finished.emit(False, str(e))


# ============================================================
#  Worker: terminal serial (monitor en vivo)
# ============================================================
class MonitorWorker(QThread):
    line    = pyqtSignal(str)
    stopped = pyqtSignal()

    def __init__(self, port: str, baud: int = BAUD_MONITOR):
        super().__init__()
        self.port = port
        self.baud = baud
        self._running = True
        self._ser = None

    def run(self):
        try:
            self._ser = serial.Serial(self.port, self.baud, timeout=0.2)
        except Exception as e:
            self.line.emit(f"[error abriendo puerto: {e}]")
            self.stopped.emit()
            return

        buf = b""
        while self._running:
            try:
                chunk = self._ser.read(256)
            except Exception as e:
                self.line.emit(f"[error de lectura: {e}]")
                break
            if chunk:
                buf += chunk
                while b'\n' in buf:
                    raw, buf = buf.split(b'\n', 1)
                    self.line.emit(raw.decode(errors='replace').rstrip('\r'))

        if self._ser and self._ser.is_open:
            self._ser.close()
        self.stopped.emit()

    def send(self, text: str):
        if self._ser and self._ser.is_open:
            self._ser.write((text + "\n").encode())

    def stop(self):
        self._running = False


# ============================================================
#  Ventana principal
# ============================================================
class FlasherWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Camioneta Flasher")
        self.resize(720, 640)
        self.setStyleSheet(f"QMainWindow{{background:{C_BASE};}}")

        self._flash_worker = None
        self._mon_worker = None
        self._flashed_count = 0

        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)
        root.setSpacing(10)
        root.setContentsMargins(14, 14, 14, 14)

        root.addWidget(self._build_bin_section())
        root.addWidget(self._build_flash_section())
        root.addWidget(self._build_log_section(), 1)
        root.addWidget(self._build_terminal_section(), 1)

        self._refresh_ports()
        self._refresh_bin_label()

    # ---------- Secciones ----------

    def _build_bin_section(self):
        box = QGroupBox("Firmware (.bin)")
        box.setStyleSheet(self._gs(C_BLUE))
        lay = QHBoxLayout(box)

        self._lbl_bin = QLabel()
        self._lbl_bin.setWordWrap(True)
        lay.addWidget(self._lbl_bin, 1)

        btn_change = QPushButton("Cambiar .bin")
        btn_change.setFixedWidth(110)
        btn_change.setStyleSheet(self._bs())
        btn_change.clicked.connect(self._elegir_bin)
        lay.addWidget(btn_change)

        btn_reset = QPushButton("Restaurar")
        btn_reset.setFixedWidth(90)
        btn_reset.setStyleSheet(self._bs())
        btn_reset.clicked.connect(self._restaurar_bin)
        lay.addWidget(btn_reset)

        return box

    def _build_flash_section(self):
        box = QGroupBox("Flashear")
        box.setStyleSheet(self._gs(C_TEAL))
        lay = QHBoxLayout(box)
        lay.setSpacing(12)

        lay.addWidget(self._lbl("Puerto:"))
        self._combo_port = QComboBox()
        self._combo_port.setMinimumWidth(120)
        lay.addWidget(self._combo_port)

        btn_ref = QPushButton("Actualizar")
        btn_ref.setFixedWidth(90)
        btn_ref.setStyleSheet(self._bs())
        btn_ref.clicked.connect(self._refresh_ports)
        lay.addWidget(btn_ref)

        lay.addStretch()

        btn_minus = QPushButton("-")
        btn_minus.setFixedWidth(28)
        btn_minus.setStyleSheet(self._bs())
        btn_minus.clicked.connect(self._dec_count)
        lay.addWidget(btn_minus)

        self._lbl_count = QLabel("Flasheados: 0")
        self._lbl_count.setStyleSheet(f"color:{C_TEXT};font-size:12px;font-weight:600;")
        self._lbl_count.setFixedWidth(110)
        self._lbl_count.setAlignment(Qt.AlignCenter)
        lay.addWidget(self._lbl_count)

        btn_plus = QPushButton("+")
        btn_plus.setFixedWidth(28)
        btn_plus.setStyleSheet(self._bs())
        btn_plus.clicked.connect(self._inc_count)
        lay.addWidget(btn_plus)

        self._btn_flash = QPushButton("Flashear")
        self._btn_flash.setFixedHeight(40)
        self._btn_flash.setFixedWidth(150)
        self._btn_flash.setStyleSheet(self._bs_primary())
        self._btn_flash.clicked.connect(self._iniciar_flash)
        lay.addWidget(self._btn_flash)

        return box

    def _build_log_section(self):
        box = QGroupBox("Log de esptool")
        box.setStyleSheet(self._gs(C_BLUE))
        lay = QVBoxLayout(box)
        lay.setSpacing(6)

        self._progress = QProgressBar()
        self._progress.setRange(0, 100)
        self._progress.setValue(0)
        self._progress.setTextVisible(True)
        self._progress.setFixedHeight(18)
        self._progress.setStyleSheet(
            f"QProgressBar{{background:{C_OVERLAY};border-radius:4px;border:none;"
            f"color:{C_TEXT};font-size:11px;}}"
            f"QProgressBar::chunk{{background:{C_BLUE};border-radius:4px;}}"
        )
        lay.addWidget(self._progress)

        self._log = QTextEdit()
        self._log.setReadOnly(True)
        self._log.setFont(_mono_font())
        self._log.setStyleSheet(
            f"QTextEdit{{background:{C_MANTLE};color:{C_GREEN};"
            f"border:1px solid {C_OVERLAY};border-radius:4px;}}"
        )
        lay.addWidget(self._log, 1)

        self._lbl_status = QLabel("Listo.")
        self._lbl_status.setStyleSheet(f"color:{C_SUBTEXT};font-size:11px;")
        lay.addWidget(self._lbl_status)

        return box

    def _build_terminal_section(self):
        box = QGroupBox("Terminal serial (verificar despues de flashear)")
        box.setStyleSheet(self._gs(C_YELLOW))
        lay = QVBoxLayout(box)
        lay.setSpacing(6)

        top = QHBoxLayout()
        self._btn_monitor = QPushButton("Conectar")
        self._btn_monitor.setFixedWidth(110)
        self._btn_monitor.setStyleSheet(self._bs())
        self._btn_monitor.clicked.connect(self._toggle_monitor)
        top.addWidget(self._btn_monitor)
        top.addStretch()
        lay.addLayout(top)

        self._term = QTextEdit()
        self._term.setReadOnly(True)
        self._term.setFont(_mono_font())
        self._term.setStyleSheet(
            f"QTextEdit{{background:{C_MANTLE};color:{C_TEXT};"
            f"border:1px solid {C_OVERLAY};border-radius:4px;}}"
        )
        lay.addWidget(self._term, 1)

        row = QHBoxLayout()
        self._input = QLineEdit()
        self._input.setPlaceholderText("escribe un comando (ej: help) y Enter...")
        self._input.setStyleSheet(
            f"QLineEdit{{background:{C_SURFACE};color:{C_TEXT};"
            f"border:1px solid {C_OVERLAY};border-radius:4px;padding:4px;}}"
        )
        self._input.returnPressed.connect(self._send_line)
        row.addWidget(self._input, 1)

        btn_send = QPushButton("Enviar")
        btn_send.setFixedWidth(80)
        btn_send.setStyleSheet(self._bs())
        btn_send.clicked.connect(self._send_line)
        row.addWidget(btn_send)
        lay.addLayout(row)

        return box

    # ---------- Logica: bin ----------

    def _refresh_bin_label(self):
        path = get_bin_path()
        exists = os.path.exists(path)
        tag = "[override]" if _bin_override else "[por defecto]"
        color = C_GREEN if exists else C_RED
        text = f"{path}  {tag}" if exists else f"{path}  NO ENCONTRADO"
        self._lbl_bin.setText(text)
        self._lbl_bin.setStyleSheet(f"color:{color};font-size:11px;font-family:{MONO_CSS_FAMILY};")

    def _elegir_bin(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Elegir firmware (.bin)", os.path.expanduser("~"),
            "Firmware ESP32 (*.bin)"
        )
        if not path:
            return
        set_bin_override(path)
        self._refresh_bin_label()
        self._log_msg(f"Bin cambiado: {path}", C_YELLOW)

    def _restaurar_bin(self):
        reset_bin_override()
        self._refresh_bin_label()
        self._log_msg("Bin restaurado al de por defecto.", C_GREEN)

    # ---------- Logica: puertos ----------

    def _refresh_ports(self):
        self._combo_port.clear()
        all_ports = serial.tools.list_ports.comports()
        ports = [p for p in all_ports if _is_usb_serial(p)]
        for p in ports:
            self._combo_port.addItem(p.device)
        if not ports:
            self._combo_port.addItem("(sin puertos USB)")

    # ---------- Logica: flasheo ----------

    def _iniciar_flash(self):
        bin_path = get_bin_path()
        if not os.path.exists(bin_path):
            QMessageBox.critical(
                self, "Archivo no encontrado",
                f"No se encontro el firmware:\n{bin_path}\n\n"
                f"Compila y exporta el binario desde Arduino IDE, o usa "
                f"'Cambiar .bin' para elegir otro archivo."
            )
            return

        port = self._combo_port.currentText()
        if not port or port == "(sin puertos)":
            QMessageBox.warning(self, "Sin puerto", "Selecciona un puerto COM.")
            return

        # El puerto no se puede compartir entre el monitor y esptool.
        if self._mon_worker is not None:
            self._log_msg("Cerrando terminal serial para poder flashear...", C_YELLOW)
            self._stop_monitor()

        reply = QMessageBox.question(
            self, "Confirmar flash",
            f"Flashear en {port}?\nArchivo: {os.path.basename(bin_path)}",
            QMessageBox.Yes | QMessageBox.No, QMessageBox.No,
        )
        if reply != QMessageBox.Yes:
            return

        self._btn_flash.setEnabled(False)
        self._progress.setValue(0)
        self._log_msg(f"--- Flash en {port} ---", C_TEAL)

        self._flash_worker = FlashWorker(port, bin_path, BAUD_FLASH)
        self._flash_worker.output.connect(self._on_flash_output)
        self._flash_worker.progress.connect(self._progress.setValue)
        self._flash_worker.finished.connect(self._on_flash_finished)
        self._flash_worker.start()

    def _on_flash_output(self, line: str):
        self._log.append(f'<span style="color:{C_GREEN}">{line}</span>')
        self._log.moveCursor(QTextCursor.End)

    def _on_flash_finished(self, success: bool, msg: str):
        color = C_GREEN if success else C_RED
        self._log_msg(msg, color)
        self._lbl_status.setText(msg)
        self._lbl_status.setStyleSheet(f"color:{color};font-size:11px;")
        self._btn_flash.setEnabled(True)
        # El contador es manual (botones +/-); flashear no lo mueve solo.

    def _inc_count(self):
        self._flashed_count += 1
        self._lbl_count.setText(f"Flasheados: {self._flashed_count}")

    def _dec_count(self):
        if self._flashed_count > 0:
            self._flashed_count -= 1
        self._lbl_count.setText(f"Flasheados: {self._flashed_count}")

    # ---------- Logica: terminal ----------

    def _toggle_monitor(self):
        if self._mon_worker is None:
            self._start_monitor()
        else:
            self._stop_monitor()

    def _start_monitor(self):
        port = self._combo_port.currentText()
        if not port or port == "(sin puertos)":
            QMessageBox.warning(self, "Sin puerto", "Selecciona un puerto COM.")
            return

        self._term_msg(f"--- conectando a {port} ---", C_TEAL)
        self._mon_worker = MonitorWorker(port, BAUD_MONITOR)
        self._mon_worker.line.connect(self._on_monitor_line)
        self._mon_worker.stopped.connect(self._on_monitor_stopped)
        self._mon_worker.start()
        self._btn_monitor.setText("Desconectar")

    def _stop_monitor(self):
        if self._mon_worker:
            self._mon_worker.stop()
            self._mon_worker.wait(1000)
            self._mon_worker = None
        self._btn_monitor.setText("Conectar")

    def _on_monitor_line(self, line: str):
        self._term.append(line)
        self._term.moveCursor(QTextCursor.End)

    def _on_monitor_stopped(self):
        self._term_msg("--- desconectado ---", C_SUBTEXT)

    def _send_line(self):
        text = self._input.text()
        if not text or self._mon_worker is None:
            return
        self._mon_worker.send(text)
        self._term.append(f'<span style="color:{C_BLUE}">&gt; {text}</span>')
        self._term.moveCursor(QTextCursor.End)
        self._input.clear()

    # ---------- Helpers ----------

    def _log_msg(self, msg: str, color: str = None):
        color = color or C_TEXT
        ts = datetime.now().strftime("%H:%M:%S")
        self._log.append(
            f'<span style="color:{C_SUBTEXT}">[{ts}]</span> '
            f'<span style="color:{color}">{msg}</span>'
        )
        self._log.moveCursor(QTextCursor.End)

    def _term_msg(self, msg: str, color: str = None):
        color = color or C_SUBTEXT
        self._term.append(f'<span style="color:{color}">{msg}</span>')
        self._term.moveCursor(QTextCursor.End)

    def _lbl(self, text: str) -> QLabel:
        l = QLabel(text)
        l.setStyleSheet(f"color:{C_TEXT};font-weight:600;")
        return l

    def _gs(self, accent: str) -> str:
        return (
            f"QGroupBox{{border:1px solid {C_OVERLAY};border-radius:8px;"
            f"margin-top:10px;font-weight:bold;color:{accent};padding:10px;}}"
            f"QGroupBox::title{{subcontrol-origin:margin;left:10px;padding:0 6px;}}"
        )

    def _bs(self) -> str:
        return (
            f"QPushButton{{background:{C_SURFACE};color:{C_TEXT};"
            f"border:1px solid {C_OVERLAY};border-radius:5px;"
            f"font-size:12px;padding:4px 10px;}}"
            f"QPushButton:hover{{background:{C_OVERLAY};}}"
            f"QPushButton:disabled{{background:{C_MANTLE};color:{C_OVERLAY};}}"
        )

    def _bs_primary(self) -> str:
        return (
            f"QPushButton{{background:#2563eb;color:white;border:none;"
            f"border-radius:5px;font-size:13px;font-weight:700;}}"
            f"QPushButton:hover{{background:#1d4ed8;}}"
            f"QPushButton:disabled{{background:#1e3a6e;color:#7a9fd6;}}"
        )

    def paintEvent(self, event):
        # Pinta primero el fondo normal (color solido del stylesheet) y
        # despues la foto encima con opacidad reducida, para que quede
        # como fondo semi-transparente detras de los paneles.
        super().paintEvent(event)

        if not os.path.exists(BACKGROUND_IMAGE_PATH):
            return

        pixmap = QPixmap(BACKGROUND_IMAGE_PATH)
        if pixmap.isNull():
            return

        scaled = pixmap.scaled(
            self.size(), Qt.KeepAspectRatioByExpanding, Qt.SmoothTransformation
        )
        x = (self.width()  - scaled.width())  // 2
        y = (self.height() - scaled.height()) // 2

        painter = QPainter(self)
        painter.setOpacity(BACKGROUND_OPACITY)
        painter.drawPixmap(x, y, scaled)
        painter.end()

    def closeEvent(self, event):
        self._stop_monitor()
        if self._flash_worker and self._flash_worker.isRunning():
            self._flash_worker.terminate()
            self._flash_worker.wait()
        event.accept()


def main():
    app = QApplication(sys.argv)
    win = FlasherWindow()
    win.show()
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()
