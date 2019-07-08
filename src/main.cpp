// GLFW将包含它自己的定义，且自动加载Vulkan头文件<vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <functional>
#include <cstdlib>

const int WIDTH = 800;
const int HEIGHT = 600;

class HelloTriangleApplication {
public:
    void run() {
		initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window;
    
    void initWindow(){
		glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // GLFW最初是被设计为创建OpenGL上下文而用的，我们需要在接下来的函数调用中告诉它不要创建OpenGL上下文
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);    // 处理大小可变的窗口要消耗更多精力，现在暂且禁用它：

        // 前3个参数标明窗口的宽度、高度和标题。第4个参数允许你可选地标明在哪个显示器上打开窗口，最后一个参数只与OpenGL有关。
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan window", nullptr, nullptr);

    }

    void initVulkan() {

    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) { //添加一个事件循环, 它循环检查事件（例如点击X按钮），直到窗口被用户关闭为止.
            glfwPollEvents();
        }

    }

    void cleanup() {
        glfwDestroyWindow(window);  // 一旦窗口被关闭，我们需要销毁资源，关闭GLFW.
        
        glfwTerminate();

    }

 
    
};

int main() {
    HelloTriangleApplication app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
