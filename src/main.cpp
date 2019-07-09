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
    VkInstance instance;

    void initWindow(){
		glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // GLFW最初是被设计为创建OpenGL上下文而用的，我们需要在接下来的函数调用中告诉它不要创建OpenGL上下文
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);    // 处理大小可变的窗口要消耗更多精力，现在暂且禁用它：

        // 前3个参数标明窗口的宽度、高度和标题。第4个参数允许你可选地标明在哪个显示器上打开窗口，最后一个参数只与OpenGL有关。
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan window", nullptr, nullptr);

    }

    void initVulkan() {
        createInstance();

    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) { //添加一个事件循环, 它循环检查事件（例如点击X按钮），直到窗口被用户关闭为止.
            glfwPollEvents();
        }

    }

    void cleanup() {
		vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);  // 一旦窗口被关闭，我们需要销毁资源，关闭GLFW.

        glfwTerminate();

    }

    void createInstance() {
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo = {};  // 主要用来声明我们想要使用哪些全局扩展和验证层。这里的全局是指整个程序，不是某个device。
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
		// Vulkan是平台无关的，它不能直接与平台窗体系统通信，为了连接Vulkan和窗体系统,使得被渲染后的结果显示到屏幕上，
		// 我们需要使用WSI扩展(Window System Integration extensions), 这里我们将使用VK_KHR_surface。
        // VK_KHR_surface扩展是一个instance级的扩展，它包含在glfwGetRequiredInstanceExtensions返回的列表中。该列表还包括一些其他WSI扩展.
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount); // 参数glfwExtensionCount返回实例扩展的数量。函数返回值glfwExtensions是实例扩展的名称数组。

        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;

        createInfo.enabledLayerCount = 0;   // 我们暂时不需要 验证层。

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
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
