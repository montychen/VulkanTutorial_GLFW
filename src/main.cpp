// GLFW将包含它自己的定义，且自动加载Vulkan头文件<vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <optional>
#include <set>

const int WIDTH = 800;
const int HEIGHT = 600;

// 以前Vulkan中有两种类型的验证层：针对instance的和针对device的。Instance层只检查与全局Vulkan对象（如instance）的调用； device层只检查与特定GPU的调用。Device层现在已经被废弃了。
const std::vector<const char*> validationLayers = {
"VK_LAYER_KHRONOS_validation"  // VK_LAYER_KHRONOS_validation是一个大的验证层集合，系统把所有标准常用的验证层，都融合在一起。
};


// 交换链本质上是一个等待 *呈现display* 到屏幕的图像队列, 用来确保当前正在 *渲染绘制render* 的image与当前正在呈现到屏幕的，不是同一个, 保证image完全渲染完毕后才能进行显示十分重要
const std::vector<const char*> deviceExtensions = {
VK_KHR_SWAPCHAIN_EXTENSION_NAME // 宏VK_KHR_SWAPCHAIN_EXTENSION_NAME，它被定义为VK_KHR_swapchain。用这个宏的优点是，编译器会捕捉到拼写错误。
};


#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif


// 函数vkCreateDebugUtilsMessengerEXT 用来生成具体的VkDebugUtilsMessengerEXT消息对象。不幸的是，因为这个函数是个扩展函数，它不会自动加载。我们不得不用vkGetInstanceProcAddr函数来查找它的地址。
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
    std::optional<uint32_t> graphicsFamily; // 有的设备支持绘制命令 drawing commands， 不代表就一定支持呈现，也就是将image呈现present到我们创建的surface窗口上。
    std::optional<uint32_t> presentFamily;	// 所以需要把支持呈现presentation作为选取设备时的一个条件。presentation是个队列相关的特性，这个问题实际上是要找到一个队列家族，其支持呈现到我们创建的surface。

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();	// 确保同时满足绘制和呈现这两个条件。 这里没有确保她们是同一个队列。
    }
};


