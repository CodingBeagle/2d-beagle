#include <Windows.h>
#include <string>
#include <iostream>
#include <optional>

#include <array>
#include <vector>
#include <set>

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
    }

    // Vulkan Cleanup
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