export module centui.input;

export namespace centui
{

enum class input_event_type
{
    pointer_moved,
    pointer_pressed,
    pointer_released,
    pointer_left,
    pointer_cancelled,
    focus_lost,
    focus_gained,
};

enum class mouse_button
{
    primary,
    secondary,
    middle,
    other,
};

struct input_event
{
    input_event_type type;
    float            x = 0;
    float            y = 0;
    mouse_button     button = mouse_button::primary;
};

} // namespace centui
