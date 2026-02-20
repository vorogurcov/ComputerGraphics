#include "RENDER_DEVICE.H"
#include "TRIANGLE_COMPONENT.H" 
#include "CUBE_COMPONENT.H"
#include <windows.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

RenderDevice* g_rd = nullptr;
TriangleComponent* g_tri = nullptr;
CubeComponent* g_cube = nullptr;

float g_camPitch = 0.0f;
float g_camYaw = 0.0f;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_SIZE && g_rd && wp != SIZE_MINIMIZED) {
        g_rd->Resize(LOWORD(lp), HIWORD(lp));
        return 0;
    }
    if (msg == WM_KEYDOWN) {
        float step = 0.05f;
        if (wp == VK_UP) g_camPitch -= step;
        if (wp == VK_DOWN) g_camPitch += step;
        if (wp == VK_LEFT) g_camYaw -= step;
        if (wp == VK_RIGHT) g_camYaw += step;
    }
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInst,
                      LoadCursor(NULL, IDC_ARROW), NULL, NULL, NULL, L"DX11_Lab", NULL };
    RegisterClassEx(&wc);

    HWND hWnd = CreateWindow(L"DX11_Lab", L"Фролов Иван Lab3", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, NULL, NULL, hInst, NULL);

    g_rd = new RenderDevice();
    g_rd->Init(hWnd);

    g_tri = new TriangleComponent();
    g_tri->Init(g_rd->device);

    g_cube = new CubeComponent();
    g_cube->Init(g_rd->device);

    ShowWindow(hWnd, nShow);

    MSG msg = {};
    const FLOAT color[4] = { 0.1f, 0.2f, 0.4f, 1.0f };
    DWORD startTime = GetTickCount();

    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            float time = (GetTickCount() - startTime) / 1000.0f;
            float aspectRatio = g_rd->height > 0 ? (float)g_rd->width / (float)g_rd->height : 1.0f;

            g_rd->PrepareFrame(color);

            g_tri->Render(g_rd->context);
            g_cube->Render(g_rd->context, time, aspectRatio, g_camPitch, g_camYaw);

            g_rd->EndFrame();
        }
    }

    g_cube->Cleanup();
    g_tri->Cleanup();
    g_rd->Cleanup();

    delete g_cube;
    delete g_tri;
    delete g_rd;

    return (int)msg.wParam;
}