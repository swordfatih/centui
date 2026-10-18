export module centui.ui;

import std;
export import centui.element;
export import centui.viewport;

export namespace centui
{

// Borrows a concrete backend; no graphics or window dependency.
// Backend supplies viewport(), glyphs(), and synchronous draw(commands).
template <typename backend_type>
class ui
{
public:
    explicit ui(backend_type& backend) : _backend(backend)
    {
    }

    void render(element& root)
    {
        const auto [width, height] = _backend.viewport();
        if(width <= 0 || height <= 0)
        {
            return;
        }
        const auto list = prepare(root, _backend.glyphs(), width, height);
        _backend.draw(list);
    }

private:
    backend_type& _backend;
};

} // namespace centui
