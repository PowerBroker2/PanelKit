#pragma once
#include <new>
#include "Panel.h"
#include "Controls.h"

constexpr size_t PanelMenuPathCapacity = 80;
constexpr size_t PanelMenuFileCapacity = 512;
constexpr int PanelMenuMaxItems = 5;

struct PanelMenuItem
{
    char label[40];
    char target[PanelMenuPathCapacity];
    bool link;
};

struct PanelMenuPage
{
    char title[40];
    char view[40];
    char text[2][48];
    PanelMenuItem items[PanelMenuMaxItems];
    int count;
};

/** Resolve a .pkm link within the virtual menu root. Returns false on traversal above root. */
bool panelMenuResolve(const char* current, const char* link, char (&out)[PanelMenuPathCapacity]);
/** Parse and validate a complete PKM1 page before the active page is changed. */
bool panelMenuParse(const char* bytes, size_t length, PanelMenuPage& page);
bool panelMenuActionName(const char* value);

/** A firmware-registered view. The SD page selects the name, never executable code. */
struct PanelMenuView
{
    const char* name;
    size_t size, alignment;
    void* (*construct)(void*, ComponentRegistry&, PixmapRect);
    void (*destroy)(void*);
    void (*update)(void*);

    template<typename Scene>
    static PanelMenuView make(const char* name)
    {
        return {name, sizeof(Scene), alignof(Scene),
            [](void* storage, ComponentRegistry& parent, PixmapRect viewport) -> void* {
                return new (storage) Scene(parent, viewport);
            },
            [](void* instance) { static_cast<Scene*>(instance)->~Scene(); },
            [](void* instance) { static_cast<Scene*>(instance)->update(); }};
    }
};

enum class PanelMenuStatus { IDLE, LOADED, FALLBACK, ACTION_DISPATCHED, NO_ACTION_HANDLER,
                             INVALID_PATH, READ_FAILED, INVALID_PAGE, UNKNOWN_VIEW, VIEW_STORAGE, NO_HISTORY };

/**
 * A single live menu page backed by bounded storage. Requests are deferred
 * until update(), allowing a page's button callback to request its replacement.
 */
template<size_t WIDTH, size_t HEIGHT>
class PanelMenuNavigator
{
    static_assert(WIDTH >= 32 && HEIGHT >= 196, "The menu viewport needs at least 32 x 196 pixels");

public:
    using Reader = bool (*)(const char* path, char* data, size_t capacity, size_t& length, void* context);
    using Action = void (*)(const char* name, void* context);

private:
    class Row : public PanelControl
    {
        PanelMenuNavigator* navigator = nullptr;
        const PanelMenuItem* item = nullptr;
    public:
        explicit Row(ComponentRegistry* parent) : PanelControl(int(WIDTH) - 16, 24, parent) {}
        void configure(PanelMenuNavigator& owner, const PanelMenuItem* value,
                       const PanelTheme& style, int index)
        {
            navigator = &owner;
            item = value;
            setTheme(style);
            setLoc(8, 68 + index * 26);
            setVisible(item != nullptr);
            if (item) activate();
        }
        void handleEvent(Event event) override
        {
            if (item && event.type == EventType::CLICK && inBounds(event.endX, event.endY))
            {
                if (item->link) navigator->request(item->target);
                else navigator->queueAction(item->target);
            }
        }
        void render(const PanelCanvas& canvas, int x, int y) override
        {
            if (!item) return;
            const auto box = panelTranslate(bounds(), x, y);
            canvas.fill(box, theme.surface);
            caption(canvas, box, item->label);
            renderDirty = false;
        }
    };

    struct Page
    {
        PanelMenuPage info;
        Panel<WIDTH, HEIGHT, false> panel;
        PanelText title, first, second;
        Row row1, row2, row3, row4, row5;

