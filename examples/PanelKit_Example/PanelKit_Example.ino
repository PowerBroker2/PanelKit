#include "TeensyTFT.h"
#include "PanelKit.h"
#include "Button.h"
#include "font_roboto_regular_50.h"

TeensyTFT display(10, 9, 255);
Panel<320, 240, false> panel;

PanelButton<116, 42> btnOk(&panel);
PanelButton<116, 42> btnCancel(&panel);
PanelLabel<288, 24> heading(&panel);
PanelList<146, 134> menu(&panel);
const char* const items[] = {"Overview", "Sensors", "Outputs", "Display",
                             "Controls", "Logging", "About"};

TouchProcessor touchProcessor;
bool ready = false;

void onOkClicked() {
    const int index = menu.selectedIndex();
    heading.setText(index >= 0 ? items[index] : "Select an item");
}

void onCancelClicked() {
    heading.setText("PanelKit menu");
}

void setup() {
    Serial.begin(115200);
    ready = display.begin();
    if (!ready)
        return;

    // 1. Configure and ACTIVATE the parent panel container
    panel.setName("mainPanel");
    panel.setLoc(0, 0);
    panel.activate();

    // 2. Configure OK Button
    btnOk.setName("btnOk");
    btnOk.setText("OK")
         .setFont(font_roboto_regular_50::Font, 0.25f)
         .setJustification(TextJustify::CENTER, TextVAlign::MIDDLE)
         .setColors(0x03E0, 0xFFFF, 0x02C0) // Green background
         .onClick(onOkClicked);
    btnOk.activate();

    // 3. Configure Cancel Button
    btnCancel.setName("btnCancel");
    btnCancel.setText("Cancel")
             .setFont(font_roboto_regular_50::Font, 0.20f)
             .setJustification(TextJustify::CENTER, TextVAlign::MIDDLE)
             .setColors(0xF800, 0xFFFF, 0xA000) // Red background
             .onClick(onCancelClicked);
    btnCancel.activate();

    BaseComponent* buttons[] = {&btnOk, &btnCancel};
    panelStack(buttons, 2, 18, 54, PanelAxis::VERTICAL, 14);
    const auto& font = font_roboto_regular_50::Font;
    heading.setLoc(18, 18);
    heading.setFont(font, 20).setText("PanelKit menu");
    menu.setLoc(156, 54);
    menu.setFont(font, 24);
    menu.setItems(items, sizeof(items) / sizeof(items[0]));
    menu.activate();
    panel.setBackground(RGB565(19, 31, 43));
    panel.present(display);

    // Use immediate clicks for buttons and list rows.
    touchProcessor.setDeferredClickMode(false);

    // Or set it to Immediate Mode (instant response, double clicks fire after single click)
    // touchProcessor.setDeferredClickMode(false);
}

void loop() {
    if (!ready) return;
    // Process incoming hardware touch coordinates
    touchProcessor.process(display.touched(),
                           display.touchX(),
                           display.touchY());

    // Update internal button state and redraw pixmaps if dirty
    panel.present(display);
}
