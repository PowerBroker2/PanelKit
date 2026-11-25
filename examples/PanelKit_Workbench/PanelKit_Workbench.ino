#include <PanelKit.h>
#include <TeensyTFT.h>
#include "font_inter_regular_16.h"

TeensyTFT display(10, 9, 255);
TouchProcessor touch;
Panel<320, 240, false> desktop;
PanelTabs tabs(304, 24, &desktop);
Panel<320, 208, false> controlsPage(&desktop);
Panel<320, 208, false> livePage(&desktop);
Panel<320, 208, false> imagesPage(&desktop);
Panel<320, 208, false> calendarPage(&desktop);
PanelFlex<126, 196> sidebar(&controlsPage);
PanelText heading(100, 24, &sidebar);
PanelComboBox mode(100, 26, &sidebar);
PanelRadioGroup units(100, 58, &sidebar);
PanelCheckBox showWindow(100, 24, &sidebar);
PanelSlider gain(100, 20, &sidebar);

PanelWindow<172, 196> inspector(&controlsPage);
PanelFlex<172, 172> inspectorLayout(&inspector);
PanelTable readings(172, 100, &inspectorLayout);
PanelScrollArea<172, 56> notes(&inspectorLayout);
PanelText firstNote(145, 22, &notes), secondNote(145, 22, &notes), thirdNote(145, 22, &notes);

PanelPlot<298, 90, 42, false> livePlot(&livePage);
PanelProgress signalLevel(200, 12, &livePage);
PanelSlider signalSpeed(200, 22, &livePage);
PanelToggle liveEnabled(54, 22, &livePage);
PanelLabel<184, 19> liveCaption(&livePage);
PanelText speedCaption(196, 17, &livePage);
PanelButton<86, 30, false> clearPlot(&livePage);

Pixmap<64, 40> artwork;
PanelPicture image(155, 100, &imagesPage);
Component<88, 59> bufferedImage(&imagesPage);
PanelSprite<74, 74, 64, 40> rotatingSprite(artwork, {16, 4, 32, 32}, &imagesPage);
PanelList<95, 76> imageModes(&imagesPage);
PanelSlider imageWidth(148, 22, &imagesPage);
PanelText imageCaption(188, 19, &imagesPage);
PanelText bufferCaption(115, 20, &imagesPage);

PanelCalendar calendar(206, 168, &calendarPage);
PanelText selectedDate(105, 24, &calendarPage);
PanelSpinBox number(96, 29, &calendarPage);
PanelTextField entry(96, 29, &calendarPage);
PanelText entryCaption(100, 19, &calendarPage);
PanelKeypad keypad(96, 78, &calendarPage);
const char* const pageNames[] = {"Controls", "Live", "Images", "Calendar"};
BaseComponent* const pages[] = {&controlsPage,&livePage,&imagesPage,&calendarPage};
const char* const fitModes[] = {"Fit", "Fill", "Stretch"};

const char* const modes[] = {"Summary", "Live", "History", "Diagnostics", "Settings"};
const char* const unitNames[] = {"Volts", "Percent"};
const PanelTable::Column columns[] = {{"Channel", 101}, {"Value", 86}};
const char* const data[][2] = {
    {"Input A", "1.25 V"}, {"Input B", "2.70 V"}, {"Input C", "0.50 V"},
    {"Input D", "3.00 V"}, {"Input E", "1.88 V"}, {"Input F", "2.15 V"}
};
const PanelFlexItem sidebarItems[] = {
    {&heading, 22, 0}, {&mode, 24, 0}, {&units, 48, 0},
    {&showWindow, 22, 0}, {&gain, 18, 0}
};
const PanelFlexItem inspectorItems[] = {{&readings, 56, 2}, {&notes, 32, 1}};
bool ready = false;
uint32_t lastSample = 0;
float phase = 0;

