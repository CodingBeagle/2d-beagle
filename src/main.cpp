#include <Windows.h>
#include <string>
#include <iostream>
#include <optional>

#include <array>
#include <vector>

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

struct QueueFamilyIndices {
    // Index to queue supporting graphics operations
    std::optional<uint32_t> graphicsFamily;
};
QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

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

    std::array<const char*, 3> instanceExtensions = {
        // Required extensions for the surface objects specific to Win32
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

    pickPhysicalDevice();

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

    // We queue the number of available queue families on the physical device, so that we can retriece indices
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

    return physicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU 
        && indices.graphicsFamily.has_value();
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