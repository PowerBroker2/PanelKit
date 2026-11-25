#include <PanelKit.h>
#include <TeensyTFT.h>
#include <SD.h>
#include "font_inter_regular_16.h"

constexpr char provisionRoot[] = "/PKMENU";
constexpr char alternateRoot[] = "/PKMENU/_swap";
const char* mountedRoot = provisionRoot;
bool mediaEnabled = true;
TeensyTFT display(10, 9, 255);
Panel<320, 240, false> screen;
PanelInput input;
PanelTouchInput touch(input);
PanelStreamInput commands(input, PanelStreamMode::CHECKED_COMMANDS);
PanelButton<62, 24, false> backButton(&screen), homeButton(&screen);
PanelText statusText(174, 24, &screen);
Component<9, 12> cursor(&screen);
bool sdReady = false;
bool displayReady = false;

bool ensureCard()
{
    return mediaEnabled && sdReady;
}

bool readPage(const char* path, char* data, size_t capacity, size_t& length, void*)
{
    if (!ensureCard()) return false;
    char filePath[PanelMenuPathCapacity + sizeof(alternateRoot)];
    snprintf(filePath, sizeof(filePath), "%s%s", mountedRoot, path);
    File file = SD.open(filePath, FILE_READ);
    if (!file || file.isDirectory() || file.size() > capacity) return false;
    length = size_t(file.size());
    return file.read(data, length) == length;
}

void runAction(const char* name, void*)
{
    if (!strcmp(name, "probe"))
    {
        statusText.setText("Firmware action ran");
        Serial.println("ACTION probe");
    }
    else Serial.println("ERROR unknown-action");
}

class RichMenu
{
    static void changed(float next) { Serial.printf("RICH_VALUE %d\n", int(next)); }
    Panel<320, 208, false> page;
    PanelText title;
    PanelSlider gain;
public:
    RichMenu(ComponentRegistry& parent, PixmapRect viewport)
        : page(&parent), title(280, 40, &page), gain(200, 24, &page)
    {
        PanelTheme theme;
        theme.font = &font_inter_regular_16::Font;
        page.setBackground(theme.background);
        page.setLoc(viewport.x, viewport.y);
        page.setSize(viewport.width, viewport.height);
        page.activate();
        title.setTheme(theme);
        title.setText("Native slider in a lazy view");
        title.setLoc(10, 10);
        gain.setLoc(10, 65);
        gain.setRange(0, 100);
        gain.setStep(10);
        gain.onChange(changed);
        gain.activate();
    }
    void update() {}
};

const auto richView = PanelMenuView::make<RichMenu>("controls");
const PanelMenuView* const richViews[] = {&richView};
alignas(RichMenu) unsigned char richStorage[sizeof(RichMenu)];
PanelMenuNavigator<320,208> menu(screen, readPage, runAction, nullptr, 0, 32);
void back() { menu.requestBack(); }
void home() { menu.requestHome(); }

void report(PanelMenuStatus result)
{
    if (result == PanelMenuStatus::IDLE || result == PanelMenuStatus::ACTION_DISPATCHED) return;
    if (result == PanelMenuStatus::LOADED)
    {
        statusText.setText(menu.path());
        Serial.printf("PAGE %s %s\n", menu.path(), menu.title());
    }
    else if (result == PanelMenuStatus::FALLBACK)
    {
        statusText.setText("Built-in recovery");
        Serial.println("FALLBACK");
    }
    else
    {
        const char* reason = result == PanelMenuStatus::READ_FAILED ? "read" :
                             result == PanelMenuStatus::INVALID_PAGE ? "page" :
                             result == PanelMenuStatus::NO_HISTORY ? "history" :
                             result == PanelMenuStatus::NO_ACTION_HANDLER ? "action" :
                             result == PanelMenuStatus::UNKNOWN_VIEW ? "view" :
                             result == PanelMenuStatus::VIEW_STORAGE ? "storage" : "path";
        statusText.setText(result == PanelMenuStatus::READ_FAILED ?
                           (sdReady ? "Page unavailable" : "SD offline") :
                           result == PanelMenuStatus::INVALID_PAGE ? "Invalid menu page" :
                           result == PanelMenuStatus::UNKNOWN_VIEW ? "View not installed" : reason);
        Serial.printf("NAV_ERROR %s %s\n", reason, menu.path());
    }
    Serial.printf("LIFECYCLE %lu %d\n", (unsigned long)menu.generation(), screen.numComps());
}

