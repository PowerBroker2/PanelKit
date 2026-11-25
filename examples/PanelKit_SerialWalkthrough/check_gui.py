"""Take a paced USB Serial tour of the PanelKit display and check widget state."""

import argparse
import math
import sys
import time

import serial


def crc8(payload):
    """CRC-8/ATM over the exact command bytes."""
    checksum = 0
    for byte in payload:
        checksum ^= byte
        for _ in range(8):
            checksum = ((checksum << 1) ^ (7 if checksum & 0x80 else 0)) & 255
    return checksum


def packet(command, ending=b"\n", *, corrupt=False):
    data = command.encode("ascii")
    checksum = crc8(data) ^ int(corrupt)
    return f"@{checksum:02X}:".encode("ascii") + data + ending


class Walkthrough:
    def __init__(self, wire, pace, verbose):
        self.wire = wire
        self.pace = pace
        self.verbose = verbose
        self.pointer = (12, 17)
        self.lines = []
        self.partial = bytearray()
        self.checkpoints = 0

    def receive(self):
        self.partial.extend(self.wire.read(self.wire.in_waiting))
        while b"\n" in self.partial:
            raw, _, self.partial = self.partial.partition(b"\n")
            line = raw.decode("ascii", "replace").rstrip("\r")
            self.lines.append(line)
            if self.verbose:
                print(line, flush=True)

    def wait_for(self, predicate, label, start=0, timeout=3):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self.receive()
            result = next((line for line in self.lines[start:] if predicate(line)), None)
            if result is not None:
                return result
            time.sleep(0.015)
        raise ValueError(f"{label}. Last device messages: {self.lines[-12:]!r}")

    def send(self, command, pause=0.045):
        self.send_bytes(packet(command), pause)

    def send_bytes(self, data, pause=0.045):
        self.wire.write(data)
        time.sleep(pause * self.pace)
        self.receive()

    def move(self, x, y):
        start_x, start_y = self.pointer
        distance = math.hypot(x - start_x, y - start_y)
        if not distance:
            return
        steps = min(10, max(2, math.ceil(distance / 23)))
        for step in range(1, steps + 1):
            px = round(start_x + (x - start_x) * step / steps)
            py = round(start_y + (y - start_y) * step / steps)
            self.send(f"move {px} {py}", 0.035)
        self.pointer = x, y

    def click(self, x, y):
        self.move(x, y)
        self.send(f"click {x} {y}")

    def wheel(self, x, y, steps):
        self.move(x, y)
        self.send(f"scroll {x} {y} {steps}")

    def drag(self, start, end):
        self.move(*start)
        self.send(f"down {start[0]} {start[1]}", 0.10)
        self.move(*end)
        self.send(f"up {end[0]} {end[1]}")

    def step(self, label, operation, *expected):
        self.checkpoints += 1
        print(label, flush=True)
        start = len(self.lines)
        operation()
        for expected_line in expected:
            self.wait_for(lambda line: line == expected_line,
                          f"Did not receive {expected_line!r} after {label}", start)
        time.sleep(0.25 * self.pace)
        self.receive()

    def main(self):
        start_time = time.monotonic()
        ready = self.wait_for(lambda line: line.startswith("PANELKIT_GUI_READY "),
                              "Walkthrough firmware did not announce itself", timeout=8)
        if ready != "PANELKIT_GUI_READY DISPLAY_OK":
            raise ValueError("Teensy responded, but the TFT could not initialize")
        self.wait_for(lambda line: line == "WINDOW 148 8 164 188", "Initial inspector geometry missing")
        def corrupted_records():
            before = len(self.lines)
            self.send_bytes(packet("click 25 112", corrupt=True))
            self.send_bytes(b"click 25 112\n")
            self.send_bytes(packet("text " + "X" * 100))
            if any(line == "VISIBLE 0" for line in self.lines[before:]):
                raise ValueError("An invalid command changed the UI")

        self.step("Reject bad CRC, unchecked, and oversized records", corrupted_records, "ERRORS 3 2")

        def split_packet():
            data = packet("move 20 22", b"\r\n")
            before = len(self.lines)
            self.send_bytes(data[:6])
            if any(line.startswith("POINTER 20 22") for line in self.lines[before:]):
                raise ValueError("A partial command changed the UI")
            self.send_bytes(data[6:])
            self.pointer = (20, 22)

        self.step("Resume and validate a split CRLF record", split_packet, "POINTER 20 22 1")
        self.step("Open and scroll the dropdown, then select Settings",
                  lambda: (self.click(50, 77), self.wheel(50, 105, 1), self.click(50, 126)),
                  "MODE 2")
        self.step("Hide the inspector with the checkbox", lambda: self.click(25, 112), "VISIBLE 0")
        self.step("Restore the inspector with the button", lambda: self.click(60, 226),
                  "BUTTON 1", "VISIBLE 1")
        self.step("Select Percent in the radio group", lambda: self.click(24, 164), "UNITS 1")
        self.step("Drag the gain slider", lambda: self.drag((43, 204), (112, 204)), "GAIN 80")
        self.step("Select an inspector table cell", lambda: self.click(166, 97), "CELL 0 0")
        self.step("Scroll the table and select a different row",
                  lambda: (self.wheel(166, 108, 1), self.click(166, 112)), "CELL 2 0")
        self.step("Scroll the nested notes pane", lambda: self.wheel(170, 176, 1), "NOTES 20")
        self.step("Drag to pan the nested notes pane", lambda: self.drag((175, 185), (175, 163)), "NOTES 42")
        self.step("Move the inspector window", lambda: self.drag((160, 52), (144, 62)),
                  "WINDOW 132 18 164 188")
        self.step("Resize the inspector window", lambda: self.drag((293, 235), (280, 214)),
                  "WINDOW 132 18 151 167")
        self.step("Snap the inspector to the right edge", lambda: self.drag((142, 61), (317, 110)),
                  "WINDOW 160 0 160 204")
        self.step("Maximize the inspector", lambda: self.click(287, 44), "WINDOW 0 0 320 204")
        self.step("Restore the inspector size", lambda: self.click(287, 44), "WINDOW 132 18 151 167")
        self.step("Minimize the inspector", lambda: self.click(226, 61), "WINDOW 132 18 151 24")
        self.step("Restore the inspector height", lambda: self.click(226, 61), "WINDOW 132 18 151 167")
        self.step("Close the inspector from the title bar", lambda: self.click(273, 61), "VISIBLE 0")
        self.step("Open the inspector as a modal with a double-click",
                  lambda: (self.move(60, 226), self.send("double 60 226")), "MODAL 1")

        def try_modal_background():
            before = len(self.lines)
            self.click(25, 112)
            if any(line.startswith("VISIBLE ") for line in self.lines[before:]):
                raise ValueError("A background widget reacted while the inspector was modal")

        self.step("Check that the modal blocks a background click", try_modal_background, "POINTER 25 112 1")
        self.step("Close the modal", lambda: self.click(273, 61), "VISIBLE 0")
        self.step("Verify that the background accepts clicks again", lambda: self.click(25, 112), "VISIBLE 1")
        self.step("Switch to the nested form and calendar", lambda: self.click(159, 17), "TAB 1")
        self.step("Focus the text field, type Lab, then backspace",
                  lambda: (self.click(62, 63), self.send("text Lab"), self.send("key 8")),
                  "TEXT Lab", "TEXT La")
        self.step("Increment the spin box", lambda: self.click(135, 105), "QUANTITY 25")
        self.step("Enter 1 using the on-screen keypad", lambda: self.click(29, 139),
                  "KEYPAD 49", "TEXT La1")
        self.step("Advance the calendar and select December 1",
                  lambda: (self.click(298, 51), self.click(213, 91)), "DATE 2026-12-01")
        self.step("Open the image dialog from the form button", lambda: self.click(60, 225), "DIALOG 1")
        self.step("Type into the dialog initial focus", lambda: self.send("text !"), "DRAFT La1!")

        def blocked_dialog_background():
            before = len(self.lines)
            self.click(55, 17)
            self.send("key 8")
            if any(line.startswith("TAB ") for line in self.lines[before:]):
                raise ValueError("A background tab reacted while the dialog was open")

        self.step("Block a background tab and keep keyboard focus", blocked_dialog_background, "DRAFT La1")
        self.step("Edit the dialog text and quantity",
                  lambda: (self.send("text OK"), self.click(248, 143)),
                  "DRAFT La1OK", "DRAFT_QTY 30")
        self.step("Accept and apply the dialog values", lambda: self.click(233, 177),
                  "DIALOG_RESULT 1 La1OK 30", "APPLIED La1OK 30")
        self.step("Cancel edits without applying them",
                  lambda: (self.click(60, 225), self.send("text X"), self.click(80, 177)),
                  "DIALOG_RESULT 0 La1OKX 30", "APPLIED La1OK 30")
        self.step("Return an application-specific button result",
                  lambda: (self.click(60, 225), self.click(152, 177)),
                  "DIALOG_RESULT 7 La1OK 30", "APPLIED La1OK 30")
        self.step("Reject with Escape", lambda: (self.click(60, 225), self.send("key 27")),
                  "DIALOG_RESULT 0 La1OK 30")
        self.step("Reject with the dialog title-bar close button",
                  lambda: (self.click(60, 225), self.click(272, 50)),
                  "DIALOG_RESULT 0 La1OK 30")
        self.step("Accept with Enter when enabled",
                  lambda: (self.click(60, 225), self.send("text !"), self.send("key 13")),
                  "DIALOG_RESULT 1 La1OK! 30", "APPLIED La1OK! 30")
        self.step("Open the gallery and its nested buffered grid", lambda: self.click(260, 17), "TAB 2")
        self.step("Scroll a layout-sized list to its last row",
                  lambda: (self.wheel(38, 102, 1), self.wheel(38, 102, 1), self.click(38, 113)),
                  "GALLERY 4", "PROGRESS 100")
        self.step("Switch artwork to Fill with bilinear filtering",
                  lambda: (self.wheel(38, 102, -1), self.wheel(38, 102, -1), self.click(38, 93)),
                  "GALLERY 1", "PROGRESS 25")
        self.step("Switch artwork to Stretch with area filtering", lambda: self.click(38, 113),
                  "GALLERY 2", "PROGRESS 50")
        self.step("Toggle sampling in direct and buffered plots", lambda: self.click(31, 143), "PLOTS 16", "METRIC_WIDTH 74")
        self.step("Rotate and resize artwork through the grid button", lambda: self.click(270, 150),
                  "GALLERY 3", "PROGRESS 75", "TILE_BUTTON 1", "GRID_WIDTH 68")
        self.step("Send a double-click to the custom pixmap",
                  lambda: (self.move(185, 150), self.send("double 185 150")), "TILE_DOUBLE 2")
        self.step("Press and release with absolute pointer samples",
                  lambda: (self.send("pointer 185 150 1"), self.send("pointer 185 150 0")),
                  "TILE_RELEASE 2")
        self.step("Cancel a captured custom widget press",
                  lambda: (self.send("down 185 150"), self.send("cancel")), "TILE_CANCEL 2")
        self.step("Toggle sampling back", lambda: self.click(31, 143), "PLOTS 4", "METRIC_WIDTH 81")
        self.step("Return to the Controls tab", lambda: self.click(56, 17), "TAB 0")
        draws = [line for line in self.lines if line.startswith("POINTER ")]
        if len(draws) < 40 or any(not line.endswith(" 1") for line in draws):
            raise ValueError(f"Expected at least 40 rendered pointer positions, received {len(draws)}")
        seconds = time.monotonic() - start_time
        print(f"PASS on {self.wire.port}: all {self.checkpoints} checkpoints, {len(draws)} pointer redraws, "
              f"{seconds:.1f} seconds", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="Teensy USB Serial port, for example COM4")
    parser.add_argument("--pace", type=float, default=1.0,
                        help="Delay multiplier. The default is 1.0, and 2.0 is slower.")
    parser.add_argument("--verbose", action="store_true", help="Show the full serial trace")
    args = parser.parse_args()
    if not math.isfinite(args.pace) or args.pace <= 0:
        parser.error("--pace must be a positive finite number")
    try:
        with serial.Serial(args.port, 115200, timeout=0.10, write_timeout=2) as wire:
            Walkthrough(wire, args.pace, args.verbose).main()
    except (ValueError, serial.SerialException) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
