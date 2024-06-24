#include <Windows.h>
#include <string>
#include <iostream>
#include <optional>

#include <array>
#include <vector>
#include <set>

#include "filehelper.h"

// In order to use the Win32 WSI extensions, we need to define VK_USE_PLATFORM_WIN32_KHR before including vulkan.h
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

// Forward Decl
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
bool checkValidationLayerSupport();
VkDebugUtilsMessengerEXT setupDebugMessenger();
void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& debugUtilsMessengerCreateInfo);
void pickPhysicalDevice();
bool isDeviceSuitable(VkPhysicalDevice device);
void createLogicalDevice();
bool checkDeviceExtensionSupport(VkPhysicalDevice device);
void createSwapChain();
void createImageViews();
void createGraphicsPipeline();
void createRenderPass();
VkShaderModule createShaderModule(const std::vector<char>& code);
void createFramebuffers();
void createCommandPool();
void createCommandBuffer();
void drawFrame();
void createSyncObjects();

struct QueueFamilyIndices {
    // Index to queue supporting graphics operations
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};
QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

void RedirectIOToConsole()
{
    // Allocate a console window
    AllocConsole();

    // Redirect the standard input, output, and error handles to the console window
    freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);

    // Clear the error state for each of the C++ standard stream objects. We need to do this, as C++ standard streams are tied to the standard C streams and this is the reason why we need to flush the C++ streams as well.
    std::wcout.clear();
    std::cout.clear();
    std::wcerr.clear();
    std::cerr.clear();
    std::wcin.clear();
    std::cin.clear();        
}

// Vulkan extension functions not part of the core Vulkan library that is statically linked, have to be loaded dynamically at runtime
// You do this using the "vkGetInstanceProcAddr" function.
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

VkInstance vkInstance;
VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
VkDevice logicalDevice = VK_NULL_HANDLE;
VkSurfaceKHR surface;
VkSwapchainKHR swapChain;
std::vector<VkImage> swapChainImages;
VkFormat swapChainImageFormat;
VkExtent2D swapChainExtent;
std::vector<VkImageView> swapChainImageViews;
VkPipelineLayout pipelineLayout;
VkRenderPass renderPass;
VkPipeline graphicsPipeline;
std::vector<VkFramebuffer> swapChainFramebuffers;
VkCommandPool commandPool;
VkCommandBuffer commandBuffer;
VkSemaphore imageAvailableSemaphore;
VkSemaphore renderFinishedSemaphore;
VkFence inFlightFence;