void pollMedia(bool force = false)
{
    static uint32_t previous = 0;
    const uint32_t now = millis();
    if (!force && now - previous < (sdReady ? 350u : 1000u)) return;
    previous = now;
    if (sdReady && (!mediaEnabled || !SD.mediaPresent()))
    {
        sdReady = false;
        statusText.setText("SD offline");
        Serial.println("MEDIA OFFLINE");
        return;
    }
    if (!sdReady && mediaEnabled && SD.begin(BUILTIN_SDCARD))
    {
        sdReady = true;
        Serial.println("MEDIA ONLINE");
        report(menu.begin());
    }
}

int hexDigit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

bool sameFile(const char* path, const char* bytes, size_t length)
{
    File file = SD.open(path, FILE_READ);
    if (!file || file.isDirectory() || file.size() != length) return false;
    for (size_t i = 0; i < length; ++i)
        if (file.read() != uint8_t(bytes[i])) return false;
    return true;
}

bool makeParents(char* path)
{
    for (char* separator = path + 1; *separator; ++separator)
    {
        if (*separator != '/') continue;
        *separator = '\0';
        const bool ok = SD.exists(path) || SD.mkdir(path);
        *separator = '/';
        if (!ok) return false;
    }
    return true;
}

bool storeFile(char* name, const char* encoded)
{
    char path[PanelMenuPathCapacity];
    const size_t digits = strlen(encoded);
    if (!ensureCard() || !panelMenuResolve("", name, path) || strcmp(path, name) ||
        !digits || (digits % 2) || digits / 2 > PanelMenuFileCapacity) return false;

    char bytes[PanelMenuFileCapacity];
    for (size_t i = 0; i < digits / 2; ++i)
    {
        const int high = hexDigit(encoded[i * 2]), low = hexDigit(encoded[i * 2 + 1]);
        if (high < 0 || low < 0) return false;
        bytes[i] = char((high << 4) | low);
    }
    char destination[PanelMenuPathCapacity + sizeof(provisionRoot)];
    snprintf(destination, sizeof(destination), "%s%s", provisionRoot, path);
    if (SD.exists(destination)) return sameFile(destination, bytes, digits / 2);

    char staging[sizeof(destination) + 4];
    snprintf(staging, sizeof(staging), "%s.tmp", destination);
    if (!makeParents(destination) || SD.exists(staging)) return false;
    File file = SD.open(staging, FILE_WRITE);
    if (!file) return false;
    const bool written = file.write(reinterpret_cast<uint8_t*>(bytes), digits / 2) == digits / 2;
    file.close();
    if (written && sameFile(staging, bytes, digits / 2) && SD.rename(staging, destination)) return true;
    SD.remove(staging);
    return false;
}

