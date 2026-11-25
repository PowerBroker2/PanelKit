#include <PanelKit.h>
#include <TeensyTFT.h>
#include "font_inter_regular_16.h"

TeensyTFT display(10, 9, 255);
PanelInput input(4);
PanelTouchInput touch(input);
PanelStreamInput usb(input, PanelStreamMode::CHECKED_COMMANDS);
Panel<320, 240, false> desktop;
PanelTabs tabs(304, 26, &desktop);
Panel<320, 204, false> controls(&desktop), forms(&desktop), gallery(&desktop);
PanelText summary(132, 20, &controls), gainLabel(132, 20, &controls);
PanelComboBox mode(132, 24, &controls);
PanelCheckBox visibleWindow(132, 24, &controls);
PanelRadioGroup units(132, 44, &controls);
PanelSlider gain(132, 19, &controls);
PanelButton<132, 22, false> reopen(&controls);

PanelWindow<164, 188> inspector(&controls);
PanelFlex<164, 164> client(&inspector);
PanelTable table(156, 82, &client);
PanelScrollArea<156, 62> notes(&client);
PanelText note1(142, 24, &notes), note2(142, 24, &notes), note3(142, 24, &notes),
          note4(142, 24, &notes);
const PanelFlexItem layout[] = {{&table, 82, 0}, {&notes, 62, 0}};

Panel<144, 178, false> formBox(&forms);
PanelTextField entry(128, 27, &formBox);
PanelSpinBox quantity(128, 29, &formBox);
PanelKeypad keypad(128, 80, &formBox);
PanelCalendar calendar(154, 158, &forms);
PanelText dateLabel(154, 20, &forms);
PanelButton<132, 22, false> dialogButton(&forms);
PanelDialog<246, 162> editor(&desktop);
PanelText dialogMessage(224, 24, &editor);
PanelPicture dialogImage(62, 55, &editor);
PanelTextField draft(142, 27, &editor);
PanelSpinBox draftQuantity(142, 26, &editor);
PanelButton<64, 24, false> dialogCancel(&editor), dialogKeep(&editor);
PanelButton<68, 24, false> dialogSave(&editor);
Component<13, 19> pointer(&desktop);
PanelLabel<140, 20> galleryTitle(&gallery);
// Keep the larger buffered widgets in Teensy RAM2 to leave RAM1 room for rendering.
DMAMEM PanelList<140, 90> assets(&gallery);
PanelFlex<140, 24> metrics(&gallery);
PanelToggle enabled(52, 22, &metrics);
PanelProgress progress(80, 10, &metrics);
const PanelFlexItem metricItems[] = {{&enabled, 52, 0}, {&progress, 74, 1}};
PanelPlot<67, 68, 16, false> livePlot(&gallery);
DMAMEM PanelPlot<67, 68, 16, true> bufferedPlot(&gallery);
Pixmap<24, 20> artwork;
PanelPicture picture(83, 65, &gallery);
PanelSprite<67, 65, 24, 20> sprite(artwork, {0, 0, 24, 20}, &gallery);
DMAMEM Panel<154, 77> tileGrid(&gallery);
DMAMEM Component<72, 68> tile(&tileGrid);
DMAMEM PanelButton<72, 68> tileButton(&tileGrid);
BaseComponent* const gridItems[] = {&tile, &tileButton};
const PanelGridLayout tileLayout{gridItems, 2, 2, 4, 0, 3};
const char* const assetNames[] = {"Fit", "Fill", "Stretch", "Rotate", "Scale"};
int artworkMode = 0;

const char* const tabLabels[] = {"Controls", "Form & date", "Gallery"};
BaseComponent* const pages[] = {&controls, &forms, &gallery};
const char* const modes[] = {"Overview", "Diagnostics", "Settings", "History"};
const char* const unitLabels[] = {"Volts", "Percent"};
const PanelTable::Column columns[] = {{"Channel", 76}, {"Value", 74}};
const char* const cells[][2] = {
    {"Input A", "1.25 V"}, {"Input B", "2.70 V"}, {"Input C", "0.50 V"},
    {"Input D", "3.00 V"}, {"Input E", "2.15 V"}, {"Input F", "1.88 V"},
};
bool displayReady = false;

void state(const char* name, int value)
{
    Serial.print(name);
    Serial.print(' ');
    Serial.println(value);
}

