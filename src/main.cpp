#include <Windows.h>
#include <string>
#include <iostream>

// Forward Decl
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

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
        return 1;
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
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);

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

    // Wait for user to press a key before closing the application
    // and thus the console window.
    std::cout << "Press any key to exit..." << std::endl;
    std::cin.get();

    return 0;
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