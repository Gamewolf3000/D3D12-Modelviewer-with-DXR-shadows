#include <ShellScalingAPI.h>
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <iostream>
#include <string>

#include "imgui_impl_win32.h"
#include "D3DPtr.h"

#include "ModelViewerScene.h"

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 600; }

extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = u8".\\D3D12\\"; }

void WriteDebugStringW(const wchar_t* toWrite)
{
    OutputDebugStringW(toWrite);
    OutputDebugStringW(L"\n");
}

void ThrowIfFailed(HRESULT hr, const std::exception& exception)
{
    if (FAILED(hr))
        throw exception;
}

void EnableDebugLayer()
{
    ID3D12Debug* debugController;
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)),
        std::runtime_error("Could not get debug interface"));
    debugController->EnableDebugLayer();
    debugController->Release();
}

void EnableGPUBasedValidation()
{
    ID3D12Debug1* debugController;
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)),
        std::runtime_error("Could not get debug interface"));
    debugController->SetEnableGPUBasedValidation(true);
    debugController->Release();
}

IDXGIFactory* CreateFactory(UINT flags = 0)
{
    IDXGIFactory* toReturn;

    HRESULT hr = CreateDXGIFactory2(flags, IID_PPV_ARGS(&toReturn));
    auto errorInfo = std::runtime_error("Could not create dxgi factory");
    ThrowIfFailed(hr, errorInfo);

    return toReturn;
}

IDXGIAdapter1* GetAdapter(IDXGIFactory1* factory, unsigned int adapterIndex)
{
    IDXGIAdapter1* toReturn;
    return factory->EnumAdapters1(adapterIndex, &toReturn) == S_OK ? toReturn : nullptr;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;

    // sort through and find what code to run for the message given
    switch (message)
    {
        // this message is read when the window is closed
    case WM_DESTROY:
    {
        // close the application entirely
        PostQuitMessage(0);
        return 0;
    } break;
    }

    // Handle any messages the switch statement didn't
    return DefWindowProc(hWnd, message, wParam, lParam);
}

HWND InitializeWindow(HINSTANCE hInstance, unsigned int windowWidth,
    unsigned int windowHeight, bool windowed = true)
{
    const wchar_t CLASS_NAME[] = L"Test Window Class";

    WNDCLASS wc = { };

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    DWORD windowStyle = windowed ? WS_OVERLAPPEDWINDOW : WS_EX_TOPMOST | WS_POPUP;
    RECT rt = { 0, 0, static_cast<LONG>(windowWidth),
        static_cast<LONG>(windowHeight) };
    AdjustWindowRect(&rt, windowStyle, FALSE);

    HWND hWnd = CreateWindowEx(0, CLASS_NAME, L"Model Viewer D3D12", windowStyle,
        0, 0, rt.right - rt.left, rt.bottom - rt.top, nullptr, nullptr,
        hInstance, nullptr);

    if (hWnd == nullptr)
    {
        DWORD errorCode = GetLastError();
        throw std::runtime_error("Could not create window, last error: " +
            std::to_string(errorCode));
    }

    ShowWindow(hWnd, windowed ? SW_SHOWNORMAL : SW_SHOWMAXIMIZED);
    return hWnd;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    const unsigned int WINDOW_WIDTH = 1280;
    const unsigned int WINDOW_HEIGHT = 720;
    //https://stackoverflow.com/questions/48172751/dxgi-monitors-enumeration-does-not-give-full-size-for-dell-p2715q-monitor
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

    EnableDebugLayer();
    EnableGPUBasedValidation();

    D3DPtr<IDXGIFactory2> factory = (IDXGIFactory2*)CreateFactory();
    D3DPtr<IDXGIAdapter1> adapter = GetAdapter(factory.Get(), 1);
    DXGI_ADAPTER_DESC adapterDesc;
    adapter->GetDesc(&adapterDesc);
    OutputDebugStringW(adapterDesc.Description);

    auto windowHandle = InitializeWindow(hInstance, WINDOW_WIDTH, WINDOW_HEIGHT);

    ModelViewerScene scene;
    scene.Initialize(windowHandle, false, WINDOW_WIDTH, WINDOW_HEIGHT, adapter);

    MSG msg = { };

    while (!(GetKeyState(VK_ESCAPE) & 0x8000) && msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            scene.Update();
            scene.Render();
        }

    }

    return 0;
}