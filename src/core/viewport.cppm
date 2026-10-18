export module centui.viewport;

export namespace centui
{

// Pixel dimensions, updated by the window or host before UI rendering.
struct viewport
{
    int width = 0;
    int height = 0;
};

} // namespace centui