void adminCommand(char* line, size_t length)
{
    if (length < 5 || line[3] != ':') { Serial.println("ERROR frame"); return; }
    const int high = hexDigit(line[1]), low = hexDigit(line[2]);
    char* body = line + 4;
    if (high < 0 || low < 0 || panelCommandCrc8(body, length - 4) != (high << 4 | low))
    {
        Serial.println("ERROR checksum");
        return;
    }

    if (!strcmp(body, "STATUS"))
    {
        Serial.printf("STATUS %s %s\n", ensureCard() ? "SD_OK" : "SD_FAIL",
                      displayReady ? "DISPLAY_OK" : "DISPLAY_FAIL");
        return;
    }
    // Demonstration harness only. Simulate an ejected card or another mounted volume.
    if (!strcmp(body, "MEDIA OFF"))
    {
        mediaEnabled = false;
        pollMedia(true);
        return;
    }
    if (!strcmp(body, "MEDIA DEFAULT") || !strcmp(body, "MEDIA ALTERNATE"))
    {
        if (sdReady) { mediaEnabled = false; pollMedia(true); }
        mountedRoot = !strcmp(body, "MEDIA DEFAULT") ? provisionRoot : alternateRoot;
        mediaEnabled = true;
        pollMedia(true);
        return;
    }
    if (!strcmp(body, "START"))
    {
        commands.reset();
        report(menu.begin());
        return;
    }
    if (!strncmp(body, "PUT ", 4))
    {
        char* name = body + 4;
        char* separator = strchr(name, ' ');
        if (separator)
        {
            *separator++ = '\0';
            if (storeFile(name, separator))
            {
                Serial.printf("STORED %s\n", name);
                return;
            }
        }
        Serial.println("ERROR write-or-conflict");
        return;
    }
    if (!strncmp(body, "GET ", 4))
    {
        char path[PanelMenuPathCapacity], bytes[PanelMenuFileCapacity];
        size_t size = 0;
        if (panelMenuResolve("", body + 4, path) && !strcmp(path, body + 4) &&
            readPage(path, bytes, sizeof(bytes), size, nullptr))
        {
            static const char hex[] = "0123456789ABCDEF";
            Serial.printf("FILE %s ", path);
            for (size_t i = 0; i < size; ++i)
            {
                const uint8_t value = uint8_t(bytes[i]);
                Serial.write(hex[value >> 4]);
                Serial.write(hex[value & 15]);
            }
            Serial.println();
            return;
        }
        Serial.println("ERROR read");
        return;
    }
    Serial.println("ERROR command");
}

void pollUsb()
{
    static char line[2 * PanelMenuFileCapacity + PanelMenuPathCapacity + 10];
    static size_t used = 0;
    static bool first = true, admin = false, discard = false, connected = false;
    if (!Serial)
    {
        if (connected)
        {
            commands.reset();
            used = 0;
            first = true;
            admin = discard = connected = false;
        }
        return;
    }
    connected = true;
    for (int budget = 128; budget && Serial.available(); --budget)
    {
        const int value = Serial.read();
        if (value < 0) break;
        const char c = char(value);
        if (first)
        {
            admin = c == '!';
            first = false;
        }
        if (!admin) commands.feed(uint8_t(c));
        else if (c == '\n')
        {
            if (discard) Serial.println("ERROR length");
            else
            {
                if (used && line[used - 1] == '\r') --used;
                line[used] = '\0';
                adminCommand(line, used);
            }
            used = 0;
            discard = false;
        }
        else if (c == '\0' || used == sizeof(line) - 1) discard = true;
        else if (!discard) line[used++] = c;
        if (c == '\n') first = true;
    }
}

void setup()
{
    Serial.begin(115200);
    sdReady = SD.begin(BUILTIN_SDCARD);
    displayReady = display.begin();
    input.setBounds(320, 240);
    screen.activate();
    screen.setBackground(RGB565(10, 22, 34));
    PanelTheme theme;
    theme.font = &font_inter_regular_16::Font;
    theme.textHeight = 14;
    menu.setTheme(theme);
    menu.setViews(richViews, 1, richStorage, sizeof(richStorage));
    statusText.setTheme(theme);
    statusText.setLoc(144, 4);
    backButton.setLoc(4, 4);
    backButton.setFont(*theme.font, 0.7f).setText("Back").onClick(back);
    backButton.activate();
    homeButton.setLoc(72, 4);
    homeButton.setFont(*theme.font, 0.7f).setText("Home").onClick(home);
    homeButton.activate();
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x <= y / 2; ++x)
            cursor.pixmap.setPixelValue(x, y, x == 0 || x == y / 2 || y == 9 ? WHITE : BLACK);
    cursor.setOverlay(true);
    report(menu.begin());
}

void loop()
{
    if (displayReady) touch.pollDisplay(display);
    pollUsb();
    input.tick();
    pollMedia();
    if (menu.hasPending()) report(menu.update());
    menu.updateView();
    cursor.setLoc(input.cursorX(), input.cursorY());
    if (displayReady) screen.present(display);
}
