---
layout: default
title: "3. State and interaction"
nav_order: 4
---

# State and interaction

Callbacks connect controls to your app. Keep the state where it belongs, retain handles for widgets you update, and change the existing tree as the user works.

## Connect a control

An input reports text changes. Here a second widget previews what the user types:

```cpp
auto preview = centui::text{"Untitled"};

auto name = centui::input{
    "",
    {
        .on_change = [preview](std::string_view value)
        {
            preview->set_text(std::string{value});
        },
    }
};
```

`input` also has `on_submit`, which lets Enter trigger the same action as a button. A `toggle` reports a `bool`, a `range` reports a number, and a `selection` reports the selected index.

If a callback needs your model, capture a reference whose lifetime covers the UI. The counter in the [first guide](getting-started.md#write-the-app) keeps its state in `main`, above the document and loop. Shared ownership is only needed when the state really has a shared lifetime.

## Add and remove rows

Keep the list container, then append a row when the user submits a task:

```cpp
auto tasks = centui::container{};

auto task = centui::container{
    centui::toggle{"Write the release notes"},
};

tasks->append(task);
```

Call `tasks->remove(task)` to remove it. The document commits tree changes requested by input callbacks at the dispatch boundary; you do not need to rebuild the document or run layout yourself.

## Display a large list

Use `list` for a large collection. Give it an item count, a fixed row height in logical pixels, and a factory that returns a fresh widget for an index:

```cpp
auto contacts = centui::list{
    [&names](std::size_t index)
    {
        return centui::text{names[index]};
    },
    {
        .item_count = names.size(),
        .row_height = 32,
    }
};
contacts->patch_style({.width = centui::percent(100), .height = centui::px(320)});
```

Here `names` is your application's collection of strings. Keep it alive while the list uses it. Give the list a bounded height, either explicitly or through a flex layout. It scrolls itself; do not wrap it in another `scroll`.

The list creates only rows inside its viewport plus two extra rows on each side by default (`overscan`). Rows that remain in that range retain their widgets. Rows that leave are removed, and focus inside a removed row is cleared. Keep persistent values, such as checked states or edited text, in your application data and initialize each row from that data. A factory must return a detached widget and must not modify its list.

Call `set_item_count(names.size())` when the collection grows or shrinks. Existing rows are identified by index. After changing existing values, inserting, removing, or reordering items, also call `refresh()` to recreate the current rows. `scroll_to(index)` reveals an item, and `rendered_range()` reports the half-open range of mounted indexes after preparation. Tab traverses mounted controls; this first version does not provide collection-wide selection or variable-height rows.

Use ordinary `scroll` for an existing set of controls that should stay alive.

## Avoid keeping removed widgets alive

A parent owns its children. A child's callback should observe its own widget or an ancestor through a weak handle:

```cpp
auto remove = centui::button{"Remove"};

remove->set_on_click(
    [list = tasks.weak(), row = task.weak()]
    {
        auto parent = list.lock();
        auto child = row.lock();

        if(parent.valid() && child.valid())
        {
            parent->remove(child);
        }
    }
);

task->append(remove);
```

The [task-list example](https://github.com/swordfatih/centui/blob/main/tests/examples/task_list/main.cpp) uses `list` to connect submission, completion, removal, and scrolling. It keeps task values in application data so completion survives rows being recreated.

## Focus and dialogs

Standard controls support keyboard focus. Tab moves between eligible controls; `request_focus()` asks to focus a specific widget. A focused control in a scroll container is revealed when possible.

Use `popup` for a dialog. Add it to the tree, call `open()` when needed, and call `close()` from its actions. It restricts focus to the dialog while open and restores previous focus when closed. Escape closes it. Use weak handles for actions inside the popup that refer back to it.

## Reach the document from a callback

Callbacks can accept an optional first argument, `centui::event&`:

```cpp
auto large_text = centui::toggle{
    "Large text",
    {
        .on_change = [](centui::event& event, bool enabled)
        {
            event.document().set_style({.font_size = enabled ? 22.0f : 16.0f});
        },
    }
};
```

`event.target()` identifies the control; `event.document()` gives access to its document. Keep those borrowed event references inside the callback.

[← Widgets and styles](widgets-and-styles.md) · [Next: text and fonts →](text.md)