// 仅仅检查交换链是否可用，不够充分，因为它可能和窗口surface不兼容。我们还需要检查这3个属性
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;	 // 基础surface功能（交换链包含的image的最大\小数量，image的宽高的最大\最小值）
    std::vector<VkSurfaceFormatKHR> formats; // Surface格式（像素格式、颜色空间）, 包含format和colorSpace成员变量。 VK_FORMAT_B8G8R8A8_UNORM像素用32位表示。SRGB颜色空间 VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
    std::vector<VkPresentModeKHR> presentModes; //  可用的presentation模式: VK_PRESENT_MODE_IMMEDIATE_KHR、FIFO、FIFO_RELAXED、MAILBOX
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

        VkSwapchainKHR swapChain;
        std::vector<VkImage> swapChainImages;
        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent;
        std::vector<VkImageView> swapChainImageViews;

        VkRenderPass renderPass;
        VkPipelineLayout pipelineLayout;
        VkPipeline graphicsPipeline;

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
            createSwapChain();
            createImageViews();
            createRenderPass();
            createGraphicsPipeline();
        }

        void mainLoop() {
            while (!glfwWindowShouldClose(window)) { //添加一个事件循环, 它循环检查事件（例如点击X按钮），直到窗口被用户关闭为止.
                glfwPollEvents();
            }

        }

        void cleanup() {
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            vkDestroyRenderPass(device, renderPass, nullptr);

            for (auto imageView : swapChainImageViews) {
                vkDestroyImageView(device, imageView, nullptr);
            }

            vkDestroySwapchainKHR(device, swapChain, nullptr);
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
            if (enableValidationLayers && !checkValidationLayerSupport()) { // 检查 VK_LAYER_KHRONOS_validation
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

            // 实例层验证VK_LAYER_KHRONOS_validation默认在终端输出所有验证信息。可以启用实例扩展VK_EXT_debug_utils，然后自定义一个回调函数，就可只输出我们感兴趣的信息。
            auto extensions = getRequiredExtensions(); // 包含3个扩展 VK_KHR_surface, VK_MVK_macos_surface,  VK_EXT_debug_utils
            createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
            createInfo.ppEnabledExtensionNames = extensions.data();

            VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
            if (enableValidationLayers) {
                createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size()); // 启用验证层VK_LAYER_KHRONOS_validation
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
            QueueFamilyIndices indices = findQueueFamilies(physicalDevice); // 要在窗口中显示图片，队列就要支持绘制和呈现，也就是：VK_QUEUE_GRAPHICS_BIT  和 vkGetPhysicalDeviceSurfaceSupportKHR 检查surface是否支持图片显示。

            std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
            std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

            float queuePriority = 1.0f; // 队列优先级（0.0到1.0的浮点数）, 值越大优先级越高。一般设成1.0
            for (uint32_t queueFamily : uniqueQueueFamilies) {
                VkDeviceQueueCreateInfo queueCreateInfo = {};
                queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueCreateInfo.queueFamilyIndex = queueFamily; // 队列家族（通过它创建队列）的索引, 队列与设备一同自动创建。
                queueCreateInfo.queueCount = 1; //我们希望在特定队列家族中启用的队列数量
                queueCreateInfo.pQueuePriorities = &queuePriority;
                queueCreateInfos.push_back(queueCreateInfo);
            }

            VkPhysicalDeviceFeatures deviceFeatures = {};

            VkDeviceCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

            createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()); // 不同队列家族的数量，我们通过它创建队列和设备。
            createInfo.pQueueCreateInfos = queueCreateInfos.data();

            createInfo.pEnabledFeatures = &deviceFeatures; // 希望启用的 vkGetPhysicalDeviceFeatures 特性， 比如几何着色器。

            createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()); // 启用交换链扩展VK_KHR_swapchain，显示图片这个逻辑设备扩展一定要
            createInfo.ppEnabledExtensionNames = deviceExtensions.data();

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



        void createSwapChain() {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);  //获取surface的 capabilities、formats、presentModes

            // if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats); // 像素格式和颜色空间：VK_FORMAT_B8G8R8A8_UNORM像素用32位表示。SRGB颜色空间VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes); //  优先使用VK_PRESENT_MODE_MAILBOX_KHR 三重缓冲
            VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities); // extent 是SwapChain中image的宽高，分辨率(resolution), 通常它与window的尺寸一样

            // VkSurfaceCapabilitiesKHR包含交换链渲染时允许的最大最小的image图像数量， image的宽高的最大/最小值
            uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1; // 交换链中有多少image。使用最小值，在请求另一个image渲染前，可能有时不得不等待driver完成内部操作。因此推荐至少比最小值多1
            if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) { // 确保不超过image的最大值，而0是个特殊值，意思没限制。
                imageCount = swapChainSupport.capabilities.maxImageCount;
            }

            VkSwapchainCreateInfoKHR createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            createInfo.surface = surface;

            createInfo.minImageCount = imageCount; // 交换链至少提供多少个image, 推荐比capabilities的最小值多 1： capabilities.minImageCount + 1;
            createInfo.imageFormat = surfaceFormat.format; // format指定颜色通道和存储类型，VK_FORMAT_B8G8R8A8_UNORM像素用32位表示
            createInfo.imageColorSpace = surfaceFormat.colorSpace;  // colorSpace用来表示SRGB颜色空间是否被支持 VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            createInfo.imageExtent = extent;
            createInfo.imageArrayLayers = 1; // 表示image的层次，除非创建3D应用，否则这个值将为1.
            createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // imageUsage指明Swap Chain里的image我们拿来做什么，在本例中将直接对image进行渲染，这就意味着Image将被当做颜色附件使用(color attachment)。如果你想先渲染一个单独的图片然后再进行处理，那就应该使用VK_IMAGE_USAGE_TRANSFER_DST_BIT并使用内存转换操作将渲染好的image 转换到SwapChain里。

            QueueFamilyIndices indices = findQueueFamilies(physicalDevice); // 在窗口显示图片，队列要支持绘制和呈现，也就是：VK_QUEUE_GRAPHICS_BIT和vkGetPhysicalDeviceSurfaceSupportKHR检查surface是否支持图片显示
            uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

            // 如果grapics queue 和 present queue不相同，会出现多种队列访问image的情况：我们在grapics queue中绘制image,然后将它提交到presention queue等待显示。
            if (indices.graphicsFamily != indices.presentFamily) {
                createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // VK_SHARING_MODE_CONCURRENT：。Image可以在多个队列家族使用，无需显式地转移所有权。
                createInfo.queueFamilyIndexCount = 2;  // 并发模式要求你用queueFamilyIndexCount 和pQueueFamilyIndices 参数，提前标明，所有权将在哪些队列家族中共享。
                createInfo.pQueueFamilyIndices = queueFamilyIndices;
            } else {
                createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // VK_SHARING_MODE_EXCLUSIVE：一个image同一时间只能属于一个队列家族，所有权必须被显式地转移后，才能在另一个队列家族使用。这个选项提供最佳性能。
            }

            createInfo.preTransform = swapChainSupport.capabilities.currentTransform; // currentTransform说明交换链里的mage不需要变换。 如果surface支持变换如，90度或水平翻转，可以标明让它应用到交换链中的image
            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // 在和其它窗口混合的时候，忽略Alpha透明通道，
            createInfo.presentMode = presentMode;
            createInfo.clipped = VK_TRUE;    // 不处理那些被遮挡的部分， 可以得到更好的性能

            createInfo.oldSwapchain = VK_NULL_HANDLE; // 窗口大小改变时，交换链会失效，需要重新创建. 这里要明确的指向旧的交换链。

            if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
                throw std::runtime_error("failed to create swap chain!");
            }

            vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr); // 交换链在创建的过程中，也会自动创建至少minImageCount个VkImage。
            swapChainImages.resize(imageCount);
            vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data()); // 从交换链中获取已经创建的VkImage

            swapChainImageFormat = surfaceFormat.format;
            swapChainExtent = extent;
        }

        void createImageViews() { // ImageView描述了如何访问Image以及访问图像的哪一部分，要在渲染管道中使用任何VkImage（包括交换链中的VkImage），必须创建一个VkImageView对象。
            swapChainImageViews.resize(swapChainImages.size());

            for (size_t i = 0; i < swapChainImages.size(); i++) { //  为交换链中每个图像创建一个ImageView
                VkImageViewCreateInfo createInfo = {};
                createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                createInfo.image = swapChainImages[i];
                createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // 将图像视为1D纹理，2D纹理，3D纹理 、立方体贴图
                createInfo.format = swapChainImageFormat;    // VK_FORMAT_B8G8R8A8_UNORM
                createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY; // components允许你将颜色通道混合起来，比如，将所有通道都映射到红色通道，形成单色材质。我们这里用默认映射
                createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
                createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // subresourceRange描述图像用途以及哪些部分能被访问，我们的图像被用作颜色目标，不用任何mipmap层级
                createInfo.subresourceRange.baseMipLevel = 0;    // 一系列mipmap缩略图的编号即为 mip level。level 0为原图, 之后的每一个level 都比上一个level长宽缩减到一半
                createInfo.subresourceRange.levelCount = 1;
                createInfo.subresourceRange.baseArrayLayer = 0;
                createInfo.subresourceRange.layerCount = 1; // 如果开发3D应用，那么要创建具有多层的交换链。然后你可以为每个图像创建多个图像视图，通过访问不同的层来表示左右眼的视图

                if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create image views!");
                }
            }
        }

        // 在完成pipeline管道创建之前，需要用render pass来告诉Vulkan，渲染时将要使用的帧缓冲附件framebuffer attachments: 包括需要多少个颜色和深度缓冲区，每个缓冲区使用多少个采样样本，以及在整个渲染操作中如何处理它们的内容。
        void createRenderPass() {
            VkAttachmentDescription colorAttachment = {};  // 只有一个颜色缓冲附件，由交换链中的一个图像表示
            colorAttachment.format = swapChainImageFormat; //  VK_FORMAT_B8G8R8A8_UNORM
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // 使用1个样本。  loadOp和storeOp设置在渲染之前和渲染之后如何处理附件中的数据。loadOp和storeOp适用于颜色和深度数据
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // 使用clear操作在绘制新帧之前将帧缓冲区清除为黑色。 VK_ATTACHMENT_LOAD_OP_CLEAR：在开始时以一个常量清理附件内容; LOAD_OP_LOAD：保留附件的现有内容; LOAD_OP_DONT_CARE：现有内容未定义，忽略它们
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // 希望在屏幕上看到渲染后的三角形，所以这里使用store操作。VK_ATTACHMENT_STORE_OP_STORE：渲染内容将存储在内存中，方便之后读取； STORE_OP_DONT_CARE：渲染操作后，帧缓冲区的内容变为undefined
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // stencilLoadOp/stencilStoreOp适用于模板数据。我们的应用程序不会对模板缓冲区执行任何操作，因此相应的加载和存储结果都无关紧要。
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;     // initialLayout指定了渲染通道开始之前图像将具有的布局, VK_IMAGE_LAYOUT_UNDEFINED意味着不关心图像之前的布局。因为都要清除它的。
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // 希望在图像渲染完毕后使用交换链呈现，所以finalLayout设置为VK_IMAGE_LAYOUT_PRESENT_SRC_KHR：图像在交换链中呈现; COLOR_ATTACHMENT_OPTIMAL：图像用作颜色附件; TRANSFER_DST_OPTIMAL：图像作为目标，用于内存复制操作

            VkAttachmentReference colorAttachmentRef = {};
            colorAttachmentRef.attachment = 0; // 指定我们要使用的附件索引， 本例只有一个附件，所以是0
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; //  在启动子通道时，Vulkan会自动将附件转换为此布局。我们打算将附件用作颜色缓冲区里VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL

            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Vulkan可能在未来支持计算子通道，因此我们必须明确将其指定为图形子通道
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorAttachmentRef; // 指定对颜色附件的引用.子通道还可以引用以下其他类型的附件 pInputAttachments：从着色器读取附件；pResolveAttachments：用于多重采样颜色附件的附件； pDepthStencilAttachment：用于深度和模板数据的附件； pPreserveAttachments：此子通道未使用但必须保留数据的附件； 

            VkRenderPassCreateInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.attachmentCount = 1;
            renderPassInfo.pAttachments = &colorAttachment;
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;

            if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
                throw std::runtime_error("failed to create render pass!");
            }
        }


        void createGraphicsPipeline() {
            auto vertShaderCode = readFile("shaders/vert.spv");
            auto fragShaderCode = readFile("shaders/frag.spv");

            VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
            VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

            VkPipelineShaderStageCreateInfo vertShaderStageInfo = {}; // 将着色器模块分配给管道中的顶点或片段着色器阶段是通过VkPipelineShaderStageCreateInfo结构进行的
            vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT; // 顶点着色器。告诉Vulkan将在哪个管道阶段使用着色器
            vertShaderStageInfo.module = vertShaderModule;          // 指定包含代码的着色器模块
            vertShaderStageInfo.pName = "main"; // 指定着色器模块要调用的函数。这意味着可以将多个顶点着色器组合到一个着色器模块中，并使用不同的入口点来区分它们的行为

            VkPipelineShaderStageCreateInfo fragShaderStageInfo = {}; // 将着色器模块分配给管道中的顶点或片段着色器阶段是通过VkPipelineShaderStageCreateInfo结构进行的
            fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT; // 片段着色器。告诉Vulkan将在哪个管道阶段使用着色器
            fragShaderStageInfo.module = fragShaderModule;            // 指定包含代码的着色器模块
            fragShaderStageInfo.pName = "main"; // 指定着色器模块要调用的函数。意味着可以将多个片段着色器组合到一个着色器模块中，并使用不同的入口点来区分它们的行为

            VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

            VkPipelineVertexInputStateCreateInfo vertexInputInfo = {}; // 描述传递给vertex shader的顶点数据格式
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInputInfo.vertexBindingDescriptionCount = 0; // 在顶点着色器中直接对顶点数据硬编码，所以要指定没有要加载的顶点数据
            vertexInputInfo.vertexAttributeDescriptionCount = 0;

            VkPipelineInputAssemblyStateCreateInfo inputAssembly = {}; // 描述了两件事：从顶点绘制什么样的几何体; 是否开启图元重启（primitive restart）
            inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO; // POINT_LIST：点; LINE_LIST：两点成线，顶点不重复使用; LINE_STRIP：每条线的结束顶点用作下一行的起始顶点;
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  // TRIANGLE_LIST：三个点成为一个三角形，顶点不重复使用; TRIANGLE_STRIP：每个三角形的第二个和第三个顶点用作下一个三角形的前两个顶点
            inputAssembly.primitiveRestartEnable = VK_FALSE;               // 不开启图元重启primitive restart

            VkViewport viewport = {}; // 视口
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float) swapChainExtent.width;
            viewport.height = (float) swapChainExtent.height;
            viewport.minDepth = 0.0f; // 指定用于帧缓冲区的深度值的范围，这里指定允许的最小值，0.0
            viewport.maxDepth = 1.0f; // 指定用于帧缓冲区的深度值的范围，这里指定允许的最大值，1.0

            VkRect2D scissor = {}; // 裁剪矩形
            scissor.offset = {0, 0};
            scissor.extent = swapChainExtent;

            VkPipelineViewportStateCreateInfo viewportState = {}; // 将视口和裁剪矩形组合到一起
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.pViewports = &viewport;
            viewportState.scissorCount = 1;
            viewportState.pScissors = &scissor;

            VkPipelineRasterizationStateCreateInfo rasterizer = {}; // 光栅化使用在顶点着色器由顶点组成的几何体，将其转换为片段。还可以执行深度测试，面部剔除和裁剪测试，并且为输出选择是填充整个多边形还是仅仅填充边缘（线框渲染）
            rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterizer.depthClampEnable = VK_FALSE; // 如果depthClampEnable设为VK_TRUE，那么超出近平面和远平面的片段将会收敛它们来使用而不是丢弃。这在阴影贴图等特殊情况下很有用
            rasterizer.rasterizerDiscardEnable = VK_FALSE; // 如果rasterizerDiscardEnable设为VK_TRUE，则几何体永远不会通过光栅化器阶段。这基本上禁用了帧缓冲的所有输出。
            rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // 声明如何绘制多边形。FILL填充、LINE绘制边缘、POINT多边形顶点绘制为点
            rasterizer.lineWidth = 1.0f;                   // 线段宽度。任何大于1.0f的线都要启用GPU的wideLines功能。
            rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;   // 面部剔除的类型：可以选择禁用剔除，剔除正面，剔除背面或都剔除
            rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE; // 面向前面的面的顶点顺序，可以是顺时针或逆时针。
            rasterizer.depthBiasEnable = VK_FALSE;          // 光栅化可以通过添加常量值或根据片段的斜率更改偏置来更改深度值。这个方法有时用于阴影贴图，但我们在这里不使用

            VkPipelineMultisampleStateCreateInfo multisampling = {}; // 多重采样，这是消除锯齿的方法之一。将光栅化后位于同一像素的点经过片段着色器进行处理后得到的结果来进行组合。这主要发生在边缘，也是最明显的锯齿出现的地方。
            multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisampling.sampleShadingEnable = VK_FALSE;               // 根据每个样本或每个片段为基准执行片段着色。值为 VK_TRUE，则是每个样本，否则为每个片段。
            multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // 该字段表示光栅化过程中每个像素使用的样本数。VK_SAMPLE_COUNT_1_BIT表示不在光栅化阶段做多重采样

            VkPipelineColorBlendAttachmentState colorBlendAttachment = {}; // 配置每个帧缓冲的颜色混合参数（src, dst, op）。颜色混合：片段着色器返回颜色后，需要将其与帧缓冲区中已有的颜色组合
            colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachment.blendEnable = VK_FALSE; // 指示是否希望启用混合。

            VkPipelineColorBlendStateCreateInfo colorBlending = {}; // 配置全局（新创建的）渲染管线的颜色混合参数，并且传入上面的BlendAttachment。颜色混合：片段着色器返回颜色后，需要将其与帧缓冲区中已有的颜色组合
            colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlending.logicOpEnable = VK_FALSE;   // 指示是否希望启用有关像素的逻辑操作。
            colorBlending.logicOp = VK_LOGIC_OP_COPY; // 预执行的逻辑操作类型（比如拷贝、清空等）
            colorBlending.attachmentCount = 1;
            colorBlending.pAttachments = &colorBlendAttachment;
            colorBlending.blendConstants[0] = 0.0f; // blendConstants包含用于混合的4个值（R、G、B、A）。
            colorBlending.blendConstants[1] = 0.0f;
            colorBlending.blendConstants[2] = 0.0f;
            colorBlending.blendConstants[3] = 0.0f;

            VkPipelineLayoutCreateInfo pipelineLayoutInfo = {}; // 即使用不到，仍然需要创建一个空的管道布局。管道布局并不关心着色器中定义的内容, 它只关心描述符和推送常量
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 0;   // Push constant是流水线的一部分，所以在设置VkPipelineLayout阶段（包含 descriptor set layout 和 push constant）就要让流水线知道有push constant的存在
            pipelineLayoutInfo.pushConstantRangeCount = 0; // 使用 VkPushConstantRange 声明设置 push constants 的用途和大小
            if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
                throw std::runtime_error("failed to create pipeline layout!");
            }

            VkGraphicsPipelineCreateInfo pipelineInfo = {};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.stageCount = 2;
            pipelineInfo.pStages = shaderStages;
            pipelineInfo.pVertexInputState = &vertexInputInfo;
            pipelineInfo.pInputAssemblyState = &inputAssembly;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &rasterizer;
            pipelineInfo.pMultisampleState = &multisampling;
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.layout = pipelineLayout;
            pipelineInfo.renderPass = renderPass;
            pipelineInfo.subpass = 0;
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

            if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
                throw std::runtime_error("failed to create graphics pipeline!");
            }
            
            vkDestroyShaderModule(device, fragShaderModule, nullptr);
            vkDestroyShaderModule(device, vertShaderModule, nullptr);

        }

        // 创建着色器模块：着色器代码要先封装在着色器模块中VkShaderModule， 才能传给图形管线
        VkShaderModule createShaderModule(const std::vector<char>& code) {
            VkShaderModuleCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;    // 创建着色器模块很简单，只需要一个指向字节码缓冲区的指针， 和字节码长度
            createInfo.codeSize = code.size();                                 // 字节码的大小是以字节为单位指定的
            createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data()); // 但字节码指针是uint32_t*类型而不是char指针。因此用reinterpret_cast进行转换

            VkShaderModule shaderModule;
            if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
                throw std::runtime_error("failed to create shader module!");
            }

            return shaderModule;
        }


        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
            for (const auto& availableFormat : availableFormats) { // VK_FORMAT_B8G8R8A8_UNORM表示以B，G，R和A的顺序，每个颜色通道用8位无符号整型数表示，总共每像素使用32位表示
                if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    return availableFormat; // SRGB颜色空间有更好的颜色表现，如果支持就用 VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
                }
            }

            return availableFormats[0];
        }


        // Presentation模式是交换链配置中最重要的一个，因为它代表了呈现image到屏幕的条件。Vulkan中有4个可能的模式：
        // VK_PRESENT_MODE_IMMEDIATE_KHR ：你的app提交的image会被立即传送到屏幕上，这可能导致撕裂。
        // VK_PRESENT_MODE_FIFO_KHR：交换链是一个队列，显示器刷新时，从队列头部拿一个image，程序将渲染好的image放到队列尾部。如果队列满了，程序就必须等待。这与现代游戏中的垂直同步最相似。显示器被刷新的时刻被称为“垂直回归”。
        // VK_PRESENT_MODE_FIFO_RELAXED_KHR：只有当垂直回归结束后，app晚了，队列空了，这一模式才与上一个模式有所区别。它不等待下一次垂直回归，而是当image到达时立即传送。这可能导致可见的撕裂。
        // VK_PRESENT_MODE_MAILBOX_KHR：这是第二个模式的另一个变种。队列满时候，它不阻塞app，队列中的image直接就被新的替换掉了。这个模式可以被用于实现三缓存，其允许你避免撕裂，且大幅减少延迟问题（与双缓存的垂直同步模式相比）。
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
            VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

            for (const auto& availablePresentMode : availablePresentModes) {
                if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) { // 三缓冲更加绝妙(nice)，可以先看看有没有它.
                    return availablePresentMode;
                }
            }

            return VK_PRESENT_MODE_FIFO_KHR; //现在的显示器，这个应总是有效的。
        }


        // extent 是Swap Chain中image的宽高分辨率(resolution) ,通常它与window的尺寸一样，可通过currentExtent设置width和height来匹配window的分辨率。
        // 但有些window Manager将currentExtent设置为uint32_t的最大值，来表示允许我们设置不同的值，这时我们可以从minImageExtent和maxImageExtent中选择最匹配window的值。
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
            if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
                return capabilities.currentExtent;
            } else {
                VkExtent2D actualExtent = {WIDTH, HEIGHT};

                actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
                actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

                return actualExtent;
            }
        }



        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
            SwapChainSupportDetails details;

            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities); // 基础surface功能（交换链包含的image的最大\小数量，image的宽度和高度的最大\最小值）

            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr); // Surface格式（像素格式，颜色空间）, VK_FORMAT_B8G8R8A8_UNORM像素用32位表示。SRGB颜色空间 VK_COLOR_SPACE_SRGB_NONLINEAR_KHR

            if (formatCount != 0) {
                details.formats.resize(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
            }

            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr); // 可用的presentation模式: VK_PRESENT_MODE_IMMEDIATE_KHR、FIFO、FIFO_RELAXED、MAILBOX

            if (presentModeCount != 0) {
                details.presentModes.resize(presentModeCount);
                vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
            }

            return details;
        }


        // 要在窗口显示图片，队列要支持绘制和呈现，也就是：VK_QUEUE_GRAPHICS_BIT 和 vkGetPhysicalDeviceSurfaceSupportKHR 检查surface是否支持图片显示。
        // 莫个物理设备的所有 *逻辑设备扩展* 确认支持交换链扩展VK_KHR_swapchain
        bool isDeviceSuitable(VkPhysicalDevice device) {
            QueueFamilyIndices indices = findQueueFamilies(device); // 在窗口显示图片，队列要支持绘制和呈现：VK_QUEUE_GRAPHICS_BIT 和vkGetPhysicalDeviceSurfaceSupportKHR 检查surface支持图片显示。


            bool extensionsSupported = checkDeviceExtensionSupport(device); // 确认莫个物理设备支持交换链扩展VK_KHR_swapchain， 要显示图片，这个逻辑实例扩展一定要


            bool swapChainAdequate = false;
            if (extensionsSupported) {
                SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device); //获取surface的 capabilities、formats、presentModes
                swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
            }

            return indices.isComplete() && extensionsSupported && swapChainAdequate;
        }

        // 获取莫个物理设备的所有 *逻辑设备扩展* 确认支持交换链扩展VK_KHR_swapchain， 要显示图片，这个逻辑实例扩展一定是要的。
        bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
            uint32_t extensionCount;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

            std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

            for (const auto& extension : availableExtensions) {
                requiredExtensions.erase(extension.extensionName);
            }

            return requiredExtensions.empty();
        }

        // vkGetPhysicalDeviceProperties： 属性包含的字段有：支持的Vulkan API版本、设备名称和类型（集成或专用/独立GPU）、厂商ID和限制。
        // vkGetPhysicalDeviceFeatures：获取物理设备的可选特性，如：是否支持geometryShaderh或者tessellationShader，纹理压缩、64位浮点数和多视口渲染
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
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport); // 该函数以物理设备，队列家族索引和surface为参数。 查看特定物理设备的特定队列家族是否支持交换链，或更准确地说， 是否支持在特定平面上显 示present 图像。 如果支持，presentSupport的值会被设为VK_TRUE.  因此我们才需要提前创建平面。 有的设备支持 绘制命令 drawing commands， 不代表就一定支持呈现，也就是将image呈现present到我们创建的surface窗口上。

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
            //  返回 *实例层* 用来创建surface的扩展列表，不但有基础的 VK_KHR_surface扩展， 还有针对特定操作系统扩展，
            //  在 Windows中称为 VK_KHR_win32_surface，在Linux中称为 VK_KHR_xlib_surface 或 VK_KHR_xcb_surface。
            //  参数glfwExtensionCount返回实例扩展的数量。函数返回值glfwExtensions是实例扩展的名称数组。
            glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

            // 用glfwExtensions的第0...glfwExtensionCount-1个元素来初始化。类似vector<int> tmp(vec.begin(), vec.begin() + 3) 用向量vec的第0个到第2个值初始化tmp
            std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

            // VK_LAYER_KHRONOS_validation默认在终端输出所有验证层的调试信息。启用实例扩展 VK_EXT_debug_utils后，我们可以自己定义一个回调函数，输出我们感兴趣的信息。
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

            for (const char* layerName : validationLayers) {   // 检查是否validationLayers 中所有的层都存在于availableLayers 列表中。 这里只有VK_LAYER_KHRONOS_validation
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

        // 辅助函数来从文件中加载二进制数据：字节码的着色器
        static std::vector<char> readFile(const std::string& filename) {
            std::ifstream file(filename, std::ios::ate | std::ios::binary); // ate从文件末尾开始阅读；binary将文件读取为二进制文件（避免文本转换）

            if (!file.is_open()) {
                throw std::runtime_error("failed to open file!");
            }

            size_t fileSize = (size_t) file.tellg(); // 从文件末尾开始读取的优点是我们可以利用读取位置来确定文件的大小并分配缓冲区
            std::vector<char> buffer(fileSize);

            file.seekg(0);      // 回到文件的开头重新读取所有字节
            file.read(buffer.data(), fileSize);

            file.close();

            return buffer;
        }


        // VK_LAYER_KHRONOS_validation默认在终端输出所有验证层的调试信息。启用实例扩展 VK_EXT_debug_utils后，我们可以自己定义一个回调函数，输出我们感兴趣的信息。
        // 定义一个用来接收消息的回调函数debugCallback，以PFN_vkDebugUtilsMessengerCallbackEXT 为原型。VKAPI_ATTR和VKAPI_CALL确保函数有正确的签名
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
