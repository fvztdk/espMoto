#!/usr/bin/env python3
"""Web simulator server for espMoto LVGL UI."""

import argparse
import ctypes
import json
import os
import shutil
import subprocess
import threading
import time
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse

SIM_DIR = os.path.dirname(os.path.abspath(__file__))
BUILD_DIR = os.path.join(SIM_DIR, "build")
BASE_SO = os.path.join(BUILD_DIR, "libespmoto_lvgl.so")
INDEX_HTML = os.path.join(SIM_DIR, "index.html")
SCREEN_SIZE = 466
FRAME_BYTES = SCREEN_SIZE * SCREEN_SIZE * 4


def _load_dotenv() -> None:
    """Load key=value pairs from .env files into os.environ without overwriting existing vars."""
    for env_path in (os.path.join(SIM_DIR, ".env"), os.path.join(os.path.dirname(SIM_DIR), ".env")):
        if not os.path.isfile(env_path):
            continue
        with open(env_path, "r", encoding="utf-8") as f:
            for raw_line in f:
                line = raw_line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                key, val = line.split("=", 1)
                key = key.strip()
                val = val.strip().strip('"').strip("'")
                if key and key not in os.environ:
                    os.environ[key] = val


class LvglSimulator:
    """Thread-safe wrapper around the compiled LVGL firmware UI shared library."""

    def __init__(self) -> None:
        self.lock = threading.RLock()
        self._lib = None
        self._loaded_so_path = None
        self._reload_counter = 0

        self.state = {
            "page": 1,
            "demoMode": True,
            "phoneConnected": False,
            "navActive": False,
            "maneuver": 2,
            "distance": "250 m",
            "street": "Passeig de Gràcia",
            "eta": "18:15",
            "obdConnected": True,
            "rpm": 2450,
            "speedKmh": 68,
            "gear": 3,
            "engineTempC": 92,
            "batteryVolts": 13.8,
            "buildStatus": "ok",
            "buildOutput": "Loaded initial build",
            "buildTime": time.strftime("%H:%M:%S"),
        }

        self._demo_phase = 0
        self._last_demo_tick = time.monotonic()
        self._load_library()

    def _configure_signatures(self, lib: ctypes.CDLL) -> None:
        lib.sim_init.argtypes = []
        lib.sim_init.restype = None

        lib.sim_tick.argtypes = [ctypes.c_uint32]
        lib.sim_tick.restype = None

        lib.sim_render.argtypes = [
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_float,
        ]
        lib.sim_render.restype = None

        lib.sim_next_page.argtypes = []
        lib.sim_next_page.restype = None

        lib.sim_set_page.argtypes = [ctypes.c_int]
        lib.sim_set_page.restype = None

        lib.sim_get_page.argtypes = []
        lib.sim_get_page.restype = ctypes.c_int

        lib.sim_set_mouse.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
        lib.sim_set_mouse.restype = None

        lib.sim_set_obd_config_json.argtypes = [ctypes.c_char_p]
        lib.sim_set_obd_config_json.restype = None

        lib.sim_reset_pairing.argtypes = []
        lib.sim_reset_pairing.restype = None

        lib.sim_complete_pairing.argtypes = []
        lib.sim_complete_pairing.restype = None

        lib.sim_get_pairing_payload.argtypes = []
        lib.sim_get_pairing_payload.restype = ctypes.c_char_p

        lib.sim_get_rgba_buffer.argtypes = []
        lib.sim_get_rgba_buffer.restype = ctypes.POINTER(ctypes.c_uint8)

    def _unload_library(self) -> None:
        self._lib = None

        if self._loaded_so_path and os.path.exists(self._loaded_so_path):
            try:
                os.remove(self._loaded_so_path)
            except OSError:
                pass
            self._loaded_so_path = None

    def _load_library(self) -> None:
        with self.lock:
            saved_page = self.state["page"]
            self._unload_library()

            self._reload_counter += 1
            runtime_so = os.path.join(BUILD_DIR, f"libespmoto_lvgl.live.{self._reload_counter}.so")
            shutil.copy2(BASE_SO, runtime_so)
            self._loaded_so_path = runtime_so

            lib = ctypes.CDLL(runtime_so)
            self._configure_signatures(lib)
            self._lib = lib
            self._lib.sim_init()
            self.render_locked()
            self._lib.sim_set_page(saved_page)
            self.render_locked()

    def render_locked(self) -> None:
        if self._lib is None:
            return
        s = self.state
        self._lib.sim_render(
            1 if s["demoMode"] else 0,
            1 if s["phoneConnected"] else 0,
            1 if s["navActive"] else 0,
            int(s["maneuver"]),
            str(s["distance"]).encode("utf-8"),
            str(s["street"]).encode("utf-8"),
            str(s["eta"]).encode("utf-8"),
            1 if s["obdConnected"] else 0,
            int(s["rpm"]),
            int(s["speedKmh"]),
            int(s["gear"]),
            int(s["engineTempC"]),
            float(s["batteryVolts"]),
        )
        s["page"] = int(self._lib.sim_get_page())

    def step(self) -> None:
        with self.lock:
            now = time.monotonic()
            if self.state["demoMode"] and (now - self._last_demo_tick) >= 0.12:
                self._last_demo_tick = now
                self._demo_phase = (self._demo_phase + 1) % 80
                # Smoothly sweep from idle (1100 RPM) to highway cruise & brief pull (5750 RPM)
                tri = 40 - abs(40 - self._demo_phase)
                rpm = 1100 + tri * 118
                speed = 0 if rpm <= 1450 else int((rpm - 1200) / 36)
                gear = 0 if speed == 0 else min(5, max(1, speed // 24 + 1))
                self.state["rpm"] = rpm
                self.state["speedKmh"] = speed
                self.state["gear"] = gear
                self.state["engineTempC"] = 92
                self.state["batteryVolts"] = 13.8

            self.render_locked()
            if self._lib is not None:
                self._lib.sim_tick(33)
                self.state["page"] = int(self._lib.sim_get_page())

    def get_frame_bytes(self) -> bytes:
        with self.lock:
            if self._lib is None:
                return bytes(FRAME_BYTES)
            ptr = self._lib.sim_get_rgba_buffer()
            return ctypes.string_at(ptr, FRAME_BYTES)

    def get_state_snapshot(self) -> dict:
        with self.lock:
            snap = dict(self.state)
            page_names = {0: "NAVIGATION", 1: "TELEMETRY", 2: "SETTINGS", 3: "PAIRING"}
            snap["pageName"] = page_names.get(snap["page"], "TELEMETRY")
            if self._lib is not None:
                raw_qr = self._lib.sim_get_pairing_payload()
                snap["pairingPayload"] = raw_qr.decode("utf-8") if raw_qr else ""
            else:
                snap["pairingPayload"] = ""
            return snap

    def update_state(self, payload: dict) -> dict:
        with self.lock:
            action = payload.get("action")
            if action == "btnA" and self._lib is not None:
                if self.state["page"] in (0, 1):
                    self._lib.sim_next_page()
                    self.state["page"] = int(self._lib.sim_get_page())
            elif action == "btnB":
                self.state["demoMode"] = not self.state["demoMode"]
            elif action == "openSettings" and self._lib is not None:
                self._lib.sim_set_page(2)
                self.state["page"] = int(self._lib.sim_get_page())
            elif action == "resetPairing" and self._lib is not None:
                self._lib.sim_reset_pairing()
                self.state["page"] = int(self._lib.sim_get_page())
            elif action == "completePairing" and self._lib is not None:
                self._lib.sim_complete_pairing()
                self.state["page"] = int(self._lib.sim_get_page())

            if "blePayload" in payload:
                raw = str(payload["blePayload"])
                parts = raw.split("|", 4)
                if len(parts) == 5:
                    self.state["navActive"] = int(parts[0] or "0") != 0
                    self.state["maneuver"] = max(0, min(17, int(parts[1] or "0")))
                    self.state["distance"] = parts[2]
                    self.state["street"] = parts[3]
                    self.state["eta"] = parts[4]
                    self.state["phoneConnected"] = True

            for key in (
                "demoMode",
                "phoneConnected",
                "navActive",
                "obdConnected",
            ):
                if key in payload:
                    self.state[key] = bool(payload[key])

            for key in ("maneuver", "rpm", "speedKmh", "gear", "engineTempC"):
                if key in payload:
                    self.state[key] = int(payload[key])

            if "batteryVolts" in payload:
                self.state["batteryVolts"] = float(payload["batteryVolts"])

            for key in ("distance", "street", "eta"):
                if key in payload:
                    self.state[key] = str(payload[key])

            if "page" in payload and self._lib is not None:
                req_page = int(payload["page"])
                if req_page not in (0, 1, 2, 3):
                    req_page = 1
                self._lib.sim_set_page(req_page)
                self.state["page"] = int(self._lib.sim_get_page())
            elif ("blePayload" in payload or "navActive" in payload) and self._lib is not None:
                self._lib.sim_set_page(0 if self.state["navActive"] else 1)
                self.state["page"] = int(self._lib.sim_get_page())

            if "obdConfigJson" in payload and self._lib is not None:
                cfg_json = str(payload["obdConfigJson"]).encode("utf-8")
                self._lib.sim_set_obd_config_json(cfg_json)

            self.render_locked()
            return self.get_state_snapshot()

    def handle_touch(self, x: int, y: int, pressed: bool) -> dict:
        with self.lock:
            if self._lib is not None:
                self._lib.sim_set_mouse(int(x), int(y), 1 if pressed else 0)
                self.state["page"] = int(self._lib.sim_get_page())
            return {"ok": True, "page": self.state["page"]}

    def rebuild(self) -> dict:
        with self.lock:
            proc = subprocess.run(
                ["make", "-C", SIM_DIR, "app", "-j4"],
                capture_output=True,
                text=True,
                check=False,
            )
            combined = (proc.stdout + "\n" + proc.stderr).strip()
            self.state["buildTime"] = time.strftime("%H:%M:%S")
            if proc.returncode == 0:
                self._load_library()
                self.state["buildStatus"] = "ok"
                self.state["buildOutput"] = combined or "Rebuilt successfully"
                return {"ok": True, "output": self.state["buildOutput"], "buildTime": self.state["buildTime"]}

            self.state["buildStatus"] = "error"
            self.state["buildOutput"] = combined or f"Build exited with code {proc.returncode}"
            return {"ok": False, "output": self.state["buildOutput"], "buildTime": self.state["buildTime"]}


SIM: LvglSimulator | None = None


class SimHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def handle(self) -> None:
        try:
            super().handle()
        except (ConnectionResetError, BrokenPipeError):
            return

    def log_message(self, format: str, *args) -> None:
        # Suppress per-frame HTTP logs so terminal stays clean
        return

    def _send_bytes(self, code: int, content_type: str, body: bytes) -> None:
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _send_json(self, code: int, obj: dict) -> None:
        self._send_bytes(code, "application/json; charset=utf-8", json.dumps(obj).encode("utf-8"))

    def _read_json(self) -> dict:
        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length) if length > 0 else b"{}"
        return json.loads(raw.decode("utf-8"))

    def do_GET(self) -> None:
        assert SIM is not None
        path = urlparse(self.path).path

        if path == "/" or path == "/index.html":
            with open(INDEX_HTML, "rb") as f:
                self._send_bytes(HTTPStatus.OK, "text/html; charset=utf-8", f.read())
        elif path == "/api/frame.rgba":
            self._send_bytes(HTTPStatus.OK, "application/octet-stream", SIM.get_frame_bytes())
        elif path == "/api/state":
            self._send_json(HTTPStatus.OK, SIM.get_state_snapshot())
        else:
            self._send_json(HTTPStatus.NOT_FOUND, {"error": "Not found"})

    def do_POST(self) -> None:
        assert SIM is not None
        path = urlparse(self.path).path

        try:
            if path == "/api/state":
                data = self._read_json()
                self._send_json(HTTPStatus.OK, SIM.update_state(data))
            elif path == "/api/touch":
                data = self._read_json()
                res = SIM.handle_touch(data.get("x", 0), data.get("y", 0), bool(data.get("pressed", False)))
                self._send_json(HTTPStatus.OK, res)
            elif path == "/api/rebuild":
                res = SIM.rebuild()
                code = HTTPStatus.OK if res["ok"] else HTTPStatus.INTERNAL_SERVER_ERROR
                self._send_json(code, res)
            else:
                self._send_json(HTTPStatus.NOT_FOUND, {"error": "Not found"})
        except Exception as exc:
            self._send_json(HTTPStatus.BAD_REQUEST, {"error": str(exc)})


def _render_loop(stop_event: threading.Event) -> None:
    assert SIM is not None
    while not stop_event.is_set():
        SIM.step()
        stop_event.wait(0.033)


def main() -> None:
    global SIM
    _load_dotenv()
    default_port = int(os.environ.get("SIMULATOR_PORT", "8152"))
    display_host = os.environ.get("SIMULATOR_HOST", "localhost")
    parser = argparse.ArgumentParser(description="espMoto LVGL Web Simulator")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=default_port)
    args = parser.parse_args()

    SIM = LvglSimulator()
    stop_event = threading.Event()
    worker = threading.Thread(target=_render_loop, args=(stop_event,), daemon=True)
    worker.start()

    ThreadingHTTPServer.allow_reuse_address = True
    server = ThreadingHTTPServer((args.host, args.port), SimHandler)
    print(f"espMoto LVGL Simulator listening on http://{display_host}:{args.port} (bind {args.host}:{args.port})", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        stop_event.set()
        server.server_close()


if __name__ == "__main__":
    main()
