// GLFW将包含它自己的定义，且自动加载Vulkan头文件<vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <cstdlib>

const int WIDTH = 800;
const int HEIGHT = 600;

// 以前Vulkan中有两种类型的验证层：针对instance的和针对device的。Instance层只检查与全局Vulkan对象（如instance）的调用；
// device层只检查与特定GPU的调用。Device层现在已经被废弃了。
const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"  // VK_LAYER_KHRONOS_validation是一个大的验证层集合，系统把所有标准常用的验证层，都融合在一起。
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

// 函数vkCreateDebugUtilsMessengerEXT 用来生成具体的VkDebugUtilsMessengerEXT消息对象。不幸的是，因为这个函数是个扩展函数，
// 它不会自动加载。我们不得不用vkGetInstanceProcAddr函数来查找它的地址。
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");   // 获取函数地址
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger); // 调用vkCreateDebugUtilsMessengerEXT函数生成VkDebugUtilsMessengerEXT消息对象。
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {    // vkDestroyDebugUtilsMessengerEXT 也不会自动加载，需要我们手动查找。
		func(instance, debugMessenger, pAllocator);
	}
}

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
	VkDebugUtilsMessengerEXT debugMessenger;

	void initWindow(){
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // GLFW最初是为OpenGL设计的，我们需要在接下来的函数调用中告诉它不要创建OpenGL上下文
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // 处理大小可变的窗口要消耗更多精力，现在暂且禁用它：

		// 前3个参数标明窗口的宽度、高度和标题。第4个参数允许你可选地标明在哪个显示器上打开窗口，最后一个参数只与OpenGL有关。
		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan window", nullptr, nullptr);

	}

	void initVulkan() {
		createInstance();
		setupDebugMessenger();
	}

	void mainLoop() {
		while (!glfwWindowShouldClose(window)) { //添加一个事件循环, 它循环检查事件（例如点击X按钮），直到窗口被用户关闭为止.
			glfwPollEvents();
		}

	}

	void cleanup() {
		if (enableValidationLayers) {
			DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		}
		vkDestroyInstance(instance, nullptr);
		glfwDestroyWindow(window);  // 一旦窗口被关闭，我们需要销毁资源，关闭GLFW.

		glfwTerminate();

	}

	void createInstance() {
		if (enableValidationLayers && !checkValidationLayerSupport()) {
			throw std::runtime_error("validation layers requested, but not available!");
		}

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

		auto extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size()); // 启用验证层，要接收验证层调试信息，还要启用VK_EXT_debug_utils扩展
			createInfo.ppEnabledLayerNames = validationLayers.data();

			populateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
		}
		else{
			createInfo.enabledLayerCount = 0;   // 我们暂时不需要 验证层。
			createInfo.pNext = nullptr;
		}

		if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
			throw std::runtime_error("failed to create instance!");
		}
	}


	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback; // 指定验证层，用来接收消息的回调函数debugCallback
	}

	void setupDebugMessenger() {
		if (!enableValidationLayers) return;

		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		populateDebugMessengerCreateInfo(createInfo);

		if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
			throw std::runtime_error("failed to set up debug messenger!");
		}
	}


	std::vector<const char*> getRequiredExtensions() {
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		// Vulkan是平台无关的，它不能直接与平台窗体系统通信，为了连接Vulkan和窗体系统,使得被渲染后的结果显示到屏幕上，
		// 我们需要使用WSI扩展(Window System Integration extensions), 这里我们将使用VK_KHR_surface。
		// VK_KHR_surface扩展是一个instance级的扩展，它包含在glfwGetRequiredInstanceExtensions返回的列表中。该列表还包括一些其他WSI扩展.
		// 参数glfwExtensionCount返回实例扩展的数量。函数返回值glfwExtensions是实例扩展的名称数组。
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		// 用glfwExtensions的第0...glfwExtensionCount-1个元素来初始化。类似vector<int> tmp(vec.begin(), vec.begin() + 3) 用向量vec的第0个到第2个值初始化tmp
		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		// 仅仅启用验证层，并没什么用，因为现在还没办法将调试消息转回给程序。为了接收消息，我们不得不用回调设置一个debug信使，这就要使用VK_EXT_debug_utils 扩展。
		if (enableValidationLayers) {
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);  // 注意，这里用的是VK_EXT_DEBUG_UTILS_EXTENSION_NAME宏，等于字符串"VK_EXT_debug_utils"
		}

		return extensions;
	}


	bool checkValidationLayerSupport() {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);   // 获得所有可用验证层的数量

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers) {   // 检查是否validationLayers 中所有的层都存在于availableLayers 列表中。
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}

		return true;
	}

	// 启用验证层后，定义一个用来接收消息的回调函数debugCallback，以PFN_vkDebugUtilsMessengerCallbackEXT 为原型。VKAPI_ATTR和VKAPI_CALL确保函数有正确的签名
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,  // 消息的严重性如：VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT, INFO, WARNING, ERROR等。
		VkDebugUtilsMessageTypeFlagsEXT messageType,             // 消息类型，如：VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT 表示和性能有关。
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,  // 消息细节结构体，成员: pMessage以\0结尾的字符串, pObjects相关消息的对象的数组; objectCount对象数组长度
		void* pUserData) {                                          // 参数pUserData 包含在设置回调函数的指针，允许你传入自己的数据。
		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
		return VK_FALSE;   // 回调函数返回一个bool值，表明触发了验证层消息的调用，是否应该被中止。如果返回true，那么调用就被中止。
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
