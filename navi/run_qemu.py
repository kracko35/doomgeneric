#!/usr/bin/env python3
"""Run the SH DOOM port in a new, diskless NHDT QEMU and collect evidence.

This runner needs Python's standard library and the existing custom NHDT QEMU.
It supplies the executable and a user-provided WAD once through GDB. Rendering,
game simulation and timing run on the emulated SH CPU, without host OS shims.
No firmware, settings or HDD image is modified. Existing QEMU guests are untouched.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import time
import zlib


STATE_FIELDS = ("magic", "frames", "ticks", "gamestate", "demoplayback",
                "gametic", "stop", "error")
SENTINEL = 0x8FFF0000
STACK = 0x8FFE0000
DEFAULT_WAD_ADDRESS = 0x8B000000
EXPECTED_MAGIC = 0x444F4F4D


def integer(value):
    return int(value, 0) if isinstance(value, str) else int(value)


def digest(data):
    return hashlib.sha256(data).hexdigest()


class RSP:
    """The small subset of GDB's remote protocol needed for RAM loading."""

    def __init__(self, port):
        self.sock = socket.create_connection(("127.0.0.1", port), timeout=5)
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.sock.settimeout(10)

    def receive(self):
        while True:
            while True:
                char = self.sock.recv(1)
                if not char:
                    raise EOFError("QEMU closed the debug connection")
                if char == b"$":
                    break
            packet = bytearray()
            while True:
                char = self.sock.recv(1)
                if not char:
                    raise EOFError("QEMU closed the debug connection")
                if char == b"#":
                    break
                packet.extend(char)
            checksum = self.sock.recv(1) + self.sock.recv(1)
            if len(checksum) != 2 or int(checksum, 16) != sum(packet) & 255:
                self.sock.sendall(b"-")
                raise RuntimeError("GDB packet checksum mismatch")
            self.sock.sendall(b"+")
            result = packet.decode("ascii")
            # O followed by hexadecimal bytes is a remote console message.
            if result != "OK" and result.startswith("O"):
                continue
            return result

    def send(self, command, wait=True):
        data = command.encode("ascii")
        self.sock.sendall(b"$" + data + b"#" + f"{sum(data) & 255:02x}".encode())
        return self.receive() if wait else None

    def interrupt(self):
        self.sock.sendall(b"\x03")
        return self.receive()

    def write(self, address, data):
        for offset in range(0, len(data), 1024):
            block = data[offset:offset + 1024]
            answer = self.send(f"M{address + offset:x},{len(block):x}:" + block.hex())
            if answer != "OK":
                raise RuntimeError(f"GDB write at {address + offset:#x}: {answer}")

    def read(self, address, length):
        output = bytearray()
        for offset in range(0, length, 1024):
            count = min(1024, length - offset)
            result = self.send(f"m{address + offset:x},{count:x}")
            if result.startswith("E"):
                raise RuntimeError(f"GDB read at {address + offset:#x}: {result}")
            output.extend(bytes.fromhex(result))
        if len(output) != length:
            raise RuntimeError("Short GDB memory read")
        return bytes(output)

    def register(self, number, value):
        if self.send(f"P{number:x}={value & 0xffffffff:08x}") != "OK":
            raise RuntimeError(f"Cannot set SH register {number}")

    def registers(self):
        data = bytes.fromhex(self.send("g"))
        words = struct.unpack(">" + "I" * (len(data) // 4), data)
        names = [f"r{i}" for i in range(16)] + [
            "pc", "pr", "gbr", "vbr", "mach", "macl", "sr", "fpul", "fpscr"]
        return dict(zip(names, words))


def qmp(path, name, arguments=None):
    with socket.socket(socket.AF_UNIX) as connection:
        connection.settimeout(10)
        connection.connect(str(path))
        stream = connection.makefile("rwb", buffering=0)
        json.loads(stream.readline())

        def call(command, parameters=None):
            request = {"execute": command}
            if parameters is not None:
                request["arguments"] = parameters
            stream.write(json.dumps(request).encode() + b"\n")
            while True:
                line = stream.readline()
                if not line:
                    raise EOFError("QEMU closed the monitor connection")
                result = json.loads(line)
                if "error" in result:
                    raise RuntimeError(result["error"])
                if "return" in result:
                    return result["return"]

        call("qmp_capabilities")
        return call(name, arguments)


def screenshot(qmp_path, output):
    """Losslessly encode the actual QEMU scanout, without adding any pixels."""
    ppm = output.with_suffix(".ppm")
    qmp(qmp_path, "screendump", {"filename": str(ppm), "format": "ppm"})
    with ppm.open("rb") as stream:
        if stream.readline().strip() != b"P6":
            raise ValueError("Unexpected QEMU screenshot format")
        dimensions = stream.readline()
        while dimensions.startswith(b"#"):
            dimensions = stream.readline()
        width, height = map(int, dimensions.split())
        if stream.readline().strip() != b"255":
            raise ValueError("Unexpected QEMU screenshot depth")
        pixels = stream.read()
    if len(pixels) != width * height * 3:
        raise ValueError("Incomplete QEMU screenshot")

    def chunk(kind, data):
        return (struct.pack(">I", len(data)) + kind + data +
                struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff))

    rows = b"".join(b"\0" + pixels[y * width * 3:(y + 1) * width * 3]
                    for y in range(height))
    output.write_bytes(
        b"\x89PNG\r\n\x1a\n" +
        chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
        chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))
    return {"png": output.name, "ppm": ppm.name, "width": width,
            "height": height, "rgb_sha256": digest(pixels)}


