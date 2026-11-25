"""Drive a PanelKit USB Input Test sketch over its USB serial port."""

import argparse
import sys
import time

import serial


def read_until(port, marker, seconds, verbose):
    lines = []
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        raw = port.read_until(b"\n", 1024)
        if not raw:
            continue
        line = raw.decode("ascii", "replace").rstrip("\r\n")
        lines.append(line)
        if verbose:
            print(line)
        if line == marker:
            return lines
    raise ValueError(f"No {marker!r} from {port.port}. Received {lines!r}")


def check(actual, expected, label):
    if actual != expected:
        raise ValueError(f"{label}: expected {expected!r}, received {actual!r}")


def test(port, verbose):
    with serial.Serial(port, 115200, timeout=0.25, write_timeout=2) as wire:
        read_until(wire, "PANELKIT_USB_INPUT_READY", 8, verbose)
        # Split a command across USB writes and vary the line endings.
        wire.write(b"cli")
        wire.flush()
        time.sleep(0.05)
        wire.write(
            b"ck 12 12\r\ntext Hi\nkey 8\nkey 33\n"
            b"move 52 12\ndown 52 12\nmove 83 40\nup 96 55\n"
            b"scroll 52 12 -2\ndouble 52 12\ndown 52 12\ncancel\n"
            b"click 12 12 trailing\n" + b"x" * 110 + b"\n"
            b"click 12 12\ntext Z\n"
        )
        wire.flush()
        lines = read_until(wire, "TEXT\tH!Z", 8, verbose)

    def events(channel):
        return [line.split("\t")[1:] for line in lines if line.startswith(channel + "\t")]

    entry = events("ENTRY")
    pad = events("PAD")
    raw = events("RAW")
    check([event[0] for event in entry],
          ["PRESS", "RELEASE", "CLICK", "TYPE", "TYPE", "TYPE", "TYPE",
           "PRESS", "RELEASE", "CLICK", "TYPE"], "focused widget events")
    check([int(event[6]) for event in entry if event[0] == "TYPE"],
          [72, 105, 8, 33, 90], "keyboard bytes")
    check([line for line in lines if line.startswith("TEXT\t")],
          ["TEXT\tH", "TEXT\tHi", "TEXT\tH", "TEXT\tH!", "TEXT\tH!Z"], "text-field state")
    check([event[0] for event in pad],
          ["MOVE", "PRESS", "DRAG", "DRAG", "RELEASE", "SCROLL",
           "PRESS", "RELEASE", "DOUBLE_CLICK", "PRESS", "CANCEL"], "mouse widget events")
    check([event[2:6] for event in pad if event[0] == "DRAG"],
          [["52", "12", "83", "40"], ["83", "40", "96", "55"]], "captured drag positions")
    check([event[4:6] for event in pad if event[0] == "RELEASE"],
          [["96", "55"], ["52", "12"]], "release positions")
    check([event[7] for event in pad if event[0] == "SCROLL"], ["-2.00"], "scroll direction")
    check({event[1] for event in entry + pad + raw}, {"2"}, "USB serial event source")
    check([event[0] for event in raw if event[0] in ("CLICK", "DOUBLE_CLICK", "CANCEL")],
          ["CLICK", "DOUBLE_CLICK", "CANCEL", "CLICK"], "global signal")
    check([line for line in lines if line.startswith("ERRORS\t")][-1:], ["ERRORS\t2"],
          "malformed and oversized record recovery")
    print(f"PASS on {port}: keyboard focus, Backspace, click, hover, drag capture, release, "
          "wheel, double-click, cancellation, source IDs, and parser recovery")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="USB serial port, for example COM4")
    parser.add_argument("--verbose", action="store_true", help="Print the firmware trace")
    args = parser.parse_args()
    try:
        test(args.port, args.verbose)
    except (ValueError, serial.SerialException) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
