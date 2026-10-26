#!/usr/bin/env python3
"""
Radio Test Matrix Orchestrator

This script orchestrates automated LoRa radio testing between two ESP32 nodes:
- Broadcaster: /dev/esp32-alpha
- Receiver: /dev/esp32-pits

It iterates through all 2016 test matrix configurations, sends JSON commands
to both nodes via serial, collects RSSI/SNR/timestamp data, and saves results
to CSV files.
"""

import json
import csv
import time
import serial
import threading
import queue
import signal
import sys
import shutil
from datetime import datetime
from itertools import product
from pathlib import Path
from typing import Callable, Dict, List, Optional, Tuple
from dataclasses import dataclass, field


# ============================================================
# CONFIGURATION
# ============================================================

BROADCASTER_PORT = "/dev/esp32-alpha"
RECEIVER_PORT = "/dev/esp32-pits"
BAUD_RATE = 115200
SERIAL_TIMEOUT = 2.0

# Test timing - optimized for ~3 hour total runtime (2016 configs)
BROADCAST_INTERVAL = 5.0  # seconds between broadcasts (not used in single-shot mode)
RECEIVE_WINDOW = 3.0      # seconds to wait for reception after broadcast
CONFIG_APPLY_DELAY = 1.0  # seconds to wait after sending config

# Resolve the project root from the current working directory when launched
# from the repository, otherwise from the script location.
_CWD = Path.cwd().resolve()
_SCRIPT_DIR = Path(__file__).resolve().parent
if (_CWD / "data").is_dir():
    PROJECT_ROOT = _CWD
elif (_SCRIPT_DIR.parent / "data").is_dir():
    PROJECT_ROOT = _SCRIPT_DIR.parent
else:
    PROJECT_ROOT = _SCRIPT_DIR

# Output files
OUTPUT_DIR = PROJECT_ROOT / "test_results"
BROADCAST_JSON = OUTPUT_DIR / "broadcast_log.json"
RECEIVE_JSON = OUTPUT_DIR / "receive_log.json"
SUMMARY_JSON = OUTPUT_DIR / "test_summary.json"

# Test matrix file
TEST_MATRIX_FILE = PROJECT_ROOT / "data" / "rce_test_matrix.json"


# ============================================================
# DATA CLASSES
# ============================================================

@dataclass
class TestConfig:
    """Single test configuration from the matrix."""
    frequency_mhz: float
    spreading_factor: int
    bandwidth_khz: int
    coding_rate: str  # "4/5", "4/6", etc.
    tx_power_dbm: int
    
    def to_broadcast_json(self) -> dict:
        return {
            "ROLE": "BROADCAST",
            "CONFIGURATION": {
                "FREQUENCY-MHZ": self.frequency_mhz,
                "SPREADING-FACTOR": self.spreading_factor,
                "BANDWIDTH-KHZ": self.bandwidth_khz,
                "CODING-RATE": self.coding_rate,
                "TX-POWER-DBM": self.tx_power_dbm
            }
        }
    
    def to_receive_json(self) -> dict:
        return {
            "ROLE": "RECEIVE",
            "CONFIGURATION": {
                "FREQUENCY-MHZ": self.frequency_mhz,
                "SPREADING-FACTOR": self.spreading_factor,
                "BANDWIDTH-KHZ": self.bandwidth_khz,
                "CODING-RATE": self.coding_rate
            }
        }
    
    def config_id(self) -> str:
        return f"F{self.frequency_mhz}_SF{self.spreading_factor}_BW{self.bandwidth_khz}_CR{self.coding_rate.replace('/', '')}_P{self.tx_power_dbm}"


@dataclass
class BroadcastRecord:
    """Record from broadcaster node."""
    timestamp: str
    config_id: str
    sequence: int
    frequency_mhz: float
    spreading_factor: int
    bandwidth_khz: int
    coding_rate: str
    tx_power_dbm: int
    packet_json: str
    raw_line: str


@dataclass
class ReceiveRecord:
    """Record from receiver node."""
    timestamp: str
    config_id: str
    sequence: int
    frequency_mhz: float
    spreading_factor: int
    bandwidth_khz: int
    coding_rate: str
    rssi_dbm: float
    snr_db: float
    reception_timestamp: str
    packet_json: str
    raw_line: str


@dataclass
class TestSummary:
    """Summary of a single test configuration."""
    config_id: str
    frequency_mhz: float
    spreading_factor: int
    bandwidth_khz: int
    coding_rate: str
    tx_power_dbm: int
    broadcasts_sent: int
    packets_received: int
    packet_loss_percent: float
    avg_rssi_dbm: float
    avg_snr_db: float
    min_rssi_dbm: float
    max_rssi_dbm: float
    min_snr_db: float
    max_snr_db: float
    test_duration_sec: float
    status: str  # "COMPLETE", "PARTIAL", "FAILED"


# ============================================================
# SERIAL READER THREAD
# ============================================================