// Device extensions extend the capabilities of a specific Vulkan device (like a GPU)
// These extensions affect the device and its operations, like providing additional features for rendering
// compute, or memory management.
std::array<const char*, 1> deviceExtensions = {
    // In order to present rendering results to a surface, we need a swapchain.
    // This is also not part of the Vulkan core, and so can be found in the "VK_KHR_swapchain" extension.
    // This extension introduces the VkSwapchainKHR objects, which provides the ability to present rendering results to a surface.
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// TODO: It is possible to have a single queue that simply supports both graphics and presentation. For now it's split up, but maybe combine them later.
VkQueue graphicsQueue;
VkQueue presentQueue;

// Windows Desktop Applications have a WinMain function as the entrypoint.
int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
{
    RedirectIOToConsole();

    WNDCLASSEX wcex = {};

    std::string window_class_name = "Main Game Window";

    // Size in bytes of structure. Must always be set to sizeof(WNDCLASSEX)
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WindowProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.lpszClassName = window_class_name.c_str();

    if (!RegisterClassEx(&wcex))
    {
        std::cout << "Failed to register window class." << std::endl;
        std::terminate();
    }

    // Create window using the window class
    std::string window_title = "2D Beagle";

    // If CreateWindowEx fails, it returns NULL.
    HWND hwnd = CreateWindowEx(
        0,
        window_class_name.c_str(),
        window_title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!hwnd)
    {
        std::cout << "Failed to create window." << std::endl;
        std::terminate();
    }

    ShowWindow(hwnd, nCmdShow);

    // Create Vulkan Instance
    // The Vulkan Instance is the connection between your application and the Vulkan library.
    vkInstance = VK_NULL_HANDLE;

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "2D Beagle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    // Instance extensions are extensions that affect the Vulkan instance itself, rather than a specific device.
    // They extend the capabilities of the Vulkan instance itself, and affect the entire application.
    std::array<const char*, 3> instanceExtensions = {
        // Required extensions for the surface objects specific to Win32
        // We use these surface objects to render images to a window.
        "VK_KHR_surface",
        "VK_KHR_win32_surface",
        // Required extension for the debug messenger
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };

    // Check if the required validation layers are available
    checkValidationLayerSupport();

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
    instanceCreateInfo.enabledLayerCount = 0;

    // We enable debug messenger for the instance creation as well, by supplying create info to "pNext".
    VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo;
    populateDebugMessengerCreateInfo(debugUtilsMessengerCreateInfo);
    instanceCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugUtilsMessengerCreateInfo;

    if (vkCreateInstance(&instanceCreateInfo, nullptr, &vkInstance) != VK_SUCCESS)
    {
        std::cout << "Failed to create Vulkan instance!" << std::endl;
        std::terminate();
    }

    VkDebugUtilsMessengerEXT debugMessenger = setupDebugMessenger();

    // Window surface creation needs to happen right after instance creation, because it can influence the physical device selection.
    VkWin32SurfaceCreateInfoKHR VkWin32SurfaceCreateInfo {};
    VkWin32SurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    VkWin32SurfaceCreateInfo.hwnd = hwnd;
    VkWin32SurfaceCreateInfo.hinstance = hInstance;

    // vkCreateWin32SurfaceKHR is technically an extension function, but because it is so commonly used
    // the standard Vulkan loader includes it.
    if (vkCreateWin32SurfaceKHR(vkInstance, &VkWin32SurfaceCreateInfo, nullptr, &surface) != VK_SUCCESS) {
        std::cout << "Failed to create Win32 surface!" << std::endl;
        std::terminate();
    }

    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createGraphicsPipeline();
    createRenderPass();
    createFramebuffers();
    createCommandPool();
    createCommandBuffer();
    createSyncObjects();

    MSG msg = {};
    auto running = true;
    while (running) {
        // Process all pending Windows messages
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                running = false;   
            }

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Update and render game here
        drawFrame();
    }

    // Vulkan Cleanup

    // Destroy semaphores and fences
    vkDestroySemaphore(logicalDevice, renderFinishedSemaphore, nullptr);
    vkDestroySemaphore(logicalDevice, imageAvailableSemaphore, nullptr);
    vkDestroyFence(logicalDevice, inFlightFence, nullptr);

    // Destroy pipeline
    vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);

    // Destroy pipeline layouts
    vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);

    // Framebuffers should be deleted before the image views and render pass
    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
    }

    // Destroy render pass
    vkDestroyRenderPass(logicalDevice, renderPass, nullptr);

    // The swapchain should be destroyed before the logical device is destroyed.
    vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);

    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(logicalDevice, imageView, nullptr);
    }

    // Destroy command pool for graphics queue
    vkDestroyCommandPool(logicalDevice, commandPool, nullptr);

    vkDestroyDevice(logicalDevice, nullptr);

    vkDestroySurfaceKHR(vkInstance, surface, nullptr);

    // TODO: Investigate this further, I can't find any official explanation for this being true.
    // It is important to destroy the debug messenger AFTER the logical device has been destroyed.
    // This is because the logical device might be using the debug messenger, and destroying the debug messenger before the logical device
    // can cause memory access violations.
    DestroyDebugUtilsMessengerEXT(vkInstance, debugMessenger, nullptr);    
    vkDestroyInstance(vkInstance, nullptr);

    // Wait for user to press a key before closing the application
    // and thus the console window.
    std::cout << "Press any key to exit..." << std::endl;
    std::cin.get();

    return 0;
}

