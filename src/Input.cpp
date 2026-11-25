#include "Input.h"
#include <string.h>

uint8_t panelCommandCrc8(const char* payload, size_t length)
{
    static constexpr uint8_t nibble[16] = {0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15, 0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d};
    uint8_t crc = 0;
    while (length--)
    {
        crc ^= uint8_t(*payload++);
        crc = uint8_t(uint8_t(crc << 4) ^ nibble[crc >> 4]);
        crc = uint8_t(uint8_t(crc << 4) ^ nibble[crc >> 4]);
    }
    return crc;
}

namespace
{
int hexDigit(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return -1;
}

bool integer(char*& cursor, int& out)
{
    while (*cursor == ' ' || *cursor == '\t') ++cursor;
    const bool negative = *cursor == '-';
    if (negative || *cursor == '+') ++cursor;
    if (*cursor < '0' || *cursor > '9') return false;
    const uint64_t limit = negative ? uint64_t(INT_MAX) + 1 : uint64_t(INT_MAX);
    uint64_t value = 0;
    while (*cursor >= '0' && *cursor <= '9')
    {
        const unsigned digit = unsigned(*cursor++ - '0');
        if (value > (limit - digit) / 10) return false;
        value = value * 10 + digit;
    }
    if (*cursor && *cursor != ' ' && *cursor != '\t') return false;
    out = negative ? int(-int64_t(value)) : int(value);
    return true;
}

bool end(const char* cursor)
{
    while (*cursor == ' ' || *cursor == '\t') ++cursor;
    return !*cursor;
}
}

bool PanelStreamInput::validateChecksum()
{
    if (size < 5 || line[0] != '@' || line[3] != ':') return false;
    const int high = hexDigit(line[1]), low = hexDigit(line[2]);
    return high >= 0 && low >= 0 &&
           panelCommandCrc8(line + 4, size - 4) == uint8_t((high << 4) | low);
}

bool PanelStreamInput::processLine(char* command)
{
    while (*command == ' ' || *command == '\t') ++command;
    if (!*command) return true;
    char* args = command;
    while (*args && *args != ' ' && *args != '\t') ++args;
    if (*args) *args++ = '\0';
    int x, y, value;
    if (!strcmp(command, "text"))
    {
        while (*args) input.key(*args++, source);
    }
    else if (!strcmp(command, "key"))
    {
        if (!integer(args, value) || !end(args) || value < 0 || value > 255) return false;
        input.key(char(uint8_t(value)), source);
    }
    else if (!strcmp(command, "cancel"))
    {
        if (!end(args)) return false;
        input.cancel(source);
    }
    else if (!strcmp(command, "up") && end(args))
    {
        input.pointer(source, false, 0, 0);
    }
    else
    {
        if (!integer(args, x) || !integer(args, y)) return false;
        if (!strcmp(command, "scroll") || !strcmp(command, "pointer"))
        {
            if (!integer(args, value) || !end(args)) return false;
            if (!strcmp(command, "scroll")) input.wheel(source, x, y, float(value));
            else if (value == 1) input.pointer(source, true, x, y);
            else if (value == 0)
            {
                if (!input.releaseAt(source, x, y)) input.move(source, x, y);
            }
            else return false;
        }
        else
        {
            if (!end(args)) return false;
            if (!strcmp(command, "down")) input.pointer(source, true, x, y);
            else if (!strcmp(command, "up")) input.releaseAt(source, x, y);
            else if (!strcmp(command, "move")) input.move(source, x, y);
            else if (!strcmp(command, "click")) input.tap(source, x, y);
            else if (!strcmp(command, "double")) input.tap(source, x, y, true);
            else return false;
        }
    }
    return true;
}

void PanelStreamInput::feed(uint8_t byte)
{
    if (mode == PanelStreamMode::TEXT)
    {
        input.key(char(byte), source);
        return;
    }
    if (byte == '\n')
    {
        if (!discard)
        {
            if (size && line[size - 1] == '\r') --size;
            line[size] = '\0';
            const bool checked = mode == PanelStreamMode::CHECKED_COMMANDS;
            if (checked && !validateChecksum())
            {
                ++checksumErrors;
                ++errors;
            }
            else if (!processLine(line + (checked ? 4 : 0))) ++errors;
        }
        size = 0;
        discard = false;
    }
    else if (!discard)
    {
        if (!byte || size == sizeof(line) - 1)
        {
            discard = true;
            ++errors;
        }
        else line[size++] = char(byte);
    }
}
