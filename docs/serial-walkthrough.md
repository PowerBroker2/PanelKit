# Serial walkthrough coverage

[← Back to PanelKit README](../README.md#documentation)

[PanelKit_SerialWalkthrough](../examples/PanelKit_SerialWalkthrough/PanelKit_SerialWalkthrough.ino) is a Teensy 4.1 integration fixture for the native input router and the TFT. It receives CRC-protected commands over USB Serial and reports state changes to [check_gui.py](../examples/PanelKit_SerialWalkthrough/check_gui.py). The host checks distinct checkpoints and asserts that cursor movement submitted display redraws. The white pointer and brief pauses make the sequence visible on the TFT.

From the PanelKit root, after uploading with **USB Type → Serial**, close Serial Monitor and run:

```sh
python examples/PanelKit_SerialWalkthrough/check_gui.py COM4
```

Replace `COM4` with the actual port. The checker requires `pyserial`. Use `--pace 2` for twice the delay or `--verbose` for the board trace. See the [wire protocol](input-interfaces.md#crc-8-framing) for custom senders. The [headless USB diagnostic](../examples/PanelKit_USBInputTest/PanelKit_USBInputTest.ino) still exercises the plain command mode.

## Widget and feature matrix

The scene instantiates every widget type available in Studio. Interactive checks use real routed touch-style pointer or text events over USB Serial. Passive widgets are exercised through their linked state changes or their containing layout.

| Widgets | Combinations exercised |
| --- | --- |
| Panel, Flex, grid layout, Window | Unbuffered root and pages, buffered grid reflow, horizontal and vertical flex reflow, nested window client layout and scroll area, move, resize, side snap, maximize, restore, minimize, close, reopen, and modal capture |
| Tabs, Text, Label | Navigation among controls, nested form, and gallery. Dropdown, slider, and calendar update text. The gallery renders a bitmap label. |
| Button, Check Box, Radio Group, Combo Box, Slider | Buffered and low-memory button captions, checkbox linked to window visibility, radio selection, popup wheel scrolling and selection, a captured stepped slider drag |
| Table, Scroll Area, List | Table selection before and after wheel scrolling, wheel scrolling and drag panning in a nested viewport, wheel scrolling to the last row of a resized list |
| Dialog | Image and message, automatic input focus, rejection of background clicks, save and cancel outcomes, a custom result, Escape, Enter, and the title-bar close button |
| Text Field, Spin Box, Keypad, Calendar | USB keyboard focus and Backspace, touch-style increment, on-screen keypad writing to an existing field, month navigation and date selection |
| Plot, Toggle, Progress | Toggle changes the sample set for both direct and buffered plots. List and button actions update the progress value. |
| Picture, Sprite, Pixmap | Fit, Fill, Stretch, nearest, bilinear, and area sampling, sprite rotation and scaling, a nested pixmap receiving double-click, release, and cancellation |

USB checks include invalid CRCs, a missing prefix, overflow, a checked command split across reads, CRLF, recovery at the next valid record, and pointer and key commands in one session. The fixture checks a finite set of widget, layout, and input interactions. Arbitrary nesting, themes, transforms, dimensions, data, and input timing make the full combination space unbounded. The checker observes the firmware’s state and redraw submissions. The TFT can be watched to verify the physical output.
