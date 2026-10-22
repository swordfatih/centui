#include <format>
#include <initializer_list>
#include <memory>
#include <stdexcept>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

import centui;
import centui.glfw;
import centui.bgfx;

namespace
{
struct glfw_context
{
    glfw_context()
    {
        if(!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW");
        }
    }

    ~glfw_context()
    {
        glfwTerminate();
    }
};

struct screen
{
    centui::length        width = centui::percent(100);
    centui::length        height = centui::percent(100);
    centui::alignment     align_items = centui::alignment::center;
    centui::justification justify_content = centui::justification::center;
    centui::states_color  background_color = centui::states_color::all({15, 23, 42, 255});
};

struct card
{
    centui::length       width = centui::px(360);
    centui::edges        padding = centui::edges::all(centui::px(28));
    centui::length       gap = centui::px(18);
    centui::states_color background_color = centui::states_color::all({30, 41, 59, 255});
};

struct title
{
    float font_size = 28;
};

struct action
{
    centui::length height = centui::px(44);
    centui::edges  padding = centui::edges::all(centui::px(12));
    struct
    {
        centui::color normal{37, 99, 235, 255};
        centui::color hover{59, 130, 246, 255};
        centui::color pressed{29, 78, 216, 255};
    } background_color;
};
} // namespace

int main()
{
    glfw_context glfw;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> window{
        glfwCreateWindow(800, 520, "CentUI + GLFW + bgfx", nullptr, nullptr), glfwDestroyWindow};
    if(!window)
    {
        throw std::runtime_error("Failed to create GLFW window");
    }

    // clang-format off
    glfwSetKeyCallback(window.get(), [](GLFWwindow* native, int key, int, int action, int)
    {
        if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(native, GLFW_TRUE);
        }
    });
    // clang-format on

    centui::glfw::platform platform{*window};
    centui::bgfx::backend renderer{platform};
    auto count = std::make_shared<int>(0);
    auto label = centui::text{"0 clicks"};

    // clang-format off
    auto root = centui::container<screen>{
        centui::container<card>{
            centui::text<title>{"Hello, GLFW."},
            centui::text{"GLFW owns the window. bgfx draws CentUI."},
            label,
            centui::button<action>{"Count a click", {
                .on_click = [count, label]() mutable
                {
                    label.set_text(std::format("{} clicks", ++*count));
                },
            }},
        },
    };
    // clang-format on

    centui::document document{root};
    document.set_font(centui::font{CENTUI_FONT_PATH});
    centui::run(platform, renderer, document);
}