const char* cellText(void*, int row, int col) { return cells[row][col]; }
void tabChanged(int value) { state("TAB", value); }
void modeChanged(int value) { summary.setText(modes[value]); state("MODE", value); }
void unitsChanged(int value) { state("UNITS", value); }
void windowChanged(bool value) { inspector.setVisible(value); state("VISIBLE", value); }
void reopenWindow() { visibleWindow.setChecked(true); windowChanged(true); state("BUTTON", 1); }
void closeWindow()
{
    inspector.setModal(false);
    visibleWindow.setChecked(false);
    state("VISIBLE", 0);
}
void showModal()
{
    visibleWindow.setChecked(true);
    inspector.setVisible(true);
    inspector.setModal(true);
    state("MODAL", 1);
}
void gainChanged(float value)
{
    char label[24];
    snprintf(label, sizeof(label), "Gain %d %%", int(value));
    gainLabel.setText(label);
    state("GAIN", int(value));
}
void cellChanged(int row, int column)
{
    Serial.print("CELL ");
    Serial.print(row);
    Serial.print(' ');
    Serial.println(column);
}
void textChanged(const char* value)
{
    Serial.print("TEXT ");
    Serial.println(value);
}
void quantityChanged(int value) { state("QUANTITY", value); }
void keypadChanged(char character)
{
    Event event{};
    event.type = EventType::TYPE;
    event.character = character;
    entry.dispatchInput(event);
    state("KEYPAD", uint8_t(character));
}
void dateChanged(PanelDate date)
{
    char label[16];
    snprintf(label, sizeof(label), "%04d-%02d-%02d", date.year, date.month, date.day);
    dateLabel.setText(label);
    Serial.print("DATE ");
    Serial.println(label);
}

void draftChanged(const char* value) { Serial.printf("DRAFT %s\n", value); }
void draftQuantityChanged(int value) { state("DRAFT_QTY", value); }
void finishEdit(int result, void*)
{
    Serial.printf("DIALOG_RESULT %d %s %d\n", result, draft.getText(), draftQuantity.getValue());
    if (result == editor.Accepted)
    {
        entry.setText(draft.getText());
        quantity.setValue(draftQuantity.getValue());
    }
    Serial.printf("APPLIED %s %d\n", entry.getText(), quantity.getValue());
}
void cancelEdit() { editor.reject(); }
void keepEdit() { editor.finish(7); }
void saveEdit() { editor.accept(); }
void openEdit()
{
    draft.setText(entry.getText());
    draftQuantity.setValue(quantity.getValue());
    state("DIALOG", editor.open(&draft));
}

void place(BaseComponent& widget, int x, int y)
{
    widget.setLoc(x, y);
    widget.activate();
}

void makePointer()
{
    const char* const arrow[] = {
        "X", "XX", "XOX", "XOOX", "XOOOX", "XOOOOX", "XOOOOOX",
        "XOOOOOOX", "XOOOOOOOX", "XOOOOOOOOX", "XOOOOOOOOOX",
        "XOOOOXXXXXXX", "XOOXOOX", "XOX XOOX", "XX  XOOX", "X    XX",
    };
    for (int y = 0; y < int(sizeof(arrow) / sizeof(*arrow)); ++y)
        for (int x = 0; arrow[y][x]; ++x)
            if (arrow[y][x] != ' ')
                pointer.pixmap.setPixelValue(x, y, arrow[y][x] == 'X' ? WHITE : BLACK);
    pointer.setOrder(100);
    pointer.setOverlay(true);
}

void renderGallery(int option)
{
    const BlitMode modes[] = {BlitMode::FIT, BlitMode::FILL, BlitMode::STRETCH};
    artworkMode = option;
    picture.setMode(modes[option % 3]);
    BlitTransform pose(33, 32);
    pose.pivotX = 12;
    pose.pivotY = 10;
    pose.angle = 14 * option;
    pose.scaleX = pose.scaleY = 1.5f + 0.15f * option;
    sprite.setPose(pose);
    tile.pixmap.clear();
    const ScaleMode filters[] = {ScaleMode::NEAREST, ScaleMode::BILINEAR, ScaleMode::AREA};
    tile.pixmap.blit_from_pixmap(artwork, modes[option % 3], filters[option % 3], 1.0f, 0, 0, 70, 60);
    tile.invalidate();
    progress.setValue(float(option) / 4);
    state("GALLERY", option);
    state("PROGRESS", int(progress.getValue() * 100));
}

void samplePlot(bool on)
{
    livePlot.clearSamples();
    bufferedPlot.clearSamples();
    const int count = on ? 16 : 4;
    for (int i = 0; i < count; ++i)
    {
        const float sample = (sinf(i * 0.73f) + 1.0f) * 0.5f;
        livePlot.append(sample);
        bufferedPlot.append(1.0f - sample);
    }
    metrics.setSize(on ? 132 : 140, 24);
    state("METRIC_WIDTH", progress.getSize().dx);
    state("PLOTS", count);
}

void tileInput(const Event& event)
{
    if (event.type == EventType::DOUBLE_CLICK) state("TILE_DOUBLE", event.source);
    if (event.type == EventType::CANCEL) state("TILE_CANCEL", event.source);
    if (event.type == EventType::RELEASE) state("TILE_RELEASE", event.source);
}

