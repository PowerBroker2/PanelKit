#include "TeensyTFT.h"
#include "PanelKit.h"
#include "Button.h"
#include "font_roboto_regular_50.h"

TeensyTFT display(10, 9, 255);
Panel<200, 200> panel;

PanelButton<120, 50> btnOk;
PanelButton<120, 50> btnCancel;

TouchProcessor touchProcessor;

void onOkClicked() {
    Serial.println("OK Button Clicked!");
}

void onCancelClicked() {
    Serial.println("Cancel Button Clicked!");
}

void setup() {
    Serial.begin(115200);
    display.begin();

    // 1. Configure and ACTIVATE the parent panel container
    panel.setName("mainPanel");
    panel.setLoc(0, 0);
    panel.activate();

    // 2. Configure OK Button
    btnOk.setName("btnOk");
    btnOk.setLoc(40, 20);
    btnOk.link(&panel);
    btnOk.setText("OK")
         .setFont(font_roboto_regular_50::Font, 0.25f)
         .setJustification(TextJustify::CENTER, TextVAlign::MIDDLE)
         .setColors(0x03E0, 0xFFFF, 0x02C0) // Green background
         .onClick(onOkClicked);
    btnOk.activate();

    // 3. Configure Cancel Button
    btnCancel.setName("btnCancel");
    btnCancel.setLoc(40, 90);
    btnCancel.link(&panel);
    btnCancel.setText("Cancel")
             .setFont(font_roboto_regular_50::Font, 0.20f)
             .setJustification(TextJustify::CENTER, TextVAlign::MIDDLE)
             .setColors(0xF800, 0xFFFF, 0xA000) // Red background
             .onClick(onCancelClicked);
    btnCancel.activate();

    display.clear();

    // Enable Mutually Exclusive Mode (single clicks wait 300ms to verify no double click happens)
    touchProcessor.setDeferredClickMode(true);

    // Or set it to Immediate Mode (instant response, double clicks fire after single click)
    // touchProcessor.setDeferredClickMode(false);
}

void loop() {
    // Process incoming hardware touch coordinates
    touchProcessor.process(display.touched(),
                           display.touchX(),
                           display.touchY());

    // Update internal button state and redraw pixmaps if dirty
    btnOk.update();
    btnCancel.update();

    // Blit pixel memory onto TeensyTFT screen hardware (only draws when dirty)
    btnOk.drawTo(display);
    btnCancel.drawTo(display);

    display.swap();
}