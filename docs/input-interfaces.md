# PanelKit input interface reference

[← Back to PanelKit README](../README.md#documentation)

PanelKit accepts three kinds of input at different levels. A transport carries bytes, a driver or parser turns those bytes or samples into input, and `PanelInput` routes the resulting events to widgets.

| Interface | What it accepts | What it provides |
| --- | --- | --- |
| `PanelStreamInput` | An Arduino `Stream` or a byte source exposing `available()` and `read()`. Applications can also call `feed(byte)`. | Raw typing events or commands decoded from newline-delimited records |
| `PanelTouchInput` | A sampled touchscreen or a push-based driver | Pointer presses, drags, releases, and click gestures |
| `PanelMouseInput` | Relative motion, primary button state, and vertical wheel movement from a mouse or HID driver | Hover, pointer gestures, and wheel events. The adapter tracks its own cursor position. |
| `PanelInput` | Typed C++ calls from the application or a device adapter | A shared cursor, pointer arbitration, and PanelKit events |
| `widget.onInput()` and `getEventSignal()` | Events already decoded by PanelKit | Routed widget callbacks and a global signal for observing input |

## Connect more than one device

Use one `PanelInput` per UI, and give each adapter a distinct 8-bit source ID. The supplied IDs are 1 for touch, 2 for a stream, and 3 for software. Legacy helpers such as `click()` and `type()` emit source ID 0 directly on the event signal. Assign explicit IDs when creating additional adapters so they can be distinguished. Source IDs are local application identifiers. They are chosen in C++ and are not fields in the serial protocol.

```cpp
PanelInput input(8, 300, true);  // Drag threshold in pixels, double-click window in ms, deferred clicks
PanelTouchInput touch(input);
PanelStreamInput usb(input, PanelStreamMode::COMMANDS);
PanelMouseInput mouse(input, 4, 160, 120);

// After display initialization in setup():
input.setBounds(display.width(), display.height());
Serial.begin(115200);

// In loop():
touch.pollDisplay(display);
usb.poll(Serial, 64);
input.tick();
scene.present(display);

// In your own mouse report handler, called from loop():
mouse.report(dx, dy, primaryDown, wheelDown);

// Software uses the same router.
input.tap(PanelInput::SoftwareSource, 20, 40);
input.key('A');
```

Poll devices and call the router from the sketch loop. The first source to start a press owns the pointer until release or cancellation. While it holds the pointer, competing pointer presses, taps, and motion are rejected. Typing can still go to the focused widget, and wheel events can still target a screen location. Touch and mouse adapters ignore a physical contact that began during another source’s press until that contact lifts. A rejected command from a stream is not queued for later.

The press selects pointer capture and keyboard focus. Drag, release, and cancellation go to the captured widget even if the pointer leaves its bounds. Typing goes to the focused widget. Hover, wheel, and click events are hit-tested by location when they are routed. A configured popup or modal can affect which widget receives an event. See the [dialog guide](dialogs.md) for focus and dismissal behavior.

Call `tick()` even while devices are idle so pending clicks can expire. In deferred mode, a qualifying pair produces a double-click and a lone click is delivered after the window expires. In immediate mode, the first click is delivered on release, and a qualifying second click produces a double-click. Starting a press from a different source finishes the previous source’s click sequence, so two different devices do not form a double-click. An explicit serial `click` or `double` bypasses timing detection and emits press, release, and the requested click event.

### Touchscreen and mouse drivers

`PanelTouchInput::pollDisplay(display)` reads `touched()` once per poll and reads `touchX()` and `touchY()` only for contact samples. TeensyTFT supplies that interface, including driver-side touch calibration. For another controller, implement `bool readTouch(PanelTouchSample& sample)` and call `touch.poll(driver)`. Fill the sample with a pressed flag and calibrated screen coordinates. Return false when there is no new sample. A lift is a fresh sample with `pressed = false`, and its coordinates are ignored. The release uses the last routed contact position. Push-based drivers can instead call `touch.report(sample)`. The adapter’s return value indicates whether the sample reached the gesture tracker.

`PanelMouseInput::report(dx, dy, primaryDown, wheelDown)` accepts relative movement, a boolean primary button state, and an optional vertical wheel delta. Positive wheel deltas mean scroll down. Device-specific code provides the reports and translates keyboard inputs to bytes or other application actions. For example, a keyboard adapter can call `input.key(character, keyboardSource)`.

Call `touch.disconnect()` or `mouse.disconnect()` when the driver disconnects. Call `touch.disconnect()` before changing touch calibration. An active gesture is cancelled without a click. `touch.setEnabled(false)` also cancels the gesture and stops polling that adapter.

## USB Serial and stream protocol

Set the port to the rate configured by the sketch. The examples use 115200. In Serial Monitor, select **Newline** or **Both NL & CR** for Commands mode.

- **Text mode** emits one typing event per byte, including CR, LF, and control bytes. It does not parse commands or perform text encoding conversion. The receiving widget decides what to do with the bytes. `PanelTextField` supports printable ASCII and Backspace/Delete.
- **Commands mode** accepts one lowercase command per LF-terminated record. A trailing CR before LF is stripped. Commands and integer arguments use spaces or tabs as separators. Empty lines are ignored. Integers are signed decimal values representable by the board’s C++ `int`, with tighter limits where shown below. Coordinates are absolute, with the origin at the upper left. After `setBounds(width, height)`, pointer positions are clamped to those bounds.

- **Checked commands mode** uses the same command grammar inside an `@HH:payload\n` frame. This mode requires a valid CRC before it applies any part of a command. Use it for automated senders or transports where corruption matters.

| Command syntax | Effect |
| --- | --- |
| `down x y` | Start a primary-button press. If the same source already holds it, update the held position. |
| `move x y` | Update a held gesture or emit hover movement when no source holds the pointer. |
| `up` or `up x y` | Release a press owned by this source. Omitting coordinates uses its last position. Supplying coordinates updates the gesture before releasing. |
| `pointer x y state` | Send an absolute pointer sample. State 1 presses or moves while held. State 0 releases at the position, or moves an idle pointer. |
| `click x y` or `double x y` | Emit an explicit click or double-click at the position. |
| `scroll x y steps` | Send a signed wheel delta at the position. Positive scrolls down. Widgets decide how far they scroll. Zero emits no event. |
| `key byte` | Forward one byte from 0 through 255. For example, `key 8` sends Backspace. |
| `text payload` | Forward the bytes after the first separator. Extra spaces, tabs, and quotes in the payload are literal. A blank payload emits no events. |
| `cancel` | Cancel this source’s active press or pending click without producing a new click. |

A record can contain at most 95 bytes before LF, including any trailing CR. Records can span multiple reads. A NUL byte or an oversized record causes the parser to discard the record through the next LF. Unknown commands, invalid arguments, and rejected records increment `errorCount()`. Valid commands that cannot act because of capture or focus are not parser errors. Invalid records do not partially update the UI.

`poll(stream, budget)` consumes up to the given number of available bytes and returns the count consumed. Its default budget is 64. It does not wait for a record to finish. `reset()` discards partial input and cancels the adapter’s gesture. Call it when a transport disconnects or an external receive queue overruns. The error counter remains cumulative.

### CRC-8 framing

Send an ASCII `@`, two hexadecimal digits `HH`, a colon, the command payload, and LF. The hexadecimal digits can be uppercase or lowercase. CRLF is also accepted. Calculate **CRC-8/ATM** over the exact payload bytes, including any leading or trailing spaces. The prefix and line ending are outside the checksum. For example, send `@A8:click 20 40` followed by LF.

| CRC parameter | Value |
| --- | --- |
| Width and polynomial | 8 bits, `0x07` |
| Initial value and final XOR | Both `0x00` |
| Bit order | MSB first, with neither input nor output reflected |
| Standard check value | `"123456789"` → `0xF4` |

The 95-byte record limit includes the four-byte header and optional CR. A checked payload must have 1–91 bytes with LF, or 1–90 with CRLF. A missing header, bad hexadecimal digit, or mismatched CRC increments both `errorCount()` and `checksumErrorCount()`. Invalid command syntax, NUL, and overflow only increment the general counter. Both counters are cumulative. After a damaged record, the next complete line starts a new frame. If the damaged command was a release, send a valid release or `cancel` to clear the held pointer.

C++ senders can calculate the checksum with `panelCommandCrc8(payload, length)`. The walkthrough’s Python sender implements the same algorithm independently. PanelKit computes the receive CRC at the newline using a 16-byte lookup table. It does not allocate memory, delay, or retry. This detects every single-bit payload error, but it is an eight-bit integrity check. It provides no sender authentication or replay protection.

The command stream defines input only. The adapter does not echo commands or emit acknowledgments or errors back to the port. A sketch can expose its own status using widget callbacks or `errorCount()`. For concrete examples, see the [headless USB trace](../examples/PanelKit_USBInputTest/PanelKit_USBInputTest.ino) and [display walkthrough](../examples/PanelKit_SerialWalkthrough/PanelKit_SerialWalkthrough.ino). Their Python checkers use the sketch-specific trace output rather than a general PanelKit response protocol.

### Diagnostic tools

| Host tool | Compatible sketch | Serial output it checks |
| --- | --- | --- |
| `check_usb_input.py PORT [--verbose]` | [PanelKit_USBInputTest](../examples/PanelKit_USBInputTest/PanelKit_USBInputTest.ino) | Ready banner, `RAW` events, `ENTRY` and `PAD` routing traces, `TEXT` state, and `ERRORS` count |
| `check_gui.py PORT [--pace MULTIPLIER] [--verbose]` | [PanelKit_SerialWalkthrough](../examples/PanelKit_SerialWalkthrough/PanelKit_SerialWalkthrough.ino) | Display-ready status, CRC rejection and recovery, widget state, window geometry, and pointer positions submitted for redraw |

Both checkers require `pyserial` and exclusive access to the serial port. Close Serial Monitor before running either checker. The GUI walkthrough uses a pace of 1.0 by default. Set it to 2.0 to double the delays. Both return a nonzero exit code if a check fails. The walkthrough verifies the firmware’s redraw submissions over serial. You can also watch the TFT for visual confirmation.

## Observe the resulting events

Use `widget.onInput(callback)` to observe events routed to that widget. It takes a `const Event&` and runs before the widget’s normal behavior. Coordinates at that layer are relative to the widget’s parent content, including any scroll offset. For an application-wide trace, attach a Callback `FunctionSlot<Event>` or `MethodSlot` to `getEventSignal()`. The raw signal supplies screen coordinates. A raw `TYPE` event from `PanelInput::key()` uses `(-1, -1)` as a placeholder, since focus determines its destination.

| Event field | Meaning |
| --- | --- |
| `type` | `PRESS`, `RELEASE`, `CLICK`, `DOUBLE_CLICK`, `DRAG`, `MOVE`, `SCROLL`, `TYPE`, or `CANCEL` |
| `source` | The source ID supplied by the adapter or software caller |
| `timestamp` | The event’s timestamp from Arduino `micros()` |
| `startX`, `startY`, `endX`, `endY` | Positions in the coordinate space of the observer. Drag events carry the movement segment. |
| `character` | A byte for `TYPE`. Cast to `uint8_t` to interpret its numeric value above 127. |
| `direction` | Wheel delta for `SCROLL` or angle in degrees for `DRAG` |

## Choose the right routing layer

Independent devices each have an adapter and share one PanelKit router. For example, USB Serial bytes go to `PanelStreamInput` while an SPI touchscreen supplies `PanelTouchInput`. PanelKit decides what each event does in the interface.

StreamSwitchboard is optional and sits before the parser when several decoders need to inspect one Arduino `Stream`. It fans out bytes to separate endpoints and can select which endpoint writes back. Give the PanelKit parser a switchboard endpoint when appropriate, and let the switchboard own reads from the underlying transport. If the same bytes could trigger both decoders, select the protocol first or frame the traffic so only the intended decoder can act. A failed or overflowed reader can affect the shared stream according to the switchboard’s configured policy.

PanelKit’s newline-delimited parser is part of the library. If several products later share a binary protocol, a separate decoder can own packet framing, versions, checksums, and resynchronization. It can call `PanelInput` after validating a packet. The router still owns pointer capture, focus, and gestures. StreamSwitchboard can distribute transport bytes independently of that choice.

## PanelKit Studio exports

In the editor, **Screen → Input devices** controls touch, click timing, the serial port, baud rate, byte budget, and protocol mode. The starter calls `beginInputs()` once, then `pollTouch(display)`, `pollInputs()`, `update()`, and `present(display)` in the loop. `update()` services deferred input as well as any configured frame code. The exported `panelkit_ui::input` is available to custom C++ adapters. If a generated source is disabled, its polling function does nothing. See the [Studio export guide](../designer/README.md#input-devices) for the settings and hooks.