void changeTiles()
{
    renderGallery((artworkMode + 1) % 5);
    tileGrid.setSize(146, 70);
    state("GRID_WIDTH", tile.getSize().dx);
    state("TILE_BUTTON", tile.getLoc().x < tileButton.getLoc().x && tile.getSize().dx > 0);
}

void resetDemo()
{
    usb.reset();
    editor.reject();
    desktop.cancelInput();
    tabs.setCurrent(0);
    mode.dismissPopup();
    mode.setSelected(0);
    summary.setText("Overview");
    units.setSelected(0);
    visibleWindow.setChecked(true);
    inspector.setModal(false);
    inspector.setVisible(true);
    if (inspector.isMinimized()) inspector.setMinimized(false);
    if (inspector.isSnapped())
    {
        inspector.setMaximized(true);
        inspector.setMaximized(false);
    }
    inspector.setLoc(148, 8);
    inspector.setSize(164, 188);
    table.setModel(columns, 2, sizeof(cells) / sizeof(cells[0]), cellText);
    notes.setScroll(0, 0);
    gain.setValue(25);
    gainLabel.setText("Gain 25 %");
    entry.setText("");
    quantity.setValue(20);
    calendar.setDate({2026, 11, 24});
    dateLabel.setText("2026-11-24");
    tileGrid.setSize(154, 77);
    assets.setItems(assetNames, 5);
    enabled.setChecked(false);
    renderGallery(0);
    samplePlot(false);
    input.move(PanelInput::SoftwareSource, 12, 17);
}

void setup()
{
    Serial.begin(115200);
    displayReady = display.begin();
    input.setBounds(320, 240);

    PanelTheme theme;
    theme.font = &font_inter_regular_16::Font;
    theme.textHeight = 13;
    desktop.setBackground(RGB565(14, 23, 35));
    desktop.activate();
    tabs.setTheme(theme);
    tabs.setPages(tabLabels, pages, 3);
    tabs.onChange(tabChanged);
    place(tabs, 8, 4);
    place(controls, 0, 34);
    place(forms, 0, 34);
    summary.setTheme(theme);
    place(summary, 8, 6);
    mode.setTheme(theme);
    mode.setItems(modes, 4);
    mode.setPopupRows(2);
    mode.onChange(modeChanged);
    place(mode, 8, 32);
    visibleWindow.setTheme(theme);
    visibleWindow.setText("Show inspector");
    visibleWindow.onChange(windowChanged);
    place(visibleWindow, 8, 66);
    units.setTheme(theme);
    units.setItems(unitLabels, 2);
    units.onChange(unitsChanged);
    place(units, 8, 99);
    gainLabel.setTheme(theme);
    place(gainLabel, 8, 143);
    gain.setRange(0, 100);
    gain.setStep(5);
    gain.onChange(gainChanged);
    place(gain, 8, 162);
    reopen.setText("Restore window").setFont(*theme.font, 0.68f).setCornerRadius(5);
    reopen.onClick(reopenWindow).onDoubleClick(showModal);
    place(reopen, 8, 181);

    inspector.setTheme(theme);
    inspector.setTitle("Inspector");
    inspector.setOrder(1);
    inspector.onClose(closeWindow);
    place(inspector, 148, 8);
    client.setBackground(theme.background);
    client.activate();
    table.setTheme(theme);
    table.setHeaderHeight(20);
    table.setRowHeight(20);
    table.onCellClick(cellChanged);
    table.activate();
    notes.setBackground(RGB565(20, 50, 52));
    notes.setContentSize(148, 110);
    notes.activate();
    PanelText* const captions[] = {&note1, &note2, &note3, &note4};
    const char* const messages[] = {"Scroll this pane", "Wheel moves notes",
                                    "Drag to pan", "End of notes"};
    for (int i = 0; i < 4; ++i)
    {
        captions[i]->setTheme(theme);
        captions[i]->setText(messages[i]);
        captions[i]->setLoc(1, 26 * i);
    }
    client.setVerticalLayout(layout, 2, 8, 4);
    inspector.fitSingleChild(true);

    formBox.setBackground(theme.background);
    place(formBox, 8, 8);
    entry.setTheme(theme);
    entry.onChange(textChanged);
    place(entry, 8, 9);
    quantity.setTheme(theme);
    quantity.setRange(0, 100, 5);
    quantity.onChange(quantityChanged);
    place(quantity, 8, 48);
    PanelTheme keys = theme;
    keys.padding = 2;
    keypad.setTheme(keys);
    keypad.onKey(keypadChanged);
    place(keypad, 8, 86);
    calendar.setTheme(keys);
    calendar.onChange(dateChanged);
    place(calendar, 158, 8);
    dateLabel.setTheme(theme);
    place(dateLabel, 158, 168);

    place(gallery, 0, 34);
    galleryTitle.setText("Artwork & plot modes").setFont(*theme.font, 13);
    place(galleryTitle, 8, 5);
    assets.setFont(*theme.font, 20);
    assets.setSize(140, 60);
    assets.onChange(renderGallery);
    place(assets, 8, 29);
    place(metrics, 8, 98);
    enabled.activate();
    enabled.onChange(samplePlot);
    metrics.setHorizontalLayout(metricItems, 2, 7);
    place(livePlot, 8, 127);
    place(bufferedPlot, 81, 127);
    artwork.draw_rect(0, 0, 24, 20, 0, true, RGB565(21, 59, 79));
    artwork.draw_circle(12, 8, 6, 0, true, RGB565(239, 179, 82));
    artwork.fill_triangle(0, 19, 9, 9, 23, 19, RGB565(60, 151, 130));
    picture.setPixmap(artwork);
    dialogImage.setPixmap(artwork);
    place(picture, 156, 12);
    place(sprite, 245, 12);
    tileGrid.setBackground(theme.surface);
    place(tileGrid, 156, 86);
    tileGrid.setGridLayout(&tileLayout);
    tileButton.setFont(*theme.font, 0.7f).setText("Rotate").setPadding(2)
              .setJustification(TextJustify::RIGHT, TextVAlign::BOTTOM).onClick(changeTiles);
    tileButton.activate();
    tile.onInput(tileInput);
    tile.activate();

    dialogButton.setText("Edit values").setFont(*theme.font, 0.68f).setCornerRadius(5).onClick(openEdit);
    place(dialogButton, 8, 182);
    editor.setTitle("Edit values");
    editor.setTheme(theme);
    editor.setAcceptOnEnter(true);
    editor.onFinished(finishEdit);
    dialogMessage.setTheme(theme);
    dialogMessage.setText("Set a label and quantity.");
    place(dialogMessage, 10, 7);
    place(dialogImage, 14, 35);
    draft.setTheme(theme);
    draft.onChange(draftChanged);
    place(draft, 87, 36);
    draftQuantity.setTheme(theme);
    draftQuantity.setRange(0, 100, 5);
    draftQuantity.onChange(draftQuantityChanged);
    place(draftQuantity, 87, 68);
    dialogCancel.setText("Cancel").setFont(*theme.font, 0.66f).onClick(cancelEdit);
    dialogKeep.setText("Keep").setFont(*theme.font, 0.66f).onClick(keepEdit);
    dialogSave.setText("Save").setFont(*theme.font, 0.66f).onClick(saveEdit);
    place(dialogCancel, 12, 102);
    place(dialogKeep, 90, 102);
    place(dialogSave, 166, 102);

    makePointer();
    resetDemo();
    pointer.setLoc(input.cursorX(), input.cursorY());
    if (displayReady) desktop.present(display, true);
}