def unused_port():
    with socket.socket() as connection:
        connection.bind(("127.0.0.1", 0))
        return connection.getsockname()[1]


def checked_ram(address, size, label):
    if not (0x88000000 <= address < address + size <= 0x90000000):
        raise ValueError(f"{label} is outside SH guest RAM")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qemu", type=Path, required=True,
                        help="Existing custom qemu-system-sh4eb executable")
    parser.add_argument("--boot-dir", type=Path, required=True,
                        help="Existing NHDT boot bundle (read only)")
    parser.add_argument("--build", type=Path,
                        default=Path(__file__).parent / "build" / "build.json")
    parser.add_argument("--wad", type=Path, required=True,
                        help="User-supplied DOOM WAD; opened read only")
    parser.add_argument("--output", type=Path, default=Path("navi-evidence"))
    parser.add_argument("--seconds", type=float, default=90,
                        help="Maximum wall time after guest launch")
    parser.add_argument("--sample-seconds", type=float, default=0.5)
    parser.add_argument("--min-demo-tics", type=int, default=350,
                        help="Observe at least this many 35 Hz game tics in the demo")
    parser.add_argument("--gdb-port", type=int, default=0)
    parser.add_argument("--vnc", help="Optional loopback VNC address, e.g. 127.0.0.1:7")
    parser.add_argument("--keep-running", action="store_true",
                        help="Leave this guest running after successful verification")
    args = parser.parse_args()
    if args.seconds <= 0 or args.sample_seconds <= 0 or args.min_demo_tics < 35:
        parser.error("Time limits must be positive")
    if args.vnc and not args.vnc.startswith("127.0.0.1:"):
        parser.error("VNC must bind to 127.0.0.1")
    if not 0 <= args.gdb_port <= 65535:
        parser.error("Invalid GDB port")

    qemu_exe = args.qemu.resolve(strict=True)
    boot_dir = args.boot_dir.resolve(strict=True)
    build_path = args.build.resolve(strict=True)
    wad_path = args.wad.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    build = json.loads(build_path.read_text())
    symbols = {key: integer(value) for key, value in build["symbols"].items()}
    binary_path = Path(build.get("binary", "doom.bin"))
    if not binary_path.is_absolute():
        binary_path = build_path.parent / binary_path
    payload = binary_path.read_bytes()
    wad = wad_path.read_bytes()
    if len(wad) < 12 or wad[:4] not in (b"IWAD", b"PWAD"):
        raise ValueError("Not a WAD file")
    if build.get("sha256") and digest(payload) != build["sha256"]:
        raise ValueError("Executable does not match build.json SHA-256")
    base = integer(build.get("base", 0x8C000000))
    wad_address = integer(build.get("wad_address", DEFAULT_WAD_ADDRESS))
    entry = integer(build.get("entry", symbols.get("_start", base)))
    state_address = symbols["navi_state"]
    wad_size_address = symbols["navi_wad_size"]
    checked_ram(base, len(payload), "Executable")
    checked_ram(wad_address, len(wad), "WAD")
    checked_ram(state_address, len(STATE_FIELDS) * 4, "State")
    checked_ram(wad_size_address, 4, "WAD length")
    if max(base, wad_address) < min(base + len(payload), wad_address + len(wad)):
        raise ValueError("Executable and WAD overlap")
    if not base <= entry < base + len(payload):
        raise ValueError("Entry point is outside the executable")
    port = args.gdb_port or unused_port()
    # Keep the Unix socket path short even when the evidence directory is long.
    runtime = Path(tempfile.mkdtemp(prefix="navi-doom-"))
    qmp_path = runtime / "qmp.sock"
    environment = {key: value for key, value in os.environ.items()
                   if not key.startswith("NAVI_")}
    environment.update(NAVI_BOOTDIR=str(boot_dir), NAVI_BT="0")
    command = [str(qemu_exe), "-machine", "nhdt", "-cpu", "sh7785",
               "-accel", "tcg,thread=single", "-m", "128M", "-display", "none",
               "-serial", "null", "-serial", "null", "-monitor", "none", "-S",
               "-no-reboot", "-audio", "driver=none", "-gdb",
               f"tcp:127.0.0.1:{port}", "-qmp",
               f"unix:{qmp_path},server=on,wait=off"]
    if args.vnc:
        command.extend(("-vnc", args.vnc))
    report = {
        "all_passed": False, "disk_attached": False, "sound_enabled": False,
        "entry_method": "GDB RAM injection into a new diskless NHDT SH guest",
        "host_injects_frames": False, "host_task_service_shims": False,
        "qemu": str(qemu_exe), "qemu_sha256": digest(qemu_exe.read_bytes()),
        "payload_sha256": digest(payload), "wad_sha256": digest(wad),
        "wad_bytes": len(wad), "wad_address": hex(wad_address),
        "command": command, "samples": [], "captures": [],
        "boot_bundle_read_only": True, "existing_guests_controlled": False,
    }
    process = None
    rsp = None
    running = False
    success = False
    try:
        with (output / "qemu.log").open("wb") as log:
            process = subprocess.Popen(command, env=environment,
                                       stdin=subprocess.DEVNULL, stdout=log,
                                       stderr=log, start_new_session=True)
        report.update(qemu_pid=process.pid, qmp=str(qmp_path), gdb_port=port,
                      vnc=args.vnc)
        # This socket belongs exclusively to the newly created child. Establish
        # its monitor before using GDB, so an occupied explicit GDB port cannot
        # lead to controlling a different guest after this child fails to bind.
        for _ in range(100):
            if process.poll() is not None:
                raise RuntimeError("QEMU exited; inspect qemu.log")
            if qmp_path.exists():
                qmp(qmp_path, "query-status")
                break
            time.sleep(0.1)
        else:
            raise TimeoutError("New QEMU monitor did not start")
        for _ in range(100):
            if process.poll() is not None:
                raise RuntimeError("QEMU exited; inspect qemu.log")
            try:
                rsp = RSP(port)
                break
            except ConnectionRefusedError:
                time.sleep(0.1)
        if rsp is None:
            raise TimeoutError("QEMU GDB listener did not start")
        rsp.send("?")
        print("Loading executable and WAD into the new SH guest...", flush=True)
        rsp.write(base, payload)
        rsp.write(wad_address, wad)
        # _start must preserve navi_wad_size (the linker places it in .data).
        rsp.write(wad_size_address, struct.pack(">I", len(wad)))
        rsp.write(SENTINEL, bytes.fromhex("000b0009"))
        # SR first: the register bank is selected by SR, not the G packet order.
        for number, value in ((22, 0x400000F0), (24, 0x000C0001),
                              (15, STACK), (17, SENTINEL),
                              (4, wad_address), (5, len(wad)), (16, entry)):
            rsp.register(number, value)
        if rsp.registers()["pc"] != entry:
            raise RuntimeError("Guest entry register verification failed")
        started = time.monotonic()
        title = None
        demos = []
        rsp.send("c", wait=False)
        running = True
        while time.monotonic() - started < args.seconds:
            time.sleep(args.sample_seconds)
            if process.poll() is not None:
                raise RuntimeError("QEMU exited during the game")
            stop_reply = rsp.interrupt()
            running = False
            raw = rsp.read(state_address, len(STATE_FIELDS) * 4)
            state = dict(zip(STATE_FIELDS, struct.unpack(">8I", raw)))
            state["wall_seconds"] = round(time.monotonic() - started, 3)
            state["pc"] = hex(rsp.registers()["pc"])
            state["gdb_stop"] = stop_reply
            report["samples"].append(state)
            print(json.dumps(state), flush=True)
            if state["error"]:
                raise RuntimeError(f"Guest reported error {state['error']}")
            if state["stop"]:
                raise RuntimeError("Guest stopped before verification completed")
            if state["magic"] == integer(build.get("magic", EXPECTED_MAGIC)):
                # doomstat.h: GS_LEVEL=0, GS_DEMOSCREEN=3.
                if state["frames"] and state["gamestate"] == 3 and title is None:
                    pointer = struct.unpack(">I", rsp.read(symbols["pagename"], 4))[0]
                    if pointer and rsp.read(pointer, 9) == b"TITLEPIC\0":
                        title = dict(state)
                        capture = screenshot(qmp_path, output / "title.png")
                        capture.update(kind="title", state=state)
                        report["captures"].append(capture)
                if state["frames"] and state["gamestate"] == 0 and state["demoplayback"]:
                    elapsed_tics = ((state["gametic"] - demos[-1]["state"]["gametic"])
                                    & 0xffffffff) if demos else 0
                    if not demos or elapsed_tics >= 35:
                        capture = screenshot(qmp_path, output / f"demo-{len(demos)}.png")
                        capture.update(kind="demo", state=state)
                        demos.append(capture)
                        report["captures"].append(capture)
            if title and len(demos) >= 2 and state["gametic"]-demos[0]["state"]["gametic"] >= args.min_demo_tics:
                first, second = demos[0], demos[-1]
                success = (
                    second["state"]["frames"] > first["state"]["frames"] and
                    second["state"]["gametic"] > first["state"]["gametic"] and
                    second["rgb_sha256"] != first["rgb_sha256"])
                if success:
                    break
            rsp.send("c", wait=False)
            running = True
        if not success:
            raise TimeoutError("Did not observe a title and advancing DOOM demo before the limit")
        report.update(all_passed=True, title_observed=True,
                      demo_playback_observed=True, demo_motion_verified=True)
        if args.keep_running:
            rsp.send("c", wait=False)
            running = True
            report["live_status"] = qmp(qmp_path, "query-status")
            if not report["live_status"].get("running"):
                raise RuntimeError("QEMU failed to resume after verification")
    except BaseException as error:
        report.update(all_passed=False, error=repr(error))
        if rsp and process and process.poll() is None:
            try:
                if running:
                    rsp.interrupt()
                    running = False
                report["failure_registers"] = rsp.registers()
                if "navi_log_length" in symbols:
                    length = min(16383, struct.unpack(">I", rsp.read(symbols["navi_log_length"], 4))[0])
                    guest_log = rsp.read(symbols["navi_log"], length).decode("utf-8", "replace")
                    (output / "guest.log").write_text(guest_log)
                    print(guest_log, flush=True)
                report["failure_screenshot"] = screenshot(qmp_path, output / "failure.png")
            except Exception as capture_error:
                report["failure_capture_error"] = repr(capture_error)
        raise
    finally:
        if rsp:
            rsp.sock.close()
        if process and not (report["all_passed"] and args.keep_running):
            try:
                qmp(qmp_path, "quit")
            except Exception:
                if process.poll() is None:
                    process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        report["guest_left_running"] = bool(process and process.poll() is None)
        report["wad_unchanged"] = digest(wad_path.read_bytes()) == digest(wad)
        (output / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps({key: value for key, value in report.items()
                          if key not in ("samples", "command", "captures")}, indent=2), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
