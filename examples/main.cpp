import centui.window;

int main()
{
    centui::window window{"CentUI", 800, 600};

    while(window.poll_events())
    {
        if(window.begin_frame())
        {
            window.end_frame();
        }
    }
}