        Page(PanelMenuNavigator& owner, const PanelMenuPage& data)
            : info(data), panel(&owner.parent), title(int(WIDTH)-16, 25, &panel),
              first(int(WIDTH)-16,20,&panel), second(int(WIDTH)-16,20,&panel),
              row1(&panel), row2(&panel), row3(&panel), row4(&panel), row5(&panel)
        {
            panel.setLoc(owner.x, owner.y);
            panel.setBackground(owner.theme.background);
            panel.activate();
            PanelText* labels[] = {&title, &first, &second};
            const char* captions[] = {info.title, info.text[0], info.text[1]};
            const int positions[] = {0, 25, 45};
            for (int i = 0; i < 3; ++i)
            {
                labels[i]->setTheme(owner.theme);
                labels[i]->setText(captions[i]);
                labels[i]->setLoc(8, positions[i]);
            }
            Row* rows[] = {&row1, &row2, &row3, &row4, &row5};
            for (int i = 0; i < PanelMenuMaxItems; ++i)
                rows[i]->configure(owner, i < info.count ? &info.items[i] : nullptr, owner.theme, i);
        }
    };

    enum class Request { NONE, LINK, BACK, HOME, ACTION, INVALID };
    ComponentRegistry& parent;
    Reader read;
    Action action;
    void* context;
    PanelTheme theme;
    const int x, y;
    alignas(Page) unsigned char storage[sizeof(Page)];
    Page* live = nullptr;
    const PanelMenuView* const* views = nullptr;
    size_t viewCount = 0, arenaSize = 0;
    void* arena = nullptr;
    const PanelMenuView* activeView = nullptr;
    void* instance = nullptr;
    char currentTitle[40]{};
    uint32_t pageGeneration = 0;
    char current[PanelMenuPathCapacity]{};
    char pending[PanelMenuPathCapacity]{};
    static constexpr size_t historyLimit = 6;
    char history[historyLimit][PanelMenuPathCapacity]{};
    size_t historySize = 0;
    Request queued = Request::NONE;

    void destroyPage()
    {
        if (activeView) activeView->destroy(instance);
        if (live) live->~Page();
        activeView = nullptr;
        instance = nullptr;
        live = nullptr;
    }

    void replace(const PanelMenuPage& page, const char* path, const PanelMenuView* view = nullptr)
    {
        destroyPage();
        if (view)
        {
            activeView = view;
            instance = view->construct(arena, parent, {x, y, int(WIDTH), int(HEIGHT)});
        }
        else live = new (storage) Page(*this, page);
        strlcpy(currentTitle, page.title, sizeof(currentTitle));
        strlcpy(current, path, sizeof(current));
        ++pageGeneration;
    }

    PanelMenuStatus load(const char* path, PanelMenuPage& page)
    {
        char bytes[PanelMenuFileCapacity];
        size_t size = 0;
        if (!read || !read(path, bytes, sizeof(bytes), size, context))
            return PanelMenuStatus::READ_FAILED;
        if (!panelMenuParse(bytes, size, page)) return PanelMenuStatus::INVALID_PAGE;
        for (int i = 0; i < page.count; ++i)
        {
            char resolved[PanelMenuPathCapacity];
            if (page.items[i].link && !panelMenuResolve(path, page.items[i].target, resolved))
                return PanelMenuStatus::INVALID_PAGE;
        }
        return PanelMenuStatus::LOADED;
    }

    void queueAction(const char* name)
    {
        if (hasPending()) return;
        strlcpy(pending, name, sizeof(pending));
        queued = Request::ACTION;
    }

public:
    PanelMenuNavigator(ComponentRegistry& parent, Reader reader, Action handler = nullptr,
                       void* context = nullptr, int x = 0, int y = 0)
        : parent(parent), read(reader), action(handler), context(context), x(x), y(y) {}
    ~PanelMenuNavigator() { destroyPage(); }
    PanelMenuNavigator(const PanelMenuNavigator&) = delete;
    PanelMenuNavigator& operator=(const PanelMenuNavigator&) = delete;

