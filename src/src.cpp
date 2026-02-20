#include "RENDER_DEVICE.H"
#include "TRIANGLE_COMPONENT.H"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

RenderDevice* g_rd = nullptr;
TriangleComponent* g_tri = nullptr;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_SIZE && g_rd && wp != SIZE_MINIMIZED) {
        g_rd->Resize(LOWORD(lp), HIWORD(lp));
        return 0;
    }
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInst,
                      LoadCursor(NULL, IDC_ARROW), NULL, NULL, NULL, L"DX11_Lab", NULL };
    RegisterClassEx(&wc);

    HWND hWnd = CreateWindow(L"DX11_Lab", L"Frolov Ivan, Lab 2", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, NULL, NULL, hInst, NULL);

    g_rd = new RenderDevice();
    g_rd->Init(hWnd);

    g_tri = new TriangleComponent();
    g_tri->Init(g_rd->device);

    ShowWindow(hWnd, nShow);

    MSG msg = {};
    const FLOAT color[4] = { 0.1f, 0.2f, 0.4f, 1.0f };

    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            g_rd->PrepareFrame(color);
            g_tri->Render(g_rd->context);
            g_rd->EndFrame();
        }
    }

    g_tri->Cleanup();
    g_rd->Cleanup();
    delete g_tri; delete g_rd;
    return (int)msg.wParam;
}