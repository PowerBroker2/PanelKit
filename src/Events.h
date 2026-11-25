#pragma once
#include <Arduino.h>
#include <limits.h>
#include <Callback.h>

constexpr float PanelRadiansToDegrees = 57.2957795f;

enum class EventType { CLICK,
                       DOUBLE_CLICK,
                       SCROLL,
                       TYPE,
                       PRESS,
                       RELEASE,
                       DRAG,
                       MOVE,
                       CANCEL };

struct Event
{
    unsigned long timestamp;
    EventType     type;
    char          character;
    int           startX;
    int           startY;
    int           endX;
    int           endY;
    float         direction;
    uint8_t       source; // 0 for legacy event helpers
};

// Declare that the signal exists globally
// extern Signal<Event> eventOccured;
Signal<Event>& getEventSignal();

// Declare function prototypes (Note: Default parameters must remain in the header)
void click(int x, int y);
void doubleClick(int x, int y);
void scroll(int x, int y, float direction);
void type(char c, int x = -1, int y = -1);
void press(int x, int y);
void release(int x, int y);
void drag(int startX, int startY, int endX, int endY);

// Helper to convert EventType enum values to human-readable strings
const char* eventTypeToString(EventType type);

class TouchProcessor
{
private:
    // Gesture tracking state
    bool          wasTouched;
    int           startX, startY;
    int           lastX, lastY;
    bool          isDragging;
    
    // Double-click tracking state
    bool          hasPendingClick;
    unsigned long lastClickTime;    // Timestamp in microseconds
    int           lastClickX, lastClickY;
    int           lastClickStartX = 0, lastClickStartY = 0;

    // Threshold configurations
    int           dragThresholdPixels;
    double        dragThresholdSq;
    unsigned long doubleClickTimeUs; // Timeout in microseconds

    // Mode configurations
    bool          autoEmit;
    bool          deferredClickMode; // false = Immediate Mode, true = Deferred (Mutually Exclusive)

    // Internal fixed buffer for frame events (no dynamic allocation)
    static constexpr size_t MAX_BUFFER_SIZE = 4;
    Event  eventBuffer[MAX_BUFFER_SIZE];
    size_t eventCount;
    uint8_t eventSource = 0;

    // Internal helper to construct Event objects
    Event createEvent(EventType type, int sX, int sY, int eX, int eY, float dir = 0.0f) const
    {
        Event e;
        e.timestamp = micros();
        e.type      = type;
        e.character = '\0';
        e.startX    = sX;
        e.startY    = sY;
        e.endX      = eX;
        e.endY      = eY;
        e.direction = dir;
        e.source    = eventSource;
        return e;
    }

    // Add event to buffer safely
    void queueEvent(const Event& e)
    {
        if (eventCount < MAX_BUFFER_SIZE)
        {
            eventBuffer[eventCount++] = e;
        }
    }

public:
    TouchProcessor(bool autoEmitEvents = true, int dragThreshold = 10, unsigned long doubleClickMs = 300, bool deferredClicks = false)
        : wasTouched(false),
          startX(0), startY(0),
          lastX(0), lastY(0),
          isDragging(false),
          hasPendingClick(false),
          lastClickTime(0),
          lastClickX(0), lastClickY(0),
          dragThresholdPixels(0),
          dragThresholdSq(0),
          doubleClickTimeUs(0),
          autoEmit(autoEmitEvents),
          deferredClickMode(deferredClicks),
          eventCount(0)
    { setDragThreshold(dragThreshold); setDoubleClickTime(doubleClickMs); }

    // --- Configuration Setters/Getters ---
    void setAutoEmit(bool enable) { autoEmit = enable; }
    bool getAutoEmit() const { return autoEmit; }
    
    // Toggle between Immediate (false) and Deferred (true) click modes
    void setDeferredClickMode(bool enable) 
    { 
        // If turning off deferred mode while a click is pending, flush it immediately
        if (!enable && deferredClickMode && hasPendingClick)
        {
            queueEvent(createEvent(EventType::CLICK, lastClickStartX, lastClickStartY, lastClickX, lastClickY));
            hasPendingClick = false;
            if (autoEmit) emitPendingEvents();
        }
        deferredClickMode = enable; 
    }
    bool getDeferredClickMode() const { return deferredClickMode; }

    void setDragThreshold(int pixels) 
    { 
        dragThresholdPixels = max(0, pixels);
        dragThresholdSq = double(dragThresholdPixels) * dragThresholdPixels;
    }
    int getDragThreshold() const { return dragThresholdPixels; }

    void setDoubleClickTime(unsigned long ms) { doubleClickTimeUs = min(ms, ULONG_MAX / 1000UL) * 1000UL; }
    unsigned long getDoubleClickTime() const { return doubleClickTimeUs / 1000UL; }

    // --- Buffer Access Methods ---
    size_t getEventCount() const { return eventCount; }
    bool hasEvents() const { return eventCount > 0; }

    const Event* getEvent(size_t index) const
    {
        if (index < eventCount) return &eventBuffer[index];
        return nullptr;
    }

    bool popEvent(Event& outEvent)
    {
        if (eventCount == 0) return false;
        outEvent = eventBuffer[0];

        for (size_t i = 1; i < eventCount; ++i)
        {
            eventBuffer[i - 1] = eventBuffer[i];
        }
        eventCount--;
        return true;
    }