const char* readCell(void*, int row, int col) { return data[row][col]; }
void toggleWindow(bool visible) { inspector.setVisible(visible); }
void closedWindow() { showWindow.setChecked(false); }
void changeMode(int index) { heading.setText(modes[index]); }
void resetPlot() { livePlot.clearSamples(); }
void changeImageMode(int index)
{
    const BlitMode modes[] = {BlitMode::FIT, BlitMode::FILL, BlitMode::STRETCH};
    if (index >= 0 && index < 3) image.setMode(modes[index]);
}
void resizeImage(float width) { image.setSize(int(width), 100); }
void showDate(PanelDate date)
{
    char text[16];
    snprintf(text, sizeof(text), "%04d-%02d-%02d", date.year, date.month, date.day);
    selectedDate.setText(text);
}
void typeKey(char key)
{
    Event e{};
    e.type = EventType::TYPE;
    e.character = key;
    entry.handleEvent(e);
}

void setup()
{
    Serial.begin(115200);
    ready = display.begin();
    if (!ready) return;

    PanelTheme theme;
    theme.font = &font_inter_regular_16::Font;
    theme.textHeight = 13;
    desktop.setBackground(RGB565(14, 23, 35));
    desktop.activate();
    tabs.setLoc(8, 3);
    tabs.setTheme(theme);
    tabs.setPages(pageNames, pages, 4);
    tabs.activate();
    for (auto* page : pages)
    {
        page->setLoc(0, 30);
        page->activate();
    }

    sidebar.setLoc(8, 3);
    sidebar.setBackground(theme.background);
    sidebar.activate();
    heading.setTheme(theme);
    heading.setText("Summary");
    mode.setTheme(theme);
    mode.setItems(modes, sizeof(modes) / sizeof(modes[0]));
    mode.setPopupRows(4);
    mode.onChange(changeMode);
    mode.activate();
    units.setTheme(theme);
    units.setItems(unitNames, sizeof(unitNames) / sizeof(unitNames[0]));
    units.activate();
    showWindow.setTheme(theme);
    showWindow.setText("Inspector");
    showWindow.setChecked(true);
    showWindow.onChange(toggleWindow);
    showWindow.activate();
    gain.setRange(0, 100);
    gain.setStep(5);
    gain.setValue(45);
    gain.activate();
    sidebar.setLayout(sidebarItems, sizeof(sidebarItems) / sizeof(sidebarItems[0]),
                      PanelAxis::VERTICAL, 7, 8);

    inspector.setLoc(140, 3);
    inspector.setTitle("Inspector");
    inspector.setTheme(theme);
    inspector.setOrder(1);
    inspector.setMinSize(116, 94);
    inspector.onClose(closedWindow);
    inspector.activate();
    inspectorLayout.activate();
    inspectorLayout.setBackground(theme.background);
    readings.setTheme(theme);
    readings.setModel(columns, 2, sizeof(data) / sizeof(data[0]), readCell);
    readings.activate();

    notes.setBackground(theme.background);
    notes.setContentSize(154, 105);
    notes.activate();
    firstNote.setTheme(theme);
    secondNote.setTheme(theme);
    thirdNote.setTheme(theme);
    firstNote.setText("Drag table to scroll");
    secondNote.setText("Resize at lower right");
    thirdNote.setText("Drag title to move");
    secondNote.setLoc(0, 34);
    thirdNote.setLoc(0, 68);
    inspectorLayout.setLayout(inspectorItems, 2, PanelAxis::VERTICAL, 4, 3);
    inspector.fitSingleChild(true);

    liveCaption.setLoc(10, 3);
    liveCaption.setFont(*theme.font, 15).setText("Live telemetry");
    speedCaption.setLoc(9, 143);
    speedCaption.setTheme(theme);
    speedCaption.setText("Sample speed");
    livePlot.setLoc(10, 29);
    livePlot.setRange(-1, 1);
    signalLevel.setLoc(10, 132);
    signalSpeed.setLoc(10, 158);
    signalSpeed.setRange(0.2f, 3.0f);
    signalSpeed.setValue(1);
    signalSpeed.activate();
    liveEnabled.setLoc(244, 128);
    liveEnabled.setChecked(true);
    liveEnabled.activate();
    clearPlot.setLoc(226, 158);
    clearPlot.setFont(*theme.font, 0.72f).setText("Clear").setCornerRadius(5).onClick(resetPlot);
    clearPlot.activate();

    artwork.draw_rect(0, 0, 64, 40, 0, true, RGB565(26, 48, 69));
    artwork.draw_rect(0, 27, 64, 13, 0, true, RGB565(20, 97, 101));
    artwork.draw_circle(32, 19, 13, 0, true, RGB565(244, 183, 76), 1, Antialiasing::COVERAGE);
    artwork.fill_triangle(0, 33, 15, 16, 31, 33, RGB565(55, 120, 142));
    artwork.fill_triangle(23, 38, 49, 11, 64, 38, RGB565(48, 117, 103));
    imageCaption.setLoc(8, 2);
    imageCaption.setTheme(theme);
    imageCaption.setText("Resize the image");
    image.setLoc(8, 27);
    image.setPixmap(artwork);
    imageWidth.setLoc(8, 146);
    imageWidth.setRange(84, 200);
    imageWidth.setValue(155);
    imageWidth.onChange(resizeImage);
    imageWidth.activate();
    imageModes.setLoc(216, 27);
    imageModes.setFont(*theme.font, 23);
    imageModes.setItems(fitModes, 3);
    imageModes.onChange(changeImageMode);
    imageModes.activate();
    bufferedImage.setLoc(8, 170);
    bufferedImage.pixmap.blitResized(artwork, 0, 0, 88, 37, ScaleMode::BILINEAR);
    bufferCaption.setLoc(102, 179);
    bufferCaption.setTheme(theme);
    bufferCaption.setText("Buffered resize");
    rotatingSprite.setLoc(232, 108);
    BlitTransform pose(37, 37);
    pose.pivotX = pose.pivotY = 16;
    pose.angle = 22;
    pose.scaleX = pose.scaleY = 1.5f;
    rotatingSprite.setPose(pose);

    calendar.setLoc(7, 5);
    PanelTheme calendarTheme = theme;
    calendarTheme.padding = 3;
    calendar.setTheme(calendarTheme);
    calendar.setDate({2026, 11, 24});
    calendar.onChange(showDate);
    calendar.activate();
    selectedDate.setLoc(215, 10);
    selectedDate.setTheme(theme);
    showDate(calendar.getDate());
    number.setLoc(217, 52);
    number.setTheme(theme);
    number.setRange(0, 100, 5);
    number.setValue(20);
    number.activate();
    entry.setLoc(217, 96);
    entryCaption.setLoc(215, 76);
    entryCaption.setTheme(theme);
    entryCaption.setText("Serial input");
    entry.setTheme(theme);
    entry.setText("");
    entry.activate();
    keypad.setLoc(217, 128);
    PanelTheme keyTheme = theme;
    keyTheme.textHeight = 12;
    keypad.setTheme(keyTheme);
    keypad.onKey(typeKey);
    keypad.activate();
    desktop.present(display);
}

void loop()
{
    if (!ready) return;
    const bool pressed = display.touched();
    touch.process(pressed, display.touchX(), display.touchY());
    const int character = Serial.read();
    if (character >= 0) type(char(character));
    const uint32_t now = millis();
    if (tabs.selectedIndex() == 1 && liveEnabled.isChecked() && now - lastSample >= 55)
    {
        lastSample = now;
        phase = fmodf(phase + 0.16f * signalSpeed.getValue(), float(2 * PI));
        const float reading = 0.77f * sinf(phase) + 0.19f * sinf(phase * 4.0f);
        livePlot.append(reading);
        signalLevel.setValue((reading + 1) * 0.5f);
    }
    desktop.present(display);
}