    /** Set styling before begin(). The style is used when a new page is built. */
    void setTheme(const PanelTheme& value) { theme = value; }
    const char* path() const { return current; }
    const char* title() const { return currentTitle; }
    /** Register compiled views and caller-owned, suitably aligned storage before begin(). */
    bool setViews(const PanelMenuView* const* table, size_t count, void* storage, size_t size)
    {
        if (activeView || (count && (!table || !storage))) return false;
        views = table;
        viewCount = count;
        arena = storage;
        arenaSize = size;
        return true;
    }
    /** Run the active compiled scene’s frame hook, outside widget dispatch. */
    void updateView() { if (activeView) activeView->update(instance); }
    /** Number of successful page replacements, including fallback creation. */
    uint32_t generation() const { return pageGeneration; }
    bool hasPending() const { return queued != Request::NONE; }

    /** Load /index.pkm or display the built-in recovery page. */
    PanelMenuStatus begin()
    {
        queued = Request::HOME;
        const auto status = update();
        if (status == PanelMenuStatus::LOADED) return status;
        PanelMenuPage fallback{};
        strlcpy(fallback.title, "Menu unavailable", sizeof(fallback.title));
        strlcpy(fallback.text[0], "Check the menu storage.", sizeof(fallback.text[0]));
        strlcpy(fallback.text[1], "Use Home to try again.", sizeof(fallback.text[1]));
        replace(fallback, "");
        historySize = 0;
        return PanelMenuStatus::FALLBACK;
    }

    /** Queue a relative or menu-root absolute .pkm path if no request is pending. */
    void request(const char* link)
    {
        if (!hasPending())
            queued = panelMenuResolve(current, link, pending) ? Request::LINK : Request::INVALID;
    }
    /** Queue a named firmware action from an application-defined view. */
    bool requestAction(const char* name)
    {
        if (hasPending() || !panelMenuActionName(name) || strlen(name) >= sizeof(pending)) return false;
        queueAction(name);
        return true;
    }
    void requestBack() { if (!hasPending()) queued = Request::BACK; }
    void requestHome() { if (!hasPending()) queued = Request::HOME; }

    /** Service one request. A failed load preserves the active page and history. */
    PanelMenuStatus update()
    {
        const Request next = queued;
        queued = Request::NONE;
        if (next == Request::NONE) return PanelMenuStatus::IDLE;
        if (next == Request::INVALID) return PanelMenuStatus::INVALID_PATH;
        if (next == Request::ACTION)
        {
            if (!action) return PanelMenuStatus::NO_ACTION_HANDLER;
            char name[PanelMenuPathCapacity];
            strlcpy(name, pending, sizeof(name));
            action(name, context);
            return PanelMenuStatus::ACTION_DISPATCHED;
        }

        if (next == Request::BACK && !historySize) return PanelMenuStatus::NO_HISTORY;
        const char* destination = next == Request::HOME ? "/index.pkm" :
                                  next == Request::BACK ? history[historySize - 1] : pending;
        PanelMenuPage page{};
        const auto status = load(destination, page);
        if (status != PanelMenuStatus::LOADED) return status;
        const PanelMenuView* view = nullptr;
        if (*page.view)
        {
            for (size_t i = 0; i < viewCount; ++i)
                if (views[i] && views[i]->name && !strcmp(views[i]->name, page.view))
                {
                    view = views[i];
                    break;
                }
            if (!view) return PanelMenuStatus::UNKNOWN_VIEW;
            if (!arena || view->size > arenaSize || !view->alignment ||
                uintptr_t(arena) % view->alignment || !view->construct || !view->destroy || !view->update)
                return PanelMenuStatus::VIEW_STORAGE;
        }
        if (next == Request::LINK && *current)
        {
            if (historySize == historyLimit)
            {
                memmove(history, history + 1, sizeof(history[0]) * (historyLimit - 1));
                --historySize;
            }
            strlcpy(history[historySize++], current, sizeof(history[0]));
        }
        else if (next == Request::BACK) --historySize;
        else if (next == Request::HOME) historySize = 0;
        replace(page, destination, view);
        return PanelMenuStatus::LOADED;
    }
};
