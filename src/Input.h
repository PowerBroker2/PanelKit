#pragma once
#include "Events.h"

// Main-loop input routing. One source owns the pointer from press through release.
class PanelInput
{
    TouchProcessor tracker;
    int x = 0, y = 0;
    int width = INT_MAX, height = INT_MAX;
    uint8_t owner = 0, recent = 0;
    bool held = false, dispatchingPointer = false, trackerUsed = false;

    static void emit(EventType type, uint8_t source, int x, int y,
                     char character = 0, float direction = 0)
    {
        const Event event{micros(), type, character, x, y, x, y, direction, source};
        getEventSignal().fire(event);
    }

    void selectSource(uint8_t source)
    {
        if (source != recent) tracker.finishClickSequence();
        recent = source;
        tracker.setEventSource(source);
    }

public:
    enum : uint8_t { TouchSource = 1, StreamSource = 2, SoftwareSource = 3 };

    explicit PanelInput(int dragThreshold = 10, unsigned long doubleClickMs = 300,
                        bool deferredClicks = false)
        : tracker(true, dragThreshold, doubleClickMs, deferredClicks) {}

    TouchProcessor& gestures() { return tracker; }
    int cursorX() const { return x; }
    int cursorY() const { return y; }
    int clampX(int pos) const { return max(0, min(width - 1, pos)); }
    int clampY(int pos) const { return max(0, min(height - 1, pos)); }
    void setBounds(int w, int h)
    {
        width = max(1, w);
        height = max(1, h);
        x = clampX(x);
        y = clampY(y);
    }

    // Touch drivers may return invalid coordinates after lift. Release preserves the last sample.
    bool pointer(uint8_t source, bool down, int px, int py)
    {
        if (dispatchingPointer || (held && owner != source) || (!held && !down)) return false;
        dispatchingPointer = true;
        if (down)
        {
            selectSource(source);
            owner = source;
            trackerUsed = true;
            held = true;
            x = clampX(px);
            y = clampY(py);
        }
        else held = false;
        tracker.process(down, x, y);
        dispatchingPointer = false;
        return true;
    }

    bool move(uint8_t source, int px, int py)
    {
        if (dispatchingPointer || (held && owner != source)) return false;
        if (held) return pointer(source, true, px, py);
        const int nextX = clampX(px), nextY = clampY(py);
        if (nextX == x && nextY == y) return true;
        x = nextX;
        y = nextY;
        emit(EventType::MOVE, source, x, y);
        return true;
    }

    bool releaseAt(uint8_t source, int px, int py)
    {
        return held && owner == source && pointer(source, true, px, py) &&
               pointer(source, false, px, py);
    }

    bool cancel(uint8_t source)
    {
        if (dispatchingPointer || (held ? owner != source : recent != source)) return false;
        dispatchingPointer = true;
        held = false;
        tracker.cancel();
        dispatchingPointer = false;
        return true;
    }

    bool tap(uint8_t source, int px, int py, bool twice = false)
    {
        if (held || dispatchingPointer) return false;
        dispatchingPointer = true;
        tracker.finishClickSequence();
        x = clampX(px);
        y = clampY(py);
        emit(EventType::PRESS, source, x, y);
        emit(EventType::RELEASE, source, x, y);
        emit(twice ? EventType::DOUBLE_CLICK : EventType::CLICK, source, x, y);
        dispatchingPointer = false;
        return true;
    }

    void key(char value, uint8_t source = SoftwareSource)
    {
        emit(EventType::TYPE, source, -1, -1, value);
    }
    void wheel(uint8_t source, int px, int py, float steps)
    {
        if (isfinite(steps) && steps != 0)
            emit(EventType::SCROLL, source, clampX(px), clampY(py), 0, steps);
    }
    void tick()
    {
        if (dispatchingPointer || !trackerUsed) return;
        dispatchingPointer = true;
        tracker.process(held, x, y);
        dispatchingPointer = false;
    }
};

struct PanelTouchSample
{
    bool pressed;
    int x, y; // Calibrated screen coordinates when pressed.
};

// A touchscreen adapter. Drivers provide samples; PanelKit owns gesture handling.
class PanelTouchInput
{
    PanelInput& input;
    const uint8_t source;
    bool enabled = true, reportedDown = false, acceptedPress = false;

public:
    explicit PanelTouchInput(PanelInput& input, uint8_t source = PanelInput::TouchSource)
        : input(input), source(source) {}