class SerialReader:
    """Threaded serial reader that captures lines from a serial port."""
    
    def __init__(self, port: str, baud: int, timeout: float, name: str, logger: Optional[Callable[[str], None]] = None):
        self.port = port
        self.baud = baud
        self.timeout = timeout
        self.name = name
        self.logger = logger or (lambda msg: print(msg))
        self.serial_conn: Optional[serial.Serial] = None
        self.line_queue: queue.Queue = queue.Queue()
        self.running = False
        self.thread: Optional[threading.Thread] = None
        self.buffer = ""
    
    def _log(self, message: str):
        """Log a message using the provided logger."""
        self.logger(f"[{self.name}] {message}")
    
    def start(self) -> bool:
        """Open serial port and start reader thread."""
        try:
            self.serial_conn = serial.Serial(
                port=self.port,
                baudrate=self.baud,
                timeout=self.timeout
            )
            # Wait for connection to stabilize
            time.sleep(0.5)
            self.serial_conn.reset_input_buffer()
            self.running = True
            self.thread = threading.Thread(target=self._read_loop, daemon=True)
            self.thread.start()
            self._log(f"Connected to {self.port}")
            return True
        except Exception as e:
            self._log(f"Failed to connect to {self.port}: {e}")
            return False
    
    def stop(self):
        """Stop reader thread and close serial port."""
        self.running = False
        if self.thread:
            self.thread.join(timeout=2.0)
        if self.serial_conn and self.serial_conn.is_open:
            self.serial_conn.close()
        self._log("Disconnected")
    
    def _read_loop(self):
        """Background thread: read lines from serial port."""
        while self.running and self.serial_conn and self.serial_conn.is_open:
            try:
                if self.serial_conn.in_waiting:
                    data = self.serial_conn.read(self.serial_conn.in_waiting).decode('utf-8', errors='ignore')
                    self.buffer += data
                    
                    # Process complete lines
                    while '\n' in self.buffer:
                        line, self.buffer = self.buffer.split('\n', 1)
                        line = line.strip('\r')
                        # Keep empty lines too; they may delimit packet output.
                        self.line_queue.put((time.time(), line))
                else:
                    time.sleep(0.01)
            except Exception as e:
                if self.running:
                    self._log(f"Read error: {e}")
                break
    
    def send_command(self, command: str) -> bool:
        """Send a command to the serial port."""
        if not self.serial_conn or not self.serial_conn.is_open:
            return False
        try:
            self.serial_conn.write((command + '\n').encode('utf-8'))
            self.serial_conn.flush()
            return True
        except Exception as e:
            self._log(f"Send error: {e}")
            return False
    
    def send_json(self, json_obj: dict) -> bool:
        """Send a JSON object as a single line."""
        json_str = json.dumps(json_obj, separators=(',', ':'))
        return self.send_command(json_str)
    
    def get_lines(self, timeout: float = 0.1) -> List[Tuple[float, str]]:
        """Get all available lines from the queue."""
        lines = []
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                line = self.line_queue.get(timeout=max(0.01, deadline - time.time()))
                lines.append(line)
            except queue.Empty:
                break
        return lines
    
    def wait_for_pattern(self, pattern: str, timeout: float) -> Optional[str]:
        """Wait for a line containing pattern."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            lines = self.get_lines(0.1)
            for _, line in lines:
                if pattern in line:
                    return line
        return None
    
    def clear_queue(self):
        """Clear the line queue."""
        while not self.line_queue.empty():
            try:
                self.line_queue.get_nowait()
            except queue.Empty:
                break


# ============================================================
# PARSERS
# ============================================================

class BroadcastParser:
    """Parse broadcaster output without depending on exact spacing/punctuation."""

    def __init__(self, config: TestConfig, logger: Optional[Callable[[str], None]] = None):
        self.config = config
        self.logger = logger or (lambda msg: None)
        self.current_seq = None
        self.json_buffer = ""
        self.waiting_for_packet = False

    def feed(self, line: str) -> Optional[BroadcastRecord]:
        import re
        self.logger(f"[BroadcastParser] Processing: {line[:160]}")

        match = re.search(r"^\s*SEQ-NO\s*[:=]\s*(\d+)\s*$", line, re.IGNORECASE)
        if match:
            self.current_seq = int(match.group(1))
            self.waiting_for_packet = True
            self.json_buffer = ""
            self.logger(f"[BroadcastParser] Found SEQ-NO: {self.current_seq}")
            return None

        if self.waiting_for_packet:
            if "packet" in line.lower() and ":" in line:
                line = line.split(":", 1)[1].strip()
            if "{" in line or self.json_buffer:
                self.json_buffer += ("\n" if self.json_buffer else "") + line.strip()
                try:
                    start = self.json_buffer.index("{")
                    end = self.json_buffer.rindex("}") + 1
                    parsed = json.loads(self.json_buffer[start:end])
                    packet_json = json.dumps(parsed, separators=(",", ":"), ensure_ascii=False)
                    seq = self.current_seq if self.current_seq is not None else parsed.get("SEQ-NO", 0)
                    record = BroadcastRecord(
                        timestamp=datetime.utcnow().isoformat() + "Z",
                        config_id=self.config.config_id(),
                        sequence=seq,
                        frequency_mhz=self.config.frequency_mhz,
                        spreading_factor=self.config.spreading_factor,
                        bandwidth_khz=self.config.bandwidth_khz,
                        coding_rate=self.config.coding_rate,
                        tx_power_dbm=self.config.tx_power_dbm,
                        packet_json=packet_json,
                        raw_line=line,
                    )
                    self.current_seq = None
                    self.json_buffer = ""
                    self.waiting_for_packet = False
                    self.logger(f"[BroadcastParser] Created record for seq {record.sequence}")
                    return record
                except (ValueError, json.JSONDecodeError):
                    self.logger("[BroadcastParser] Waiting for remaining JSON lines")
        return None


class ReceiveParser:
    """Parse receiver output whether it prints raw JSON or decoded KEY : VALUE lines."""

    def __init__(self, config: TestConfig, logger: Optional[Callable[[str], None]] = None):
        self.config = config
        self.logger = logger or (lambda msg: None)
        self.in_packet = False
        self.packet_lines = []
        self.packet_fields = {}
        self.rssi = None
        self.snr = None
        self.reception_ts = None
        self.packet_json = None

    def _reset(self):
        self.in_packet = False
        self.packet_lines = []
        self.packet_fields = {}
        self.rssi = None
        self.snr = None
        self.reception_ts = None
        self.packet_json = None

    def _make_record(self) -> Optional[ReceiveRecord]:
        if not self.in_packet or self.rssi is None or self.snr is None:
            return None

        payload = self.packet_fields.copy()
        seq = payload.get("SEQ-NO", 0)
        try:
            seq = int(seq)
        except (TypeError, ValueError):
            seq = 0

        packet_json = self.packet_json
        if not packet_json and payload:
            packet_json = json.dumps(payload, separators=(",", ":"), ensure_ascii=False)
        if not packet_json:
            self.logger("[ReceiveParser] Not writing record: no packet fields/JSON captured")
            return None

        record = ReceiveRecord(
            timestamp=datetime.utcnow().isoformat() + "Z",
            config_id=self.config.config_id(),
            sequence=seq,
            frequency_mhz=self.config.frequency_mhz,
            spreading_factor=self.config.spreading_factor,
            bandwidth_khz=self.config.bandwidth_khz,
            coding_rate=self.config.coding_rate,
            rssi_dbm=self.rssi,
            snr_db=self.snr,
            reception_timestamp=self.reception_ts or "",
            packet_json=packet_json,
            raw_line="\n".join(self.packet_lines),
        )
        self.logger(f"[ReceiveParser] Created record for seq {seq}, RSSI={self.rssi}, SNR={self.snr}")
        self._reset()
        return record

    def feed(self, line: str) -> Optional[ReceiveRecord]:
        import re
        self.logger(f"[ReceiveParser] Processing: {line[:160]}")
        upper_line = line.upper()
        is_header = "RECEIVED JSON PACKET" in upper_line or "TEST RECEIVED PACKET" in upper_line

        if is_header:
            previous = self._make_record()
            self._reset()
            self.in_packet = True
            self.packet_lines = [line]
            self.logger("[ReceiveParser] Started new packet capture")
            return previous

        if not self.in_packet:
            return None

        self.packet_lines.append(line)

        if "{" in line or "}" in line:
            full_text = "\\n".join(self.packet_lines)
            try:
                start = full_text.index("{")
                end = full_text.rindex("}") + 1
                parsed = json.loads(full_text[start:end])
                self.packet_json = json.dumps(parsed, separators=(",", ":"), ensure_ascii=False)
                if isinstance(parsed, dict):
                    self.packet_fields.update(parsed)
                self.logger(f"[ReceiveParser] Parsed JSON, length: {len(self.packet_json)}")
            except (ValueError, json.JSONDecodeError):
                pass

        if ":" in line:
            key, value = line.split(":", 1)
            key = key.strip().strip("[]")
            value = value.strip()
            upper_key = key.upper()

            if upper_key == "RSSI":
                match = re.search(r"[-+]?\d+(?:\.\d+)?", value)
                if match:
                    self.rssi = float(match.group(0))
                    self.logger(f"[ReceiveParser] Found RSSI: {self.rssi}")
            elif upper_key == "SNR":
                match = re.search(r"[-+]?\d+(?:\.\d+)?", value)
                if match:
                    self.snr = float(match.group(0))
                    self.logger(f"[ReceiveParser] Found SNR: {self.snr}")
            elif upper_key == "RECEPTION-TIMESTAMP":
                self.reception_ts = value
                self.logger(f"[ReceiveParser] Found RECEPTION-TIMESTAMP: {self.reception_ts}")
            elif key and upper_key not in ("RECEPTION-TIMESTAMP",):
                # Keep decoded payload fields printed by the firmware.
                if upper_key not in ("RSSI", "SNR"):
                    self.packet_fields[key] = value

        # Finalize at SNR; no blank separator is required.
        if self.rssi is not None and self.snr is not None:
            return self._make_record()
        return None


# MAIN ORCHESTRATOR
# ============================================================

class RadioTestOrchestrator:
    """Main orchestrator for radio test matrix execution."""
    
    def __init__(self):
        self.broadcaster = SerialReader(BROADCASTER_PORT, BAUD_RATE, SERIAL_TIMEOUT, "BROADCASTER", logger=self._log)
        self.receiver = SerialReader(RECEIVER_PORT, BAUD_RATE, SERIAL_TIMEOUT, "RECEIVER", logger=self._log)
        self.configs: List[TestConfig] = []
        self.broadcast_records: List[BroadcastRecord] = []
        self.receive_records: List[ReceiveRecord] = []
        self.summaries: List[TestSummary] = []
        self.running = True
        self.current_config_idx = 0
        
        # Progress tracking
        self.start_time: Optional[float] = None
        self.config_times: List[float] = []  # Time taken for each config
        
        # JSON file handles for active writing
        self._broadcast_json_file = None
        self._receive_json_file = None
        
        # Debug log file handles for live debugging
        self._broadcast_debug_file = None
        self._receive_debug_file = None
        
        # Setup signal handlers
        signal.signal(signal.SIGINT, self._signal_handler)
        signal.signal(signal.SIGTERM, self._signal_handler)
        
        # TUI state
        self._tui_initialized = False
        self._log_lines: List[str] = []
        self._max_log_lines = 0
        self._progress_line = ""
        self._terminal_height = 24
        self._terminal_width = 80
    
    def _signal_handler(self, signum, frame):
        self._cleanup_tui()
        # Print to original screen after TUI cleanup
        print("\n[Orchestrator] Shutdown signal received...")
        self.running = False
    
    def _format_time(self, seconds: float) -> str:
        """Format seconds into human-readable time string."""
        if seconds < 60:
            return f"{seconds:.0f}s"
        elif seconds < 3600:
            return f"{seconds/60:.1f}m"
        else:
            hours = seconds / 3600
            return f"{hours:.1f}h"
    
    def _get_terminal_size(self) -> Tuple[int, int]:
        """Get terminal width and height."""
        try:
            size = shutil.get_terminal_size()
            return size.columns, size.lines
        except:
            return 80, 24
    
    def _init_tui(self):
        """Initialize the TUI - hide cursor, clear screen, reserve bottom line for progress."""
        self._terminal_width, self._terminal_height = self._get_terminal_size()
        self._max_log_lines = self._terminal_height - 3  # Reserve 3 lines: progress, separator, status
        
        # Hide cursor, clear screen, enable alternate screen buffer
        print("\033[?1049h", end="")  # Enter alternate screen buffer
        print("\033[?25l", end="")    # Hide cursor
        print("\033[2J", end="")      # Clear screen
        print("\033[H", end="")       # Move to home
        self._tui_initialized = True
        
        # Draw initial progress bar
        self._update_progress_display(0, len(self.configs) if self.configs else 1, "")
    
    def _cleanup_tui(self):
        """Clean up TUI - restore cursor, exit alternate screen."""
        if self._tui_initialized:
            print("\033[?25h", end="")    # Show cursor
            print("\033[?1049l", end="")  # Exit alternate screen buffer
            self._tui_initialized = False
    
    def _log(self, message: str):
        """Add a log message to the scrollable log area."""
        if not self._tui_initialized:
            print(message)
            return
        
        timestamp = datetime.now().strftime("%H:%M:%S")
        formatted = f"[{timestamp}] {message}"
        
        self._log_lines.append(formatted)
        
        # Keep only the last max_log_lines
        if len(self._log_lines) > self._max_log_lines:
            self._log_lines = self._log_lines[-self._max_log_lines:]
        
        # Redraw log area
        self._redraw_log_area()
    
    def _redraw_log_area(self):
        """Redraw the log area above the progress bar."""
        if not self._tui_initialized:
            return
        
        # Save cursor position
        print("\033[s", end="")
        
        # Move to top of log area (line 1)
        print("\033[1;1H", end="")
        
        # Clear log area (from line 1 to max_log_lines)
        for i in range(self._max_log_lines):
            print("\033[2K", end="")  # Clear line
            if i < self._max_log_lines - 1:
                print("\033[1B", end="")  # Move down
        
        # Move back to top
        print("\033[1;1H", end="")
        
        # Print log lines (most recent at bottom of log area)
        start_idx = max(0, len(self._log_lines) - self._max_log_lines)
        for line in self._log_lines[start_idx:]:
            # Truncate line to terminal width
            truncated = line[:self._terminal_width - 1]
            print(truncated)
        
        # Restore cursor position
        print("\033[u", end="")
        sys.stdout.flush()
    
    def _update_progress_display(self, current: int, total: int, current_config_id: str = ""):
        """Update the fixed progress bar at the bottom of the terminal."""
        if not self._tui_initialized:
            return
        
        if self.start_time is None:
            return
        
        elapsed = time.time() - self.start_time
        
        # Calculate progress
        progress = current / total if total > 0 else 0
        percent = progress * 100
        
        # Calculate ETA
        if current > 0 and len(self.config_times) > 0:
            avg_time_per_config = sum(self.config_times) / len(self.config_times)
            remaining_configs = total - current
            eta_seconds = avg_time_per_config * remaining_configs
            eta_str = self._format_time(eta_seconds)
        else:
            eta_str = "calculating..."
        
        # Build progress bar
        term_width = self._terminal_width
        prefix = f" [{current}/{total}] "
        suffix = f" | {percent:.1f}% | ETA: {eta_str} | Elapsed: {self._format_time(elapsed)}"
        if current_config_id:
            suffix += f" | {current_config_id}"
        
        bar_width = max(10, term_width - len(prefix) - len(suffix) - 2)
        filled = int(bar_width * progress)
        bar = "█" * filled + "░" * (bar_width - filled)
        
        progress_line = f"{prefix}[{bar}]{suffix}"
        
        # Save cursor position
        print("\033[s", end="")
        
        # Move to progress bar line (terminal_height - 2)
        progress_line_num = self._terminal_height - 2
        print(f"\033[{progress_line_num};1H", end="")
        
        # Draw separator line above progress
        print("\033[2K", end="")  # Clear line
        print("─" * term_width)
        
        # Draw progress bar
        print("\033[2K", end="")  # Clear line
        print(progress_line)
        
        # Draw status line at bottom
        print("\033[2K", end="")  # Clear line
        status = "RUNNING" if self.running else "STOPPED"
        print(f" Status: {status} | Press Ctrl+C to stop")
        
        # Restore cursor position
        print("\033[u", end="")
        sys.stdout.flush()
    
    def _record_config_time(self, duration: float):
        """Record the time taken for a configuration."""
        self.config_times.append(duration)
        # Keep only last 50 times for rolling average
        if len(self.config_times) > 50:
            self.config_times = self.config_times[-50:]
    
    def load_test_matrix(self) -> bool:
        """Load test matrix from JSON file."""
        try:
            with open(TEST_MATRIX_FILE, 'r') as f:
                data = json.load(f)
            
            # Generate all combinations
            for freq, sf, bw, cr, tx in product(
                data["frequency"],
                data["spreading_factor"],
                data["bandwidth"],
                data["coding_rate"],
                data["tx_power"]
            ):
                self.configs.append(TestConfig(
                    frequency_mhz=freq,
                    spreading_factor=sf,
                    bandwidth_khz=bw,
                    coding_rate=cr,
                    tx_power_dbm=tx
                ))
            
            print(f"[Orchestrator] Loaded {len(self.configs)} test configurations")
            return True
        except Exception as e:
            print(f"[Orchestrator] Failed to load test matrix: {e}")
            return False
    
    def connect_nodes(self) -> bool:
        """Connect to both serial ports and initialize CSV files."""
        self._log("[Orchestrator] Connecting to nodes...")
        
        if not self.broadcaster.start():
            return False
        if not self.receiver.start():
            self.broadcaster.stop()
            return False
        
        # Wait for nodes to be ready
        time.sleep(1.0)
        
        # Clear any initial output
        self.broadcaster.clear_queue()
        self.receiver.clear_queue()
        
        # Initialize JSON files for active writing
        self._init_json_files()
        
        # Initialize debug log files for live debugging
        self._init_debug_logs()
        
        return True
    
    def _init_json_files(self):
        """Initialize JSON files for active writing."""
        OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
        
        # Broadcast JSON - start with empty array
        self._broadcast_json_file = open(BROADCAST_JSON, 'w+', encoding='utf-8')
        self._broadcast_json_file.write('[]')
        self._broadcast_json_file.flush()
        self._write_debug_log("PARSER", f"[JSON_INIT] Broadcast JSON initialized")
        
        # Receive JSON - start with empty array
        self._receive_json_file = open(RECEIVE_JSON, 'w+', encoding='utf-8')
        self._receive_json_file.write('[]')
        self._receive_json_file.flush()
        self._write_debug_log("PARSER", f"[JSON_INIT] Receive JSON initialized")
        
        self._log("JSON files initialized for active writing")
    
    def _init_debug_logs(self):
        """Initialize debug log files for live debugging."""
        OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
        
        # Broadcast debug log
        broadcast_debug_path = OUTPUT_DIR / "broadcast_debug.log"
        self._broadcast_debug_file = open(broadcast_debug_path, 'w')
        self._broadcast_debug_file.write(f"=== Broadcast Debug Log Started at {datetime.now().isoformat()} ===\n")
        self._broadcast_debug_file.flush()
        
        # Receive debug log
        receive_debug_path = OUTPUT_DIR / "receive_debug.log"
        self._receive_debug_file = open(receive_debug_path, 'w')
        self._receive_debug_file.write(f"=== Receive Debug Log Started at {datetime.now().isoformat()} ===\n")
        self._receive_debug_file.flush()
        
        self._log(f"Debug logs initialized: {broadcast_debug_path}, {receive_debug_path}")
    
    def _write_debug_log(self, node_name: str, line: str):
        """Write a line to the appropriate debug log file."""
        timestamp = datetime.now().isoformat()
        formatted = f"[{timestamp}] {line}\n"
        
        if node_name == "BROADCASTER" and self._broadcast_debug_file:
            self._broadcast_debug_file.write(formatted)
            self._broadcast_debug_file.flush()
        elif node_name == "RECEIVER" and self._receive_debug_file:
            self._receive_debug_file.write(formatted)
            self._receive_debug_file.flush()
        elif node_name == "PARSER":
            # Write parser logs to both debug files for correlation
            if self._broadcast_debug_file:
                self._broadcast_debug_file.write(formatted)
                self._broadcast_debug_file.flush()
            if self._receive_debug_file:
                self._receive_debug_file.write(formatted)
                self._receive_debug_file.flush()
    
    def _write_broadcast_record(self, record: BroadcastRecord):
        """Write a broadcast record to JSON immediately."""
        self._write_debug_log("PARSER", f"[JSON_WRITE] Writing broadcast record seq={record.sequence}")
        if self._broadcast_json_file:
            try:
                # Read existing data
                self._broadcast_json_file.seek(0)
                data = json.load(self._broadcast_json_file)
                
                # Append new record
                record_dict = {
                    "timestamp": record.timestamp,
                    "config_id": record.config_id,
                    "sequence": record.sequence,
                    "frequency_mhz": record.frequency_mhz,
                    "spreading_factor": record.spreading_factor,
                    "bandwidth_khz": record.bandwidth_khz,
                    "coding_rate": record.coding_rate,
                    "tx_power_dbm": record.tx_power_dbm,
                    "packet_json": record.packet_json,
                    "raw_line": record.raw_line
                }
                data.append(record_dict)
                
                # Write back
                self._broadcast_json_file.seek(0)
                self._broadcast_json_file.truncate()
                json.dump(data, self._broadcast_json_file, indent=2, ensure_ascii=False)
                self._broadcast_json_file.write('\n')
                self._broadcast_json_file.flush()
                self._write_debug_log("PARSER", f"[JSON_WRITE] Broadcast record written successfully")
            except Exception as e:
                self._write_debug_log("PARSER", f"[JSON_WRITE] ERROR writing broadcast: {e}")
        else:
            self._write_debug_log("PARSER", f"[JSON_WRITE] ERROR: broadcast_json_file is None!")
    
    def _write_receive_record(self, record: ReceiveRecord):
        """Write a receive record to JSON immediately."""
        self._write_debug_log("PARSER", f"[JSON_WRITE] Writing receive record seq={record.sequence}")
        if self._receive_json_file:
            try:
                # Read existing data
                self._receive_json_file.seek(0)
                data = json.load(self._receive_json_file)
                
                # Append new record
                record_dict = {
                    "timestamp": record.timestamp,
                    "config_id": record.config_id,
                    "sequence": record.sequence,
                    "frequency_mhz": record.frequency_mhz,
                    "spreading_factor": record.spreading_factor,
                    "bandwidth_khz": record.bandwidth_khz,
                    "coding_rate": record.coding_rate,
                    "rssi_dbm": record.rssi_dbm,
                    "snr_db": record.snr_db,
                    "reception_timestamp": record.reception_timestamp,
                    "packet_json": record.packet_json,
                    "raw_line": record.raw_line
                }
                data.append(record_dict)
                
                # Write back
                self._receive_json_file.seek(0)
                self._receive_json_file.truncate()
                json.dump(data, self._receive_json_file, indent=2, ensure_ascii=False)
                self._receive_json_file.write('\n')
                self._receive_json_file.flush()
                self._write_debug_log("PARSER", f"[JSON_WRITE] Receive record written successfully")
            except Exception as e:
                self._write_debug_log("PARSER", f"[JSON_WRITE] ERROR writing receive: {e}")
        else:
            self._write_debug_log("PARSER", f"[JSON_WRITE] ERROR: receive_json_file is None!")
    
    def disconnect_nodes(self):
        """Disconnect from both serial ports and close JSON files."""
        self.broadcaster.stop()
        self.receiver.stop()
        
        # Close JSON files
        if self._broadcast_json_file:
            self._write_debug_log("PARSER", "[JSON_CLOSE] Closing broadcast JSON")
            self._broadcast_json_file.close()
            self._broadcast_json_file = None
        if self._receive_json_file:
            self._write_debug_log("PARSER", "[JSON_CLOSE] Closing receive JSON")
            self._receive_json_file.close()
            self._receive_json_file = None
        
        # Close debug log files
        if self._broadcast_debug_file:
            self._broadcast_debug_file.write(f"=== Broadcast Debug Log Ended at {datetime.now().isoformat()} ===\n")
            self._broadcast_debug_file.close()
            self._broadcast_debug_file = None
        if self._receive_debug_file:
            self._receive_debug_file.write(f"=== Receive Debug Log Ended at {datetime.now().isoformat()} ===\n")
            self._receive_debug_file.close()
            self._receive_debug_file = None
    
    def enter_test_mode(self, reader: SerialReader, name: str) -> bool:
        """Navigate to Radio Test mode (option 9) on a node."""
        self._log(f"[{name}] Entering Radio Test mode...")
        
        # Send '9' to select Radio Test
        if not reader.send_command("9"):
            return False
        
        # Wait for prompt
        time.sleep(0.5)
        lines = reader.get_lines(2.0)
        for _, line in lines:
            self._log(f"[{name}] {line}")
            self._write_debug_log(name, line)
            if "Enter JSON" in line or "RADIO TEST" in line:
                return True
        
        return False
    
    def exit_test_mode(self, reader: SerialReader, name: str):
        """Exit Radio Test mode (send X)."""
        reader.send_command("X")
        time.sleep(0.3)
        reader.clear_queue()
    
    def run_single_test(self, config: TestConfig) -> TestSummary:
        """Run a single test configuration."""
        config_id = config.config_id()
        self._log(f"Test {self.current_config_idx + 1}/{len(self.configs)}: {config_id}")
        
        # Enter test mode on both nodes
        if not self.enter_test_mode(self.broadcaster, "BROADCASTER"):
            return TestSummary(
                config_id=config_id, frequency_mhz=config.frequency_mhz,
                spreading_factor=config.spreading_factor, bandwidth_khz=config.bandwidth_khz,
                coding_rate=config.coding_rate, tx_power_dbm=config.tx_power_dbm,
                broadcasts_sent=0, packets_received=0, packet_loss_percent=100.0,
                avg_rssi_dbm=0, avg_snr_db=0, min_rssi_dbm=0, max_rssi_dbm=0,
                min_snr_db=0, max_snr_db=0, test_duration_sec=0, status="FAILED"
            )
        
        if not self.enter_test_mode(self.receiver, "RECEIVER"):
            self.exit_test_mode(self.broadcaster, "BROADCASTER")
            return TestSummary(
                config_id=config_id, frequency_mhz=config.frequency_mhz,
                spreading_factor=config.spreading_factor, bandwidth_khz=config.bandwidth_khz,
                coding_rate=config.coding_rate, tx_power_dbm=config.tx_power_dbm,
                broadcasts_sent=0, packets_received=0, packet_loss_percent=100.0,
                avg_rssi_dbm=0, avg_snr_db=0, min_rssi_dbm=0, max_rssi_dbm=0,
                min_snr_db=0, max_snr_db=0, test_duration_sec=0, status="FAILED"
            )
        
        # Send configurations
        self._log("Sending broadcast config...")
        self.broadcaster.send_json(config.to_broadcast_json())
        
        self._log("Sending receive config...")
        self.receiver.send_json(config.to_receive_json())
        
        # Wait for configs to apply
        time.sleep(CONFIG_APPLY_DELAY)
        
        # Clear any config application output
        self.broadcaster.clear_queue()
        self.receiver.clear_queue()
        
        # Parsers with debug logging
        def parser_logger(msg: str):
            self._write_debug_log("PARSER", msg)
        
        bcast_parser = BroadcastParser(config, logger=parser_logger)
        recv_parser = ReceiveParser(config, logger=parser_logger)
        
        # Test loop - send one broadcast per config
        NUM_BROADCASTS_PER_CONFIG = 1
        broadcasts_sent = 0
        packets_received = 0
        rssi_values = []
        snr_values = []
        test_start = time.time()
        
        for i in range(NUM_BROADCASTS_PER_CONFIG):
            if not self.running:
                break
            
            self._log(f"  Broadcast {i+1}/{NUM_BROADCASTS_PER_CONFIG}...")
            
            # The broadcaster will automatically send packets every ~1 second
            # We just need to collect the output
            
            # Wait for broadcast and reception
            wait_start = time.time()
            while time.time() - wait_start < RECEIVE_WINDOW:
                if not self.running:
                    break
                
                # Read broadcaster lines
                for _, line in self.broadcaster.get_lines(0.1):
                    self._log(f"[BROADCASTER] {line}")
                    self._write_debug_log("BROADCASTER", line)
                    record = bcast_parser.feed(line)
                    if record:
                        self.broadcast_records.append(record)
                        broadcasts_sent += 1
                        # Write to CSV immediately
                        self._write_broadcast_record(record)
                
                # Read receiver lines
                for _, line in self.receiver.get_lines(0.1):
                    self._log(f"[RECEIVER] {line}")
                    self._write_debug_log("RECEIVER", line)
                    record = recv_parser.feed(line)
                    if record:
                        self.receive_records.append(record)
                        packets_received += 1
                        rssi_values.append(record.rssi_dbm)
                        snr_values.append(record.snr_db)
                        # Write to CSV immediately
                        self._write_receive_record(record)
            
            # Small delay between broadcasts
            time.sleep(0.5)
        
        test_duration = time.time() - test_start
        
        # Exit test mode on both nodes
        self.exit_test_mode(self.broadcaster, "BROADCASTER")
        self.exit_test_mode(self.receiver, "RECEIVER")
        
        # Calculate summary
        if broadcasts_sent > 0:
            packet_loss = ((broadcasts_sent - packets_received) / broadcasts_sent) * 100
        else:
            packet_loss = 100.0
        
        if rssi_values:
            avg_rssi = sum(rssi_values) / len(rssi_values)
            min_rssi = min(rssi_values)
            max_rssi = max(rssi_values)
            avg_snr = sum(snr_values) / len(snr_values)
            min_snr = min(snr_values)
            max_snr = max(snr_values)
        else:
            avg_rssi = min_rssi = max_rssi = 0
            avg_snr = min_snr = max_snr = 0
        
        status = "COMPLETE" if packets_received > 0 else ("PARTIAL" if broadcasts_sent > 0 else "FAILED")
        
        summary = TestSummary(
            config_id=config_id,
            frequency_mhz=config.frequency_mhz,
            spreading_factor=config.spreading_factor,
            bandwidth_khz=config.bandwidth_khz,
            coding_rate=config.coding_rate,
            tx_power_dbm=config.tx_power_dbm,
            broadcasts_sent=broadcasts_sent,
            packets_received=packets_received,
            packet_loss_percent=packet_loss,
            avg_rssi_dbm=avg_rssi,
            avg_snr_db=avg_snr,
            min_rssi_dbm=min_rssi,
            max_rssi_dbm=max_rssi,
            min_snr_db=min_snr,
            max_snr_db=max_snr,
            test_duration_sec=test_duration,
            status=status
        )
        
        print(f"  Result: {broadcasts_sent} sent, {packets_received} received, "
              f"{packet_loss:.1f}% loss, RSSI: {avg_rssi:.1f} dBm, SNR: {avg_snr:.1f} dB")
        
        return summary
    
    def save_results(self):
        """Save summary results to JSON file (broadcast and receive JSONs are written actively)."""
        OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
        
        # Summary JSON only - broadcast and receive JSONs are written actively during experiment
        summary_data = []
        for s in self.summaries:
            summary_data.append({
                "config_id": s.config_id,
                "frequency_mhz": s.frequency_mhz,
                "spreading_factor": s.spreading_factor,
                "bandwidth_khz": s.bandwidth_khz,
                "coding_rate": s.coding_rate,
                "tx_power_dbm": s.tx_power_dbm,
                "broadcasts_sent": s.broadcasts_sent,
                "packets_received": s.packets_received,
                "packet_loss_percent": s.packet_loss_percent,
                "avg_rssi_dbm": s.avg_rssi_dbm,
                "avg_snr_db": s.avg_snr_db,
                "min_rssi_dbm": s.min_rssi_dbm,
                "max_rssi_dbm": s.max_rssi_dbm,
                "min_snr_db": s.min_snr_db,
                "max_snr_db": s.max_snr_db,
                "test_duration_sec": s.test_duration_sec,
                "status": s.status
            })
        
        with open(SUMMARY_JSON, 'w') as f:
            json.dump(summary_data, f, indent=2)
        
        self._log(f"Summary saved to {SUMMARY_JSON}")
        self._log(f"Broadcast log: {BROADCAST_JSON} (written actively)")
        self._log(f"Receive log: {RECEIVE_JSON} (written actively)")
    
    def run(self):
        """Run the complete test matrix."""
        # Initialize TUI
        self._init_tui()
        
        self._log("=" * 60)
        self._log("Radio Test Matrix Orchestrator")
        self._log("=" * 60)
        
        if not self.load_test_matrix():
            self._cleanup_tui()
            return False
        
        self._log(f"Loaded {len(self.configs)} test configurations")
        
        if not self.connect_nodes():
            self._cleanup_tui()
            return False
        
        self._log("Connected to both nodes")
        
        # Start progress tracking
        self.start_time = time.time()
        total_configs = len(self.configs)
        
        try:
            for i, config in enumerate(self.configs):
                if not self.running:
                    break
                
                self.current_config_idx = i
                config_start_time = time.time()
                
                # Update progress bar (i+1 because we're about to start this config)
                self._update_progress_display(i + 1, total_configs, config.config_id())
                self._log(f"Starting test {i + 1}/{total_configs}: {config.config_id()}")
                
                summary = self.run_single_test(config)
                self.summaries.append(summary)
                
                # Record time for this config
                config_duration = time.time() - config_start_time
                self._record_config_time(config_duration)
                
                self._log(f"  Result: {summary.broadcasts_sent} sent, {summary.packets_received} received, "
                          f"{summary.packet_loss_percent:.1f}% loss, RSSI: {summary.avg_rssi_dbm:.1f} dBm, SNR: {summary.avg_snr_db:.1f} dB")
                
                # Save progress periodically
                if (i + 1) % 50 == 0:
                    self.save_results()
                    self._log(f"Progress saved ({i + 1}/{total_configs})")
                
                # Small delay between configurations
                time.sleep(0.5)
            
            # Final progress display
            self._update_progress_display(total_configs, total_configs)
        
        finally:
            self.disconnect_nodes()
            self.save_results()
        
        # Print final summary in log area
        total_sent = sum(s.broadcasts_sent for s in self.summaries)
        total_received = sum(s.packets_received for s in self.summaries)
        total_elapsed = time.time() - self.start_time if self.start_time else 0
        
        self._log("=" * 60)
        self._log("TEST MATRIX COMPLETE")
        self._log("=" * 60)
        self._log(f"Total configurations tested: {len(self.summaries)}")
        self._log(f"Total broadcasts sent: {total_sent}")
        self._log(f"Total packets received: {total_received}")
        self._log(f"Total time elapsed: {self._format_time(total_elapsed)}")
        if total_sent > 0:
            self._log(f"Overall packet loss: {((total_sent - total_received) / total_sent) * 100:.1f}%")
        
        # Keep TUI open for a moment to show final results
        time.sleep(2)
        self._cleanup_tui()
        
        return True


def main():
    orchestrator = RadioTestOrchestrator()
    success = orchestrator.run()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()