std::string MessageSeverityToString(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity) {
    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            return "VERBOSE";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            return "INFO";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            return "WARNING";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallBack(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    std::string messageSeverityString = MessageSeverityToString(messageSeverity);

    std::cerr << "Debug Message: " << "{" << messageSeverityString << "}" << ": " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& debugUtilsMessengerCreateInfo) {
    debugUtilsMessengerCreateInfo = {};
    debugUtilsMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

    // Specify what message severities should be passed to the callback.
    debugUtilsMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    // Specify what types of messages should be passed to the callback.
    debugUtilsMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    // Specify the callback function pointer.
    debugUtilsMessengerCreateInfo.pfnUserCallback = debugCallBack;
}

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice) {
    QueueFamilyIndices indices;

    // We queue the number of available queue families on the physical device, so that we can retrieve indices
    // To specific queues that we need in order to submit work for things like rendering.
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        // We need to figure out whether the physical device supports presenting to the surface we created.
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

void pickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(vkInstance, &deviceCount, nullptr);

    if (deviceCount == 0)
    {
        std::cout << "Failed to find GPUs with Vulkan support!" << std::endl;
        std::terminate();
    }

    // vkEnumeratePhysicalDevices will return a list of physical devices that can be used by Vulkan.
    // This is not necessarily a GPU, and also there a multiple types of GPUs that you might be interested in differentiating between.
    // A physical device can be an integrated GPU, a discrete GPU (dedicated), a virtual GPU, or a CPU, or something else.
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(vkInstance, &deviceCount, devices.data());

    for (const auto& device : devices)
    {
        if (isDeviceSuitable(device))
        {
            physicalDevice = device;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE)
    {
        std::cout << "Failed to find a suitable GPU!" << std::endl;
        std::terminate();
    }
}

bool isDeviceSuitable(VkPhysicalDevice device)
{
    // In order to evaluate whether a physical device is suitable for what we want, we can query
    // the properties of the device using vkGetPhysicalDeviceProperties.
    VkPhysicalDeviceProperties physicalDeviceProperties {};
    vkGetPhysicalDeviceProperties(device, &physicalDeviceProperties);

    QueueFamilyIndices indices = findQueueFamilies(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    return physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU 
        && indices.isComplete() && extensionsSupported && swapChainAdequate;
}
    
bool checkDeviceExtensionSupport(VkPhysicalDevice device)
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

void createLogicalDevice()
{
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos {};
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies)
    {
        // When creating a logical device, can can also supply information about the queue families we want to create queues for,
        // and how many.
        VkDeviceQueueCreateInfo queueCreateInfo {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
        queueCreateInfo.queueCount = 1;
        // We also have to specify the priority of the queue, which influences the scheduling of command buffer execution.
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // We also need to specify the device features we are interested in.
    VkPhysicalDeviceFeatures deviceFeatures {};

    // Now we can create the logical device
    VkDeviceCreateInfo deviceCreateInfo {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    // Enable device extensions.
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    // Previous implementations of Vulkan made a distinction between instance and device specific validation layers.
    // This is no longer the case however. And in up-to-date implementations, enabledLayerCount and ppEnabledLayerNames properties of a logical device are ignored.
    // It is good, however, to still set them in order to be compatible with older implementations.
    deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();

    if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice) != VK_SUCCESS)
    {
        std::cout << "Failed to create logical device!" << std::endl;
        std::terminate();
    }

    // Queues are automatically created when the logical device is created.
    vkGetDeviceQueue(logicalDevice, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(logicalDevice, indices.presentFamily.value(), 0, &presentQueue);
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    // In order to set up a swapchain, it's crucial to choose a suitable surface format.
    // Surface formats define how the pixels of images presented to the screen are stored and processed.
    // A VkSurfaceFormatKHR consists of a format and a colorSpace member.
    // The vkFormat struct specifies the actual format of the image data. This includes things like the number of
    // bits used for each color channel and the arrangement of the channels (like RGBA or BGRA)
    // The VkColorSpaceKHR member defines how the color values should be interpreted in terms of color space.
    // A color space indicates the range and characteristics of colors that can be represented.
    // TODO: Read more up on color spaces and how they work.
    for (const auto& availableFormat : availableFormats)
    {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

// The presentation mode represents the actual conditions for showing images to the screen.
// VK_PRESENT_MODE_IMMEDIATE_KHR: 
// - Images submitted by your application are transferred to the screen right away, which may result in tearing.
// VK_PRESENT_MODE_FIFO_KHR: 
// - The swapchain is a queue where the display takes an image from the front of the queue. This "taking" is synchronized with the display's vertical blanking interval (VBI).
// - This ensures that tearing does not occur.
// - If the queue is full, the application must wait before submitting a new image.
// - If the queue is empty during a VBI, the display will keep showing the latest image available until a new image is available in the queue.
// VK_PRESENT_MODE_FIFO_RELAXED_KHR:
// - Like VK_PRESENT_MODE_FIFO, but if the application is late and the queue was empty at the last vertical blank, instead of waiting for the next vertical blank, the image is
// - transferred right away when it finally arrives. This may result in visible tearing.
// VK_PRESENT_MODE_MAILBOX_KHR:
// - Instead of blocking the application when the queue is full, the images that are already queued are simply replaced with the newer ones.
// - This mode can be used to render frames as fast as possible while still avoiding tearing, result in fewer latency issues than standard vertical sync.
// - Commonly known as "triple buffering", although the existence of three buffers alone does not necessarily mean that the framerate is unlocked.
VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    // VK_PRESENT_MODE_MAILBOX_KHR is a nice trade-off if energy usage is not a concern.
    // It allows us to avoid tearing while still maintaining a fairly low latency by rendering new images that are as up-to-date as possible right until the vertical blank.
    for (const auto& availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            std::cout << "Chose VK_PRESENT_MODE_MAILBOX_KHR" << std::endl;
            return availablePresentMode;
        }
    }

    // VK_PRESENT_MODE_FIFO_KHR mode is guarenteed to be available.
    return VK_PRESENT_MODE_FIFO_KHR;
}

// VkSurfaceCapabilitiesKHR specifies the capabilities of the surface.
// When "currentExtent" is not '0xFFFFFFFF', the surface size will match the window size in pixels. The swapchain created for the surface must use images of this extent.
// When "currentExtent" is '0xFFFFFFFF', the application can choose the extent within the bounds specified by "minImageExtent" and "maxImageExtent".
// A VkExtend2D is essentially just a struct defining a width and height. A two dimensional extent. It's the context of it that matters.
VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        return capabilities.currentExtent;
    }

    std::cout << "Failed to choose swap extent!" << std::endl;
    std::terminate();
}

void createSwapChain()
{
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    swapChainImageFormat = surfaceFormat.format;

    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);

    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
    swapChainExtent = extent;

    // We have to decide how many imagines we would like to have in the swap chain.
    // It is recommended to request at least on more image than the minimum.
    // TODO: Read up on why this is the case later.
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    // We make sure not to exceed the maximum number of images that the swap chain supports.
    // A "maxImageCount" of 0 means that there is no maximum.
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
    {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapChainCreateInfo {};
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;

    swapChainCreateInfo.minImageCount = imageCount;
    swapChainCreateInfo.surface = surface;
    swapChainCreateInfo.imageFormat = surfaceFormat.format;
    swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent = extent;

    // Specifies the amount of layers each image consist of. This is always 1 unless you are developing a stereoscopic 3D application.
    swapChainCreateInfo.imageArrayLayers = 1;
    
    // ImageUsage bit specifies what kind of operations we'll use the images in the swap chain for.
    // For now we render directly to them, which means that they're used as color attachment.
    swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    std::array<uint32_t, 2> queueFamilyIndices { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily)
    {
        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapChainCreateInfo.queueFamilyIndexCount = 2;
        swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices.data();
    } else {
        swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapChainCreateInfo.queueFamilyIndexCount = 0;
        swapChainCreateInfo.pQueueFamilyIndices = nullptr;
    }

    // You can specify that a certain transform should be applied to the images in the swap chain if it is supported.
    // Like a 90 degree clockwise rotation or horizontal flip.
    // To specify that you do not want any transformation, simply specify the current transformation.
    swapChainCreateInfo.preTransform = swapChainSupport.capabilities.currentTransform;

    // The compositeAlpha field specifies if the alpha channel should be used for blending with other windows in the window system.
    // You'll almost always want to simply ignore the alpha channel.
    swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    // If the "clipped" member is set to true, then that means that we don't care about the color of pixels that are obscured.
    // For example, because another window is in front of them.
    // Unless you really need to be able to read these pixels back and get predictable results, you'll get the best performance by enabling clipping.
    swapChainCreateInfo.presentMode = presentMode;
    swapChainCreateInfo.clipped = VK_TRUE;

    // With Vulkan it is possible that your swap chain becomes invalid or unoptimized while your application is running.
    // This can be, for example, because the window is resized.
    // In that case, the swap chain actually needs to be recreated from scratch and a reference to the old one must be specified in this field.
    // TODO: We ignore oldSwapChain for now.
    swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(logicalDevice, &swapChainCreateInfo, nullptr, &swapChain) != VK_SUCCESS)
    {
        std::cout << "Failed to create swap chain!" << std::endl;
        std::terminate();
    }

    // The actual images are created by the implementation for the swap chain, and will be automatically cleaned up once the swap chain
    // has been destroyed.
    vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, nullptr);

    // The implementation is allowed to create more images than the minimum we specified,
    // So we have to query how many images were actually created.
    swapChainImages.resize(imageCount);

    vkGetSwapchainImagesKHR(logicalDevice, swapChain, &imageCount, swapChainImages.data());
}

