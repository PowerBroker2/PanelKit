#include "Menu.h"
#include <string.h>

namespace
{
bool nameByte(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-';
}

bool applyLink(const char* word, char (&out)[PanelMenuPathCapacity], size_t& used)
{
    while (*word)
    {
        const char* start = word;
        while (*word && *word != '/') ++word;
        const size_t count = size_t(word - start);
        if (!count) return false;
        if (count == 2 && start[0] == '.' && start[1] == '.')
        {
            if (!used) return false;
            while (used && out[--used] != '/') {}
        }
        else if (count != 1 || start[0] != '.')
        {
            const bool final = !*word;
            const bool extension = final && count > 4 && !strncmp(word - 4, ".pkm", 4);
            if (final && !extension) return false;
            for (size_t i = 0; i < count - (extension ? 4 : 0); ++i)
                if (!nameByte(start[i])) return false;
            if (used + count + 1 >= sizeof(out)) return false;
            out[used++] = '/';
            memcpy(out + used, start, count);
            used += count;
        }
        if (*word && !*++word) return false;
    }
    out[used] = '\0';
    return true;
}

bool copyField(char* destination, size_t capacity, const char* first, size_t count)
{
    if (!count || count >= capacity) return false;
    for (size_t i = 0; i < count; ++i)
        if (first[i] < 32 || first[i] > 126) return false;
    memcpy(destination, first, count);
    destination[count] = '\0';
    return true;
}
}

bool panelMenuResolve(const char* current, const char* link, char (&out)[PanelMenuPathCapacity])
{
    if (!link || !*link) return false;
    size_t used = 0;
    if (*link == '/') ++link;
    else if (current && *current)
    {
        if (*current != '/' || !applyLink(current + 1, out, used) || used <= 5 ||
            strcmp(out + used - 4, ".pkm")) return false;
        while (used && out[--used] != '/') {}
    }
    if (!applyLink(link, out, used)) return false;
    return used > 5 && !strcmp(out + used - 4, ".pkm");
}

bool panelMenuActionName(const char* value)
{
    if (!value || !*value) return false;
    while (*value) if (!nameByte(*value++)) return false;
    return true;
}

bool panelMenuParse(const char* bytes, size_t length, PanelMenuPage& page)
{
    if (!bytes || length < 5 || length > PanelMenuFileCapacity || memchr(bytes, 0, length)) return false;
    page = {};
    size_t offset = 0;
    int line = 0, textCount = 0;
    while (offset < length)
    {
        const char* start = bytes + offset;
        const char* newline = static_cast<const char*>(memchr(start, '\n', length - offset));
        if (!newline) return false;
        offset = size_t(newline - bytes) + 1;
        const char* end = newline;
        if (end != start && end[-1] == '\r') --end;
        if (!line++)
        {
            if (end - start != 4 || memcmp(start, "PKM1", 4)) return false;
            continue;
        }
        const char* field = static_cast<const char*>(memchr(start, '\t', size_t(end-start)));
        if (!field) return false;
        const size_t tag = size_t(field-start);
        const char* value = field + 1;
        if (tag == 5 && !memcmp(start,"TITLE",5))
        {
            if (*page.title || !copyField(page.title,sizeof(page.title),value,size_t(end-value))) return false;
        }
        else if (tag == 4 && !memcmp(start,"VIEW",4))
        {
            if (*page.view || !copyField(page.view,sizeof(page.view),value,size_t(end-value)) ||
                !panelMenuActionName(page.view)) return false;
        }
        else if (tag == 4 && !memcmp(start,"TEXT",4))
        {
            if (textCount == 2 || !copyField(page.text[textCount],sizeof(page.text[0]),
                                             value,size_t(end-value))) return false;
            ++textCount;
        }
        else
        {
            const bool link = tag == 4 && !memcmp(start,"LINK",4);
            if ((!link && !(tag == 6 && !memcmp(start,"ACTION",6))) || page.count == PanelMenuMaxItems)
                return false;
            const char* separator = static_cast<const char*>(memchr(value,'\t',size_t(end-value)));
            auto& item = page.items[page.count];
            if (!separator || !copyField(item.label,sizeof(item.label),value,size_t(separator-value)) ||
                !copyField(item.target,sizeof(item.target),separator+1,size_t(end-separator-1)) ||
                (!link && !panelMenuActionName(item.target))) return false;
            item.link = link;
            ++page.count;
        }
    }
    return *page.title && (!*page.view || (!page.count && !textCount));
}