    void emitPendingEvents()
    {
        Event pending[MAX_BUFFER_SIZE];
        const size_t count = eventCount;
        for (size_t i = 0; i < count; ++i) pending[i] = eventBuffer[i];
        eventCount = 0;
        for (size_t i = 0; i < count; ++i) getEventSignal().fire(pending[i]);
    }

    void clearEvents()
    {
        eventCount = 0;
    }

    void setEventSource(uint8_t source) { eventSource = source; }
    void finishClickSequence()
    {
        const bool emitClick = deferredClickMode && hasPendingClick;
        hasPendingClick = false;
        if (emitClick)
        {
            queueEvent(createEvent(EventType::CLICK, lastClickStartX, lastClickStartY, lastClickX, lastClickY));
            if (autoEmit) emitPendingEvents();
        }
    }
    void cancel()
    {
        const bool held = wasTouched;
        wasTouched = isDragging = hasPendingClick = false;
        eventCount = 0;
        if (held) queueEvent(createEvent(EventType::CANCEL, lastX, lastY, lastX, lastY));
        if (autoEmit) emitPendingEvents();
    }

    // --- Main Gesture State Machine ---
    void process(bool isTouched, int x, int y)
    {
        eventCount = 0;
        const unsigned long now = micros();

        // 1. TOUCH STARTED (UNTOUCHED -> TOUCHED)
        if (isTouched && !wasTouched)
        {
            wasTouched = true;
            startX     = x;
            startY     = y;
            lastX      = x;
            lastY      = y;
            isDragging = false;

            // In deferred mode, flush pending click if new touch starts far away
            if (deferredClickMode && hasPendingClick)
            {
                const double clickDistX = double(x) - lastClickX, clickDistY = double(y) - lastClickY;
                const double clickDistSq = clickDistX * clickDistX + clickDistY * clickDistY;

                if (clickDistSq > dragThresholdSq)
                {
                    queueEvent(createEvent(EventType::CLICK, lastClickStartX, lastClickStartY, lastClickX, lastClickY));
                    hasPendingClick = false;
                }
            }

            queueEvent(createEvent(EventType::PRESS, x, y, x, y));
        }
        // 2. TOUCH CONTINUING (Evaluate Drag)
        else if (isTouched && wasTouched)
        {
            const bool wasDragging = isDragging;
            if (!isDragging)
            {
                const double dx = double(x) - startX, dy = double(y) - startY;
                
                if ((dx * dx + dy * dy) > dragThresholdSq)
                {
                    isDragging = true;
                    
                    if (deferredClickMode && hasPendingClick)
                    {
                        queueEvent(createEvent(EventType::CLICK, lastClickStartX, lastClickStartY, lastClickX, lastClickY));
                        hasPendingClick = false;
                    }
                }
            }

            if (isDragging)
            {
                const int originX = wasDragging ? lastX : startX;
                const int originY = wasDragging ? lastY : startY;
                if (x != originX || y != originY)
                {
                    const float dir = atan2f(float(y) - originY, float(x) - originX) * PanelRadiansToDegrees;
                    
                    queueEvent(createEvent(EventType::DRAG, originX, originY, x, y, dir));
                }
            }
            lastX = x;
            lastY = y;
        }
        // 3. TOUCH RELEASED (TOUCHED -> UNTOUCHED)
        else if (!isTouched && wasTouched)
        {
            wasTouched = false;
            queueEvent(createEvent(EventType::RELEASE, lastX, lastY, lastX, lastY));

            if (!isDragging)
            {
                const double clickDistX = double(lastX) - lastClickX, clickDistY = double(lastY) - lastClickY;
                const double clickDistSq = clickDistX * clickDistX + clickDistY * clickDistY;

                if (hasPendingClick && 
                    (now - lastClickTime <= doubleClickTimeUs) && 
                    (clickDistSq <= dragThresholdSq))
                {
                    // Confirmed DOUBLE CLICK -> Emit double click and clear pending state
                    queueEvent(createEvent(EventType::DOUBLE_CLICK, startX, startY, lastX, lastY));
                    hasPendingClick = false;
                }
                else
                {
                    if (deferredClickMode)
                    {
                        // Deferred Mode: Flush older pending click, store new one for timeout
                        if (hasPendingClick)
                        {
                            queueEvent(createEvent(EventType::CLICK, lastClickStartX, lastClickStartY, lastClickX, lastClickY));
                        }
                        hasPendingClick = true;
                        lastClickTime   = now;
                        lastClickX      = lastX;
                        lastClickY      = lastY;
                        lastClickStartX = startX;
                        lastClickStartY = startY;
                    }
                    else
                    {
                        // Immediate Mode: Fire CLICK right now, store metadata for double-click check
                        queueEvent(createEvent(EventType::CLICK, startX, startY, lastX, lastY));
                        hasPendingClick = true;
                        lastClickTime   = now;
                        lastClickX      = lastX;
                        lastClickY      = lastY;
                    }
                }
            }
        }

        // 4. TIMED EXPIRATION (Deferred mode only)
        if (deferredClickMode && hasPendingClick && (now - lastClickTime > doubleClickTimeUs))
        {
            queueEvent(createEvent(EventType::CLICK, lastClickStartX, lastClickStartY, lastClickX, lastClickY));
            hasPendingClick = false;
        }

        if (autoEmit)
        {
            emitPendingEvents();
        }
    }
};