void createImageViews()
{
    // Resize image views to the same amount as swap chain images.
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++)
    {
        VkImageViewCreateInfo imageViewCreateInfo {};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.image = swapChainImages[i];
        
        // #TODO: Read up on image view types. Specifically, what does it really mean for something to be a 2D or 3D texture?
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = swapChainImageFormat;

        // #TODO: Read up on this swizzle magic
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        // Our image views will deal with the color aspect of the images,
        // hence, VK_IMAGE_ASPECT_COLOR_BIT
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        // We don't use multiple mipmap levels in this view, hence a level count of 1 and a base mip level of 0 (the first level)
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(logicalDevice, &imageViewCreateInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS)
        {
            std::cout << "Failed to create image views!" << std::endl;
            std::terminate();
        }
    }
}

SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device)
{
    SwapChainSupportDetails details {};

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    // VkPresentModeKHR describes the modes available for presenting images to the surface.
    // The mode determines the behaviour of the swapchain in regard to how images are presented
    // and synchronized with the display.
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    VkSurfaceCapabilitiesKHR surfaceCapabilities {};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &surfaceCapabilities);

    return details;
}

VkDebugUtilsMessengerEXT setupDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT createInfo {};
    populateDebugMessengerCreateInfo(createInfo);

    // Create the debug messenger
    VkDebugUtilsMessengerEXT debugMessenger;
    if (CreateDebugUtilsMessengerEXT(vkInstance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        std::cout << "Failed to set up debug messenger." << std::endl;
        std::terminate();
    }

    return debugMessenger;
}

