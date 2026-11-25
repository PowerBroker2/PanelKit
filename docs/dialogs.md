# Modal dialogs

[← Back to PanelKit README](../README.md#documentation)

Use `PanelDialog<W, H>` to open a modal from a button callback, an input event, or application code. A dialog is a panel, so its contents can be ordinary PanelKit widgets such as text, pictures, text fields, sliders, or nested layouts. The dialog starts hidden and uses a 24-pixel title bar. Child coordinates are relative to its content area below the title bar.

## Open a dialog and handle its result

This example opens an editable name when a button is clicked and applies the new name only when the user saves. The sketch uses an existing bitmap font through `font` and an existing activated `screen`.

```cpp
// Declare screen before its children.
Panel<320, 240, false> screen;
PanelButton<110, 26, false> editButton(&screen);
PanelDialog<240, 144> editor(&screen);
PanelText prompt(220, 24, &editor);
PanelTextField draft(220, 28, &editor);
PanelButton<88, 26, false> cancelButton(&editor), saveButton(&editor);
char savedName[65] = "Input A";

void startEdit()
{
    draft.setText(savedName);
    editor.open(&draft);
}

void cancelEdit() { editor.reject(); }
void saveEdit() { editor.accept(); }

void applyEdit(int result, void*)
{
    if (result == editor.Accepted)
        strlcpy(savedName, draft.getText(), sizeof(savedName));
}

void configureEditor(const BitmapFont& font)
{
    PanelTheme theme;
    theme.font = &font;
    editor.setTitle("Rename input");
    editor.setTheme(theme);
    editor.onFinished(applyEdit);

    prompt.setLoc(10, 5);
    prompt.setTheme(theme);
    prompt.setText("Enter the new input name.");
    prompt.activate();

    draft.setLoc(10, 33);
    draft.setTheme(theme);
    draft.activate();

    cancelButton.setLoc(10, 77);
    cancelButton.setFont(font, 0.72f).setText("Cancel").onClick(cancelEdit);
    cancelButton.activate();

    saveButton.setLoc(142, 77);
    saveButton.setFont(font, 0.72f).setText("Save").onClick(saveEdit);
    saveButton.activate();

    editButton.setLoc(10, 10);
    editButton.setFont(font, 0.72f).setText("Edit name").onClick(startEdit);
    editButton.activate();
}
```

Call `configureEditor(font)` during setup and continue polling input and presenting the screen in the normal loop. For an image, add a `PanelPicture` child and call `setPixmap()` or `setSource()` with storage that stays alive as long as the picture uses it. Keep draft widget data separate from committed application state if Cancel should discard edits.

## Lifecycle and input

| API | Behavior |
| --- | --- |
| `open()` or `open(&child)` | Shows the dialog and returns immediately. The optional focus target must be a visible, active direct child. Returns false if the dialog is already open, its parent already has an active modal, the dialog lacks an active parent panel, the dialog is an overlay, or the focus target is invalid. |
| `onFinished(handler, context)` | Calls `handler(int result, void* context)` once after the dialog is hidden and the modal slot is released. The application owns the context and child widgets can still be read in the callback. |
| `accept()` | Finishes with `Accepted` (1). |
| `reject()` | Finishes with `Rejected` (0). Escape, the title-bar close button, and hiding an open dialog also reject it. |
| `finish(result)` | Finishes with a custom integer result, for example 7 for a third action. Returns false if the dialog is already closed. |
| `setAcceptOnEnter(true)` | Lets CR or LF accept the open dialog. By default typing is forwarded to the focused widget. |
| `setCenterOnOpen(false)` | Preserves the position you set with `setLoc()`. Otherwise the dialog centers in the parent viewport on open and after a parent resize. |

Attach an application-wide dialog to the root panel so it blocks background input across the screen. A nested parent limits modal scope to its own registry. The parent allows one active modal at a time. To show another dialog in the same scope, open it from the first dialog’s result handler. If a dialog owns an open child dialog, Escape goes to the child first. Programmatically finishing the parent closes its child modal. Background clicks outside the modal leave its keyboard focus intact. Completing the dialog dismisses its popups and restores the parent’s previous focus when that target is still interactive. Unlinking or destroying the dialog removes it without invoking the result handler, so call `reject()` first if the application needs a cancellation callback.

Touchscreen, USB Serial, and software-generated input follow the same modal rules. For keyboard input through the serial protocol, use `key 27` for Escape and `key 13` for Enter. If you draw a cursor as a component, `cursor.setOverlay(true)` keeps it visible above dialogs and popups without taking input.

## Verify the example

The [serial walkthrough](serial-walkthrough.md) opens a dialog that has an image, message, text field, spin box, and three buttons. Its checks cover initial focus, background blocking, committed and discarded edits, custom result codes, and dismissal through Escape, Enter, and the title bar.
