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

The [task-list example](https://github.com/swordfatih/centui/blob/main/tests/examples/task_list/main.cpp) connects submission, completion, removal, and scrolling in one app.

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