    bool isEnabled() const { return enabled; }
    void setEnabled(bool value)
    {
        if (enabled == value) return;
        enabled = value;
        if (!enabled) disconnect();
    }
    void disconnect()
    {
        reportedDown = acceptedPress = false;
        input.cancel(source);
    }

    // Returns true when the sample reaches the gesture tracker, including accepted lifts.
    // A contact rejected while another source holds capture stays rejected until lift.
    bool report(const PanelTouchSample& sample)
    {
        if (!enabled) return false;
        bool routed = false;
        if (sample.pressed)
        {
            if (!reportedDown || acceptedPress)
                acceptedPress = routed = input.pointer(source, true, sample.x, sample.y);
        }
        else if (acceptedPress)
        {
            routed = input.pointer(source, false, 0, 0);
            acceptedPress = false;
        }
        reportedDown = sample.pressed;
        return routed;
    }

    // A false return from readTouch means no new sample. Report lift with pressed=false.
    template <typename Driver>
    bool poll(Driver& driver)
    {
        if (!enabled) return false;
        PanelTouchSample sample{};
        return driver.readTouch(sample) && report(sample);
    }

    // Convenience bridge for TeensyTFT and other displays with this polling API.
    template <typename Display>
    bool pollDisplay(Display& display)
    {
        if (!enabled) return false;
        const bool pressed = display.touched();
        return report(pressed ? PanelTouchSample{true, int(display.touchX()), int(display.touchY())}
                              : PanelTouchSample{false, 0, 0});
    }
};

// Device-independent wrapper for relative mouse or HID reports.
class PanelMouseInput
{
    PanelInput& input;
    uint8_t source;
    int x, y;
    bool reportedDown = false, acceptedPress = false;

    static int add(int value, int delta)
    {
        const int64_t sum = int64_t(value) + delta;
        return int(max(int64_t(INT_MIN), min(int64_t(INT_MAX), sum)));
    }

public:
    PanelMouseInput(PanelInput& input, uint8_t source, int x = 0, int y = 0)
        : input(input), source(source), x(x), y(y) {}
    void report(int dx, int dy, bool primaryDown, float wheelDown = 0)
    {
        x = input.clampX(add(x, dx));
        y = input.clampY(add(y, dy));
        if (primaryDown)
        {
            if (!reportedDown || acceptedPress) acceptedPress = input.pointer(source, true, x, y);
        }
        else if (acceptedPress)
        {
            input.releaseAt(source, x, y);
            acceptedPress = false;
        }
        else input.move(source, x, y);
        reportedDown = primaryDown;
        input.wheel(source, x, y, wheelDown);
    }
    void disconnect()
    {
        reportedDown = acceptedPress = false;
        input.cancel(source);
    }
};

// CHECKED_COMMANDS requires @HH: followed by the command and a newline.
enum class PanelStreamMode { TEXT, COMMANDS, CHECKED_COMMANDS };

/** CRC-8/ATM over exactly length payload bytes, excluding framing. */
uint8_t panelCommandCrc8(const char* payload, size_t length);

// Accepts Arduino Stream objects and other sources exposing available() and read().
class PanelStreamInput
{
    PanelInput& input;
    const uint8_t source;
    PanelStreamMode mode;
    char line[96];
    size_t size = 0;
    bool discard = false;
    unsigned long errors = 0, checksumErrors = 0;

    bool processLine(char* command);
    bool validateChecksum();

public:
    PanelStreamInput(PanelInput& input, PanelStreamMode mode = PanelStreamMode::COMMANDS,
                     uint8_t source = PanelInput::StreamSource)
        : input(input), source(source), mode(mode) {}

    void setMode(PanelStreamMode next)
    {
        if (next == mode) return;
        reset();
        mode = next;
    }
    void reset()
    {
        size = 0;
        discard = false;
        input.cancel(source);
    }
    /** Cumulative rejected records, including checksum failures. */
    unsigned long errorCount() const { return errors; }
    /** Subset rejected for a missing, malformed, or mismatched checksum. */
    unsigned long checksumErrorCount() const { return checksumErrors; }
    /** Supply a byte without waiting. Commands are routed only after a valid newline. */
    void feed(uint8_t byte);

    /** Read at most budget available bytes. A partial record is retained across calls.
     * Event handlers execute synchronously on this call stack.
     */
    template <typename ByteSource>
    size_t poll(ByteSource& stream, size_t budget = 64)
    {
        size_t consumed = 0;
        while (consumed < budget && stream.available() > 0)
        {
            const int next = stream.read();
            if (next < 0) break;
            feed(uint8_t(next));
            ++consumed;
        }
        return consumed;
    }
};