void loop()
{
    static bool session = false;
    static int lastScroll = -1;
    static unsigned long errorBase = 0, crcBase = 0, lastErrors = 0;
    static PixmapRect lastWindow{};
    if (Serial && !session)
    {
        resetDemo();
        lastScroll = -1;
        lastWindow = {};
        errorBase = usb.errorCount();
        crcBase = usb.checksumErrorCount();
        lastErrors = 0;
        Serial.println(displayReady ? "PANELKIT_GUI_READY DISPLAY_OK" : "PANELKIT_GUI_READY DISPLAY_FAILED");
        session = true;
    }
    else if (!Serial && session)
    {
        usb.reset();
        session = false;
    }

    if (displayReady) touch.pollDisplay(display);
    if (session) usb.poll(Serial);
    input.tick();
    const int x = input.cursorX();
    const int y = input.cursorY();
    const auto oldPointer = pointer.getLoc();
    const bool moved = oldPointer.x != x || oldPointer.y != y;
    if (moved) pointer.setLoc(x, y);
    const bool presented = displayReady && desktop.present(display);
    if (!session) return;
    if (usb.errorCount() - errorBase != lastErrors)
    {
        lastErrors = usb.errorCount() - errorBase;
        Serial.printf("ERRORS %lu %lu\n", lastErrors, usb.checksumErrorCount() - crcBase);
    }
    if (moved)
        Serial.printf("POINTER %d %d %d\n", x, y, presented);
    const int scrollY = notes.getScroll().y;
    if (scrollY != lastScroll)
    {
        lastScroll = scrollY;
        state("NOTES", scrollY);
    }
    const auto window = inspector.bounds();
    if (window.x != lastWindow.x || window.y != lastWindow.y ||
        window.width != lastWindow.width || window.height != lastWindow.height)
    {
        lastWindow = window;
        Serial.printf("WINDOW %d %d %d %d\n", window.x, window.y, window.width, window.height);
    }
}
