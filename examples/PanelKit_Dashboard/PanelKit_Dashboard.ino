#include <PanelKit.h>
#include <TeensyTFT.h>

TeensyTFT display(10, 9, 255);
Panel<280, 200, false> dashboard;
Panel<90, 55, false> controls(&dashboard);
PanelPlot<156, 72, 32> history(&dashboard);
PanelSlider rate(156, 20, &dashboard);
PanelProgress level(156, 10, &dashboard);
PanelToggle running(52, 22, &controls);
Pixmap<64, 32> spriteSheet;
Component<86, 86> dial(&dashboard);
PanelSprite<86, 86, 64, 32> needle(spriteSheet, {32, 0, 32, 32}, &dashboard);
TouchProcessor touch;
bool ready = false;
uint32_t lastFrame = 0;
float phase = 0;

void setup()
{
    Serial.begin(115200);
    ready = display.begin();
    if (!ready)
    {
        Serial.println("Display initialization failed.");
        return;
    }
    Serial.println("Drag the slider to change speed. Tap the switch to pause. Send c to calibrate.");
    dashboard.setLoc(20, 20);
    dashboard.setBackground(RGB565(17, 28, 42));
    dashboard.activate();
    history.setLoc(10, 10);
    BaseComponent* meters[] = {&rate, &level};
    panelStack(meters, 2, 10, 104, PanelAxis::VERTICAL, 14);
    rate.setValue(0.4f);
    rate.activate();

    controls.setLoc(180, 104);
    controls.setBackground(RGB565(27, 43, 59));
    controls.activate();
    running.setLoc(19, 16);
    running.setColors(RGB565(51, 61, 72), GREEN);
    running.setChecked(true);
    running.activate();

    // An atlas crop draws a dial. The neighboring sprite supplies the needle.
    spriteSheet.draw_circle(16, 16, 15, 1, false, CYAN, 1, Antialiasing::COVERAGE);
    spriteSheet.fill_triangle(46, 14, 61, 16, 46, 18, YELLOW);
    dial.setLoc(180, 10);
    dial.pixmap.blitRegion(spriteSheet, {0, 0, 32, 32}, {0, 0, 86, 86},
                           BlitMode::STRETCH, ScaleMode::BILINEAR);
    needle.setLoc(180, 10);
    dashboard.present(display);
}

void loop()
{
    if (!ready) return;
    if (Serial.read() == 'c') display.calibrateTouch();
    const bool pressed = display.touched();
    touch.process(pressed, display.touchX(), display.touchY());

    const uint32_t now = millis();
    if (running.isChecked() && now - lastFrame >= 50)
    {
        lastFrame = now;
        phase = fmodf(phase + 0.04f + 0.36f * rate.getValue(), float(2 * PI));
        const float reading = 0.5f + 0.46f * sinf(phase);
        history.append(reading);
        level.setValue(reading);
        BlitTransform pose(43, 43);
        pose.pivotX = pose.pivotY = 16;
        pose.scaleX = pose.scaleY = 2.1f;
        pose.angle = -130 + 260 * reading;
        needle.setPose(pose);
    }
    dashboard.present(display);
}
