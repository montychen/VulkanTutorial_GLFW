// GLFW将包含它自己的定义，且自动加载Vulkan头文件<vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <optional>
#include <set>

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



struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily; // 有的设备支持绘制命令 drawing commands， 不代表就一定支持呈现，也就是将image呈现到我们创建的surface窗口上。
    std::optional<uint32_t> presentFamily;	// 所以需要把支持呈现presentation作为选取设备时的一个条件。presentation是个队列相关的特性，这个问题实际上是要找到一个队列家族，其支持呈现到我们创建的surface。

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();	// 确保同时满足绘制和呈现这两个条件。 这里没有确保她们是同一个队列。
    }
};


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
	VkSurfaceKHR surface;


	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device;			// 在选择了要用的物理设备后，我们需要设置一个逻辑设备，用以与其交互。

	VkQueue graphicsQueue;
	VkQueue presentQueue;

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
		createSurface();		// 窗口surface需要在instance被创建后立即被创建，因为它实际上会影响物理设备的选择。
		pickPhysicalDevice();
		createLogicalDevice();
	}

	void mainLoop() {
		while (!glfwWindowShouldClose(window)) { //添加一个事件循环, 它循环检查事件（例如点击X按钮），直到窗口被用户关闭为止.
			glfwPollEvents();
		}

	}

	void cleanup() {
		vkDestroyDevice(device, nullptr);

		if (enableValidationLayers) {
			DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		}

		vkDestroySurfaceKHR(instance, surface, nullptr);
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


	// 尽管VkSurfaceKHR 对象及其用法是平台无关的，它的创建过程却不是平台无关的，因为它依赖窗口系统的细节。例如，在Windows上它需要HWND 和HMODULE 。
	// 因此有一个平台相关的扩展，在Windows上是VK_KHR_win32_surface ，它也被自动包含在由glfwGetRequiredInstanceExtensions得到的列表中了。
	// GLFW实际上有glfwCreateWindowSurface ，它处理了平台相关的差异。
	void createSurface() {
		if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface!");
		}
	}


	// 通过VkInstance 初始化了Vulkan库之后，需要在系统中查找和选择一个支持我们需要的特性的图形卡。实际上可以选择任意多个图形卡，同步地使用它们，这里我们只使用第一个满足我们需要的图形卡。
	void pickPhysicalDevice() {
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr); // 枚举图形卡，先获取数量。

		if (deviceCount == 0) {	// 如果支持Vulkan的设备数量为0，那么就没有必要继续了。
			throw std::runtime_error("failed to find GPUs with Vulkan support!");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount); // 申请一个数组来记录所有的VkPhysicalDevice 句柄。
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		for (const auto& device : devices) {
			if (isDeviceSuitable(device)) {
				physicalDevice = device; // 这里我们只使用第一个满足我们需要的图形卡。
				break;
			}
		}

		if (physicalDevice == VK_NULL_HANDLE) {
			throw std::runtime_error("failed to find a suitable GPU!");
		}
	}


	void createLogicalDevice() {
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

		float queuePriority = 1.0f; // 队列赋予优先级（0.0到1.0的浮点数）
		for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo = {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures = {};

        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = 0;

		if (enableValidationLayers) { // 基于逻辑设备的验证层已经废弃，这里只是为了兼容性才进行设置，其实完全可以不设置，直接把0赋给enabledLayerCount就可以了。
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		} else {
			createInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) { // 队列随着逻辑设备的创建而自动创建
			throw std::runtime_error("failed to create logical device!");
		}

		vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue); // 用vkGetDeviceQueue函数从队列家族获取队列句柄，并保存在graphicsQueue。因为我们只创建一个队列，用索引0即可。
		vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
	}


	// 检查设备，是否满足我们的需要，因为不是所有的图形卡都一样。
	bool isDeviceSuitable(VkPhysicalDevice device) {
		QueueFamilyIndices indices = findQueueFamilies(device); // 任何操作，从绘画到上传纹理，都要将命令提交到队列。有不同的队列，这里是选择我们需要的队列。

		return indices.isComplete();
	}

	// vkGetPhysicalDeviceProperties： 获取物理设备的基本属性，如名字、类型和支持的Vulkan版本
	// vkGetPhysicalDeviceFeatures：获取物理设备的可选特性，如：纹理压缩、64位浮点数和多视口渲染
	// vkGetPhysicalDeviceQueueFamilyProperties: 获取物理设备的 队列属性
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		for (const auto& queueFamily : queueFamilies) {
			if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) { // 现在我们将只查找支持图形命令的队列
				indices.graphicsFamily = i;
			}

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport); // 查询能够呈现到窗口surface的队列家族，该函数以物理设备，队列家族索引和surface为参数。

			if (queueFamily.queueCount > 0 && presentSupport) {
				indices.presentFamily = i; // 支持绘制和呈现的队列不一定是同一个。 可以考虑添加一段逻辑来显式地选择一个物理设备，其在同一队列中同时支持绘制和presentation，以获得更好的性能。
			}

			if (indices.isComplete()) {
				break;
			}

			i++;
		}

		return indices;
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
