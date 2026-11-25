"""Provision the SD menu fixture and verify lazy navigation on a Teensy 4.1."""

import argparse
from pathlib import Path
import sys
import time

import serial


def crc8(payload):
    result = 0
    for byte in payload:
        result ^= byte
        for _ in range(8):
            result = ((result << 1) ^ (7 if result & 128 else 0)) & 255
    return result


def frame(prefix, command):
    payload = command.encode("ascii")
    return f"{prefix}{crc8(payload):02X}:".encode("ascii") + payload + b"\n"


class Tour:
    def __init__(self, wire, verbose):
        self.wire = wire
        self.verbose = verbose
        self.position = (0, 0)
        self.checks = 0
        self.generation = None

    def wait(self, check, timeout=4):
        deadline = time.monotonic() + timeout
        partial = bytearray()
        seen = []
        while time.monotonic() < deadline:
            partial.extend(self.wire.read_until(b"\n", 1400))
            if not partial.endswith(b"\n"):
                continue
            line = partial.decode("ascii", "replace").strip()
            partial.clear()
            seen.append(line)
            if self.verbose:
                print(line, flush=True)
            if check(line):
                return line
        raise ValueError(f"Timed out. Recent responses: {seen[-8:]}")

    def admin(self, command, check):
        self.wire.write(frame("!", command))
        response = self.wait(lambda line: check(line) or line.startswith("ERROR "))
        if not check(response):
            raise ValueError(f"{command.split()[0]} failed. Board reported {response}.")
        return response

    def provision(self, folder):
        files = sorted(folder.rglob("*.pkm"))
        if not files:
            raise ValueError("No .pkm files found in the selected folder.")
        for file in files:
            name = "/" + file.relative_to(folder).as_posix()
            payload = file.read_bytes()
            if not 1 <= len(payload) <= 512:
                raise ValueError(f"{name} must contain 1–512 bytes.")
            self.admin(f"PUT {name} {payload.hex()}", lambda line: line == f"STORED {name}")
            received = self.admin(f"GET {name}", lambda line: line.startswith(f"FILE {name} "))
            if bytes.fromhex(received.split(" ", 2)[2]) != payload:
                raise ValueError(f"SD readback differed for {name}.")
            print(f"Verified SD file {name}", flush=True)

    def lifecycle(self, delta=None):
        _, generation, children = self.wait(lambda line: line.startswith("LIFECYCLE ")).split()
        generation = int(generation)
        if int(children) != 5 or (delta is not None and generation != self.generation + delta):
            raise ValueError(f"Unexpected page lifecycle: generation {generation}, root children {children}.")
        self.generation = generation

    def point(self, x, y):
        start = self.position
        for step in range(1, 5):
            px = start[0] + (x - start[0]) * step // 4
            py = start[1] + (y - start[1]) * step // 4
            self.wire.write(frame("@", f"move {px} {py}"))
            time.sleep(0.033)
        self.position = x, y

    def step(self, label, x, y, expected):
        print(label, flush=True)
        self.point(x, y)
        self.wire.write(frame("@", f"click {x} {y}"))
        self.wait(lambda line: line == expected or line.startswith(expected + " "))
        if expected.startswith("PAGE "):
            self.lifecycle(1)
        elif expected.startswith("NAV_ERROR "):
            self.lifecycle(0)
        self.checks += 1
        time.sleep(0.22)

    def media_walkthrough(self):
        self.admin("MEDIA OFF", lambda line: line == "MEDIA OFFLINE")
        self.step("Removed media leaves the current page intact", 70, 111, "NAV_ERROR read /index.pkm")
        for mode, title in (("DEFAULT", "Workshop menu"), ("ALTERNATE", "Replacement menu")):
            self.admin("MEDIA " + mode, lambda line: line == "MEDIA ONLINE")
            self.wait(lambda line: line == "PAGE /index.pkm " + title)
            self.lifecycle(1)
            print(f"Rediscovered {title} without pressing Home", flush=True)
        self.step("Reject an unknown compiled view", 70, 137, "NAV_ERROR view /index.pkm")
        self.step("Instantiate a registered scene lazily", 70, 111, "PAGE /rich.pkm Controls view")
        self.step("Use the compiled slider", 110, 110, "RICH_VALUE 50")
        self.step("Back destroys the rich scene", 27, 15, "PAGE /index.pkm Replacement menu")
        self.admin("MEDIA DEFAULT", lambda line: line == "MEDIA ONLINE")
        self.wait(lambda line: line == "PAGE /index.pkm Workshop menu")
        self.lifecycle(1)

    def walkthrough(self):
        steps = (
            ("Back is unavailable at the start", 27, 15, "NAV_ERROR history /index.pkm"),
            ("Load Tools", 70, 111, "PAGE /tools/index.pkm"),
            ("Load a deeper relative page", 70, 111, "PAGE /tools/advanced/view.pkm"),
            ("Follow two parent segments to About", 70, 111, "PAGE /about.pkm"),
            ("Back reloads Advanced", 27, 15, "PAGE /tools/advanced/view.pkm"),
            ("Back reloads Tools", 27, 15, "PAGE /tools/index.pkm"),
            ("Follow a relative link to the root", 70, 137, "PAGE /index.pkm"),
            ("Keep Home when the destination is missing", 70, 163, "NAV_ERROR read /index.pkm"),
            ("Keep Home when the destination is malformed", 70, 189, "NAV_ERROR page /index.pkm"),
            ("Run a registered firmware action", 70, 215, "ACTION probe"),
            ("Load the About page again", 70, 137, "PAGE /about.pkm"),
            ("Home reloads the index", 100, 15, "PAGE /index.pkm"),
            ("Home clears Back history", 27, 15, "NAV_ERROR history /index.pkm"),
        )
        for step in steps:
            self.step(*step)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--folder", type=Path, default=Path(__file__).parent / "menu",
                        help="Folder of .pkm pages to upload")
    parser.add_argument("--provision-only", action="store_true",
                        help="Upload and read back pages without running fixture-specific navigation")
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--expect-existing", action="store_true",
                        help="Require Home to load before sending any files")
    args = parser.parse_args()
    try:
        with serial.Serial(args.port, 115200, timeout=0.15, write_timeout=3) as wire:
            tour = Tour(wire, args.verbose)
            status = tour.admin("STATUS", lambda line: line.startswith("STATUS "))
            if status != "STATUS SD_OK DISPLAY_OK":
                raise ValueError(f"The board reported {status}.")
            before = tour.admin("START", lambda line: line == "FALLBACK" or line.startswith("PAGE "))
            tour.lifecycle()
            if args.expect_existing and not before.startswith("PAGE /index.pkm "):
                raise ValueError("The index did not load from SD before provisioning.")
            print(f"Initial state: {before}", flush=True)
            tour.provision(args.folder)
            tour.admin("START", lambda line: line.startswith("PAGE /index.pkm "))
            tour.lifecycle(1)
            if not args.provision_only:
                tour.admin("PUT /../outside.pkm 4142", lambda line: line == "ERROR write-or-conflict")
                tour.admin("PUT /index.pkm 4142", lambda line: line == "ERROR write-or-conflict")
                tour.walkthrough()
                tour.media_walkthrough()
            checks = tour.checks
            if not args.provision_only:
                wire.write(b"!AA:PUT /aborted.pkm ")
                wire.flush()
                time.sleep(0.06)
        if not args.provision_only:
            time.sleep(0.25)
            with serial.Serial(args.port, 115200, timeout=0.15, write_timeout=3) as wire:
                reopened = Tour(wire, args.verbose)
                reopened.admin("STATUS", lambda line: line == "STATUS SD_OK DISPLAY_OK")
                reopened.admin("GET /aborted.pkm", lambda line: line == "ERROR read")
                print("Verified USB reconnect cleared the partial upload", flush=True)
        print(f"PASS on {args.port}: SD file readback and {checks} navigation checks")
    except (OSError, ValueError, serial.SerialException) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