void createGraphicsPipeline() {
    auto vertShaderCode = readFile("shaders/vert.spv");
    auto fragShaderCode = readFile("shaders/frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    // Create a vertex shader stage
    // Using the vertex shader module we created.
    // pName specifies the function to invoke in our shader (entrypoint), in this case "main".
    VkPipelineShaderStageCreateInfo vertShaderStageInfo {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    // Create a fragment shader stage
    // Using the fragment shader module we created.
    VkPipelineShaderStageCreateInfo fragShaderStageInfo {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    // Describe vertex input
    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo {};
    vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    // Describe input assembly
    // VkPipelineInputAssemblyStateCreateInfo describes two things:
    // What kind of geometry will be drawn from the vertices, and if primitive restart should be enabled.
    // We intend to draw triangles
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo {};
    inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST means a triangle from every 3 vertices without reuse.
    inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

    // Describe the viewport.
    // The viewport describes the region of the framebuffer that the output will be rendered to.
    // This will almost always be (0, 0) to (width, height).
    VkViewport viewport {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) swapChainExtent.width;
    viewport.height = (float) swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // The scissor rectangle defines in which regions pixels will actually be stored.
    // Anything outside of this rectanble will be discarded by the rasterizer.
    // For now, I just create it to be the same size as the viewport.
    VkRect2D scissor {};
    scissor.offset = { 0, 0 };
    scissor.extent = swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo {};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = &viewport;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = &scissor;

    // Describe the Rasterizer stage
    // The rasterizer takes the geometry that is shaped by the vertices from the vertex shader and turns it into fragments.
    // The fragments will then be colored, depth tested, and face culled in the fragment shader.
    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo {};
    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;

    // polygonMode determines how fragments are generated for geometry.
    // VK_POLYGON_MODE_FILL = Fill the area of the polygon with fragments.
    rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationStateCreateInfo.lineWidth = 1.0f;

    // Culling refers to the process of discarding triangles during rendering, based on their orientation to the camera.
    // VK_CULL_MODE_BACK_BIT = Triangles that are back-facing will be discarded.
    rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    // frontFace determines that order of vertices that determines the front.
    rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;

    // TODO: Read up on what behaviour all these settings alter
    rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
    rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
    rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
    rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;

    // VkPipelineMultisampleStateCreateInfo configures multisamlping, a type of anti-aliasing.
    // We disable it for now, as enabling it requires enabling a GPU feature.
    VkPipelineMultisampleStateCreateInfo multisamplingStateCreateInfo {};
    multisamplingStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisamplingStateCreateInfo.sampleShadingEnable = VK_FALSE;
    multisamplingStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisamplingStateCreateInfo.minSampleShading = 1.0f;
    multisamplingStateCreateInfo.pSampleMask = nullptr;
    multisamplingStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
    multisamplingStateCreateInfo.alphaToOneEnable = VK_FALSE;

    // Configure color blending.
    // Color blending is the process of combining the color of a fragment that is being written with the color that is already in the framebuffer.
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    // VkPiplineColorBlendStateCreateInfo contains the configuration for the entire pipeline's color blending state.
    // This configuration with the above color blend attachment represents a configuration in where the color from the fragment shader
    // will be written to the framebuffer in an unmodified way, regardless of the existing color on the framebuffer.
    VkPipelineColorBlendStateCreateInfo colorBlendingStateCreateInfo {};
    colorBlendingStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendingStateCreateInfo.logicOpEnable = VK_FALSE;
    colorBlendingStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
    colorBlendingStateCreateInfo.attachmentCount = 1;
    colorBlendingStateCreateInfo.pAttachments = &colorBlendAttachment;
    colorBlendingStateCreateInfo.blendConstants[0] = 0.0f;

    // Uniform values in Shaders needs to be specified during pipeline creation through VkPipelineLayout objects.
    // Currently I don't use uniform values, but it is required to create an empty VkPipelineLayout object at minimum.
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 0;
    pipelineLayoutCreateInfo.pSetLayouts = nullptr;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        std::cout << "Failed to create pipeline layout." << std::endl;
        std::terminate();
    }

    VkGraphicsPipelineCreateInfo pipelineCreateInfo {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStages;

    pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
    pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisamplingStateCreateInfo;
    pipelineCreateInfo.pDepthStencilState = nullptr;
    pipelineCreateInfo.pColorBlendState = &colorBlendingStateCreateInfo;
    pipelineCreateInfo.pDynamicState = nullptr;

    pipelineCreateInfo.layout = pipelineLayout;

    pipelineCreateInfo.renderPass = renderPass;
    pipelineCreateInfo.subpass = 0;

    // Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline.
    // It is less expensive to set up pipelines when they have much functionality in common with an existing pipeline
    // And switching between pipelines from the same parent can also be done quicker.
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex = -1;

    graphicsPipeline = VK_NULL_HANDLE;
    if (vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        std::cout << "Failed to create graphics pipeline." << std::endl;
        std::terminate();
    }

    vkDestroyShaderModule(logicalDevice, vertShaderModule, nullptr);
    vkDestroyShaderModule(logicalDevice, fragShaderModule, nullptr);
}

void createRenderPass() {
    // We will have a single color buffer attachment 
    VkAttachmentDescription colorAttachment {};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

    // Determine what to do with the color attachment BEFORE rendering
    // VK_ATTACHMENT_LOAD_OP_CLEAR = Clear the values to a constant at the start.
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

    // Determine what to do with the color attachment AFTER rendering
    // VK_ATTACHMENT_STORE_OP_STORE = Rendered contents will be stored in memory and can be read later.
    // We do this because we are interested in seeing the rendered triable on the screen.
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    // 'loadOp' and 'storeOp' relates to color and depth data,
    // 'stencilLoadOp' and 'stencilStoreOP' relates to stencil data.
    // We don't do anything with stencil data, so they are irrelevant.
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    // Image layouts specify how the data in an image is organized in memory.
    // we don't care about the initial layout of the image data, because we're going to clear it anyway.
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // Final Layout specifies the layout the attachment image subresource will be transitioned to when a render pass instance ends.
    // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR specifies that the image can be presented to the screen via a swapchain.
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // A single render pass can consist of multiple subpasses.
    // Subpasses are subsequent rendering operations that depend on the contents of framebuffers in previous passes, applied one after the other.
    // For now, we'll stick with a single subpass.
    // Vulkan requires at least one subpass.

    // Every subpass references one or more of the attachments that we've described.
    // We only have a single attachment, and its in index 0 of the attachment array.
    VkAttachmentReference colorAttachmentRef {};
    colorAttachmentRef.attachment = 0;
    // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL specifies which layout we would like the attachment to have during a subpass that use this reference.
    // We specify VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL because this shows intent to use it as a color buffer.
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassCreateInfo {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = 1;
    renderPassCreateInfo.pAttachments = &colorAttachment;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpassDescription;

    if (vkCreateRenderPass(logicalDevice, &renderPassCreateInfo, nullptr, &renderPass) != VK_SUCCESS) {
        std::cout << "Failed to create render pass." << std::endl;
        std::terminate();
    }
}

void createFramebuffers() {
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        VkImageView attachments[] = {
            swapChainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferCreateInfo {};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.renderPass = renderPass;
        framebufferCreateInfo.attachmentCount = 1;
        framebufferCreateInfo.pAttachments = attachments;
        framebufferCreateInfo.width = swapChainExtent.width;
        framebufferCreateInfo.height = swapChainExtent.height;
        framebufferCreateInfo.layers = 1;

        if (vkCreateFramebuffer(logicalDevice, &framebufferCreateInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            std::cout << "Failed to create framebuffer." << std::endl;
            std::terminate();
        }
    }
}

// Command pools are memory managers for command buffers.
// They allocate memory for command buffers and also allow for recycling of command buffers.
void createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    // We create a command pool for the graphics family queue.
    // Command pools can only allocate command buffers for a single, specific type of queue.
    VkCommandPoolCreateInfo poolInfo {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT = Allow command buffers to be rerecorded individually.
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        std::cout << "Failed to create command pool." << std::endl;
        std::terminate();
    }
}

// Command buffers are containers for recording commands that will be submitted to a device queue for execution.
// These commands include drawing, compute operations, and resource state transitions.
// There are two types of command buffers: primary and secondary.
// Primary command buffers can be submitted to a queue, while secondary command buffers are executed by primary command buffers.
void createCommandBuffer() {
    VkCommandBufferAllocateInfo allocInfo {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        std::cout << "Failed to allocate command buffer." << std::endl;
        std::terminate();
    }
}

void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo commandBufferBeginInfo {};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    // flags specifies how we are going to use the command buffer.
    commandBufferBeginInfo.flags = 0;
    commandBufferBeginInfo.pInheritanceInfo = nullptr;

    // If a command buffer was already recorded once, then a call to vkBeginCommandBuffer will implicitly reset it.
    // It's not possible to append commands to a buffer at a later time.
    if (vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS) {
        std::cout << "Failed to begin recording command buffer." << std::endl;
        std::terminate();
    }

    // Drawing starts by beginning a render pass with vkCmdBeginRenderPass.
    VkRenderPassBeginInfo renderPassBeginInfo {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = renderPass;
    renderPassBeginInfo.framebuffer = swapChainFramebuffers[imageIndex];
    // RenderArea defines the area of the framebuffer that will be rendered to.
    // RenderArea must be contained within the framebuffer dimensions.
    // If the RenderArea is smaller than the framebuffer, it may lead to performance cost.
    renderPassBeginInfo.renderArea.offset = { 0, 0 };
    renderPassBeginInfo.renderArea.extent = swapChainExtent;

    // Define the clear values to use for VK_ATTACHMENT_LOAD_OP_CLEAR, which we used
    // for the load operation of the color attachment.
    // This is black with 100% opacity.
    VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = &clearColor;

    // Record the command to begin a render pass.
    // VK_SUBPASS_CONTENTS_INLINE = The render pass command will be embedded in the primary command buffer itself.
    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind the graphics pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    // Issue draw command
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    // End the render pass
    vkCmdEndRenderPass(commandBuffer);

    // Finish recording the command buffer
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        std::cout << "Failed to record command buffer." << std::endl;
        std::terminate();
    }
}

// At a high level, rendering a frame in Vulkan consists of the following steps:
// 1. Wait for the previous frame to finish
// 2. Acquire an image from the swap chain
// 3. Record a command buffer which draws the scene onto that image
// 4. Submit the recorded command buffer
// 5. Present the swap chain image
void drawFrame() {
    // Wait for our fence, which in this case signals that the previous frame has finished.
    // The last parameter of vkWaitForFences is a timeout in milliseconds, and we specify the maximum value. Effectively disabling timeout.
    vkWaitForFences(logicalDevice, 1, &inFlightFence, VK_TRUE, UINT64_MAX);

    // After waiting, we need to manually reset the fence to unsignaled state.
    vkResetFences(logicalDevice, 1, &inFlightFence);

    // We aquire an image from the swap chain.
    // First two parameters: the logical device and swap chain from which we wish to aquire an image.
    // The third parameter specifies a timeout in nanoseconds for an image to become available. Using a max value effectively disables it.
    // The next two parameters specify synchronization objects that are to be signaled when the presentation engine is finished using the image.
    // That's the point in time where we can start drawing to it. We use our imageAvailableSemaphore semaphore here.
    // The last parameter is an output to the index of the swap chain where an image has become available.
    // The index refers to the VkImage in the swap chain images array. We use that index to pick a VkFrameBuffer.
    uint32_t imageIndex;
    vkAcquireNextImageKHR(logicalDevice, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    // Before we start rendering, we reset the command buffer, so that it can be recorded again.
    vkResetCommandBuffer(commandBuffer, 0);
    // Record the command buffer with a new drawing operation
    recordCommandBuffer(commandBuffer, imageIndex);

    // Submit the command buffer to the graphics queue.
    VkSubmitInfo submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // We specify the semaphores to singal once the comamnd buffers have finished execution.
    // Here, we want to signal the renderFinishedSemaphore, to indicate that rendering has finished.
    VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    // Submit the command buffer to the graphics queue.
    // The inFlightFence parameter is the fence that will be signaled when the command buffer has finished execution.
    // This will indicate to us when it is safe to reuse the command buffer for another frame.
    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
        std::cout << "Failed to submit draw command buffer." << std::endl;
        std::terminate();
    }
}

void createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // We create the fence being initially in a signaled state.
    // We do this so that the first time we wait for the fence in the draw function, it won't block indefinitely.
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
        vkCreateFence(logicalDevice, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
        std::cout << "Failed to create synchronization objects." << std::endl;
        std::terminate();
    }
}

VkShaderModule createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo shaderModuleCreateInfo {};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = code.size();
    // TODO: Read up on reinterpret_cast and the alignment of data. And how std::vector apparently guarantees alignment in worst case alignment requirements.
    // Notice: pCode is a pointer to an array of 32-bit words, but the data is a char array.
    shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    // vkShaderModules are simply thin wrappers around the SPIR-V bytecode, and a VkShaderModule is nothing but a handle to that bytecode.
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        std::cout << "Failed to create shader module." << std::endl;
        std::terminate();
    }

    return shaderModule;
}

bool checkValidationLayerSupport() {
    // Get the number of available layers.
    // vkEnumerateInstanceLayerProperties is a function that returns the number of available layers and their properties.
    // If it is called with NULL for the "pProperties" parameter, it will return the number of available layers in the first parameter.
    uint32_t layerCount;
    VkResult instanceLayerPropertiesResult = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    if (instanceLayerPropertiesResult != VK_SUCCESS) {
        std::cout << "Failed to enumerate instance layer properties." << std::endl;
        std::terminate();
    }

    // If it is called with a valid pointer for the "pProperties" parameter, it will return the properties of the available layers.
    std::vector<VkLayerProperties> availableLayers(layerCount);
    instanceLayerPropertiesResult = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    if (instanceLayerPropertiesResult != VK_SUCCESS) {
        std::cout << "Failed to enumerate instance layer properties." << std::endl;
        std::terminate();
    }

    // Check if the validation layers we want to use are available in the queried layers.
    // TODO: Read up on C++ and it's abilities to use LINQ-like queries.
    for (const char* layerName : validationLayers) {
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

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}