

// for getcwd
#ifndef _MSC_VER
#include <unistd.h>
#else
#include <direct.h>
#endif

#include "defines.h"
#include "vulkan_main.h"
#include "tiny/tiny_log.h"

u32 SCREEN_WIDTH = 800; // default
u32 SCREEN_HEIGHT = 600;

GLFWwindow* glob_glfw_window = nullptr;

void framebuffer_resize_callback(GLFWwindow* window, s32 width, s32 height) 
{
    
}

void mouse_callback(GLFWwindow* window, f64 xpos, f64 ypos) 
{

}

bool should_close_window(GLFWwindow* window) 
{
    glfwPollEvents();
    return glfwWindowShouldClose(window);
}

void initWindow()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // need to tell glfw we are not using Opengl
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // TODO: to be implemented later
    glob_glfw_window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Vulkan", nullptr, nullptr);
}

void mainLoop()
{
    // close window on esc key
    if (glfwGetKey(glob_glfw_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(glob_glfw_window, true);
    }
}

int main(int argc, char** argv)
{
    s8 cwd[PATH_MAX];
    getcwd(cwd, PATH_MAX);
    LOG_INFO("CWD: %s", cwd);
    initWindow();
    RuntimeData runtime = initVulkan();
    while(!should_close_window(glob_glfw_window)) 
    {
        mainLoop();
        vulkanMainLoop(runtime);
    }
    vulkanCleanup(runtime);
    glfwDestroyWindow(glob_glfw_window);
    glfwTerminate();
    return 0;
}