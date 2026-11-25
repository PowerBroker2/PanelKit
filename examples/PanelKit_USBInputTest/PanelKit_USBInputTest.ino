#include <PanelKit.h>

Panel<160, 90, false> screen;
PanelTextField entry(34, 20, &screen);
Component<28, 20> mousePad(&screen);
PanelInput input(4);
PanelStreamInput commands(input, PanelStreamMode::COMMANDS);

void printEvent(const char* channel, const Event& event)
{
    Serial.print(channel);
    Serial.print('\t');
    Serial.print(eventTypeToString(event.type));
    Serial.print('\t');
    Serial.print(event.source);
    Serial.print('\t');
    Serial.print(event.startX);
    Serial.print('\t');
    Serial.print(event.startY);
    Serial.print('\t');
    Serial.print(event.endX);
    Serial.print('\t');
    Serial.print(event.endY);
    Serial.print('\t');
    Serial.print(uint8_t(event.character));
    Serial.print('\t');
    Serial.println(event.direction, 2);
}

void traceRaw(Event event) { printEvent("RAW", event); }
void traceEntry(const Event& event) { printEvent("ENTRY", event); }
void tracePad(const Event& event) { printEvent("PAD", event); }

void traceText(const char* value)
{
    Serial.print("TEXT\t");
    Serial.println(value);
}

FunctionSlot<Event> rawTrace(traceRaw);

void setup()
{
    Serial.begin(115200);
    screen.activate();
    entry.setLoc(8, 8);
    entry.onInput(traceEntry);
    entry.onChange(traceText);
    entry.activate();
    mousePad.setLoc(50, 8);
    mousePad.onInput(tracePad);
    mousePad.activate();
    input.setBounds(160, 90);
    getEventSignal().attach(rawTrace);
}

void loop()
{
    static bool connected = false;
    static unsigned long baseline = 0;
    static unsigned long reported = 0;
    if (!Serial)
    {
        if (connected)
        {
            commands.reset();
            connected = false;
        }
        return;
    }
    if (!connected)
    {
        entry.setText("");
        baseline = commands.errorCount();
        reported = 0;
        connected = true;
        Serial.println("PANELKIT_USB_INPUT_READY");
    }
    commands.poll(Serial, 16);
    input.tick();
    const unsigned long errors = commands.errorCount() - baseline;
    if (errors != reported)
    {
        reported = errors;
        Serial.print("ERRORS\t");
        Serial.println(errors);
    }
}
