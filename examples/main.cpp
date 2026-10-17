import centui.window;

int main()
{
    centui::window window{"CentUI", 800, 600};

    while(window.pollEvents())
    {
        if(window.beginFrame())
        {
            window.endFrame();
        }
    }
}