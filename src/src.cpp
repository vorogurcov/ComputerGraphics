#include "RENDER_DEVICE.H"
#include "TRIANGLE_COMPONENT.H" 
#include "CUBE_COMPONENT.H"
#include "SKYBOX_COMPONENT.H"
#include "transparent_quad_component.h"
#include <windows.h>
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

RenderDevice* g_rd = nullptr;
TriangleComponent* g_tri = nullptr;
CubeComponent* g_cube = nullptr;
SkyboxComponent* g_sky = nullptr;
TransparentQuadComponent* g_transparentQuad = nullptr;

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

    HWND hWnd = CreateWindow(L"DX11_Lab", L"Frolov Ivan Lab3", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, NULL, NULL, hInst, NULL);

    g_rd = new RenderDevice();
    g_rd->Init(hWnd);

    g_tri = new TriangleComponent();
    g_tri->Init(g_rd->device);

    g_cube = new CubeComponent();
    g_cube->Init(g_rd->device);

    g_sky = new SkyboxComponent();
    g_sky->Init(g_rd->device);

    g_transparentQuad = new TransparentQuadComponent();
    g_transparentQuad->Init(g_rd->device);

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

            {
                using namespace DirectX;
                CubeFrameLightingParams lighting = {};
                lighting.cameraPos = XMFLOAT3(0.0f, 0.0f, -3.0f);
                lighting.ambientColor = XMFLOAT3(0.12f, 0.12f, 0.14f);
                lighting.lightCount = 3;
                lighting.enableNormalMapping = true;

                lighting.lights[0].position = XMFLOAT3(cosf(time) * 1.2f, 0.8f, sinf(time) * 1.2f + 0.2f);
                lighting.lights[0].color = XMFLOAT3(1.0f, 0.85f, 0.75f);
                lighting.lights[1].position = XMFLOAT3(-1.1f, 0.3f, 0.9f);
                lighting.lights[1].color = XMFLOAT3(0.35f, 0.55f, 1.0f);
                lighting.lights[2].position = XMFLOAT3(0.9f, -0.25f, -0.35f);
                lighting.lights[2].color = XMFLOAT3(0.65f, 1.0f, 0.7f);
                g_cube->SetLightingParams(lighting);

                XMMATRIX modelCubeA =
                    XMMatrixRotationAxis(XMVectorSet(1.0f, 1.0f, 0.0f, 0.0f), time) *
                    XMMatrixTranslation(-0.45f, 0.0f, 0.1f);
                XMMATRIX modelCubeB =
                    XMMatrixScaling(0.92f, 0.55f, 0.72f) *
                    XMMatrixRotationY(-time * 0.35f) *
                    XMMatrixTranslation(0.55f, 0.12f, 0.45f);

                g_cube->RenderWithModel(g_rd->context, modelCubeA, aspectRatio, g_camPitch, g_camYaw);
                g_cube->RenderWithModel(g_rd->context, modelCubeB, aspectRatio, g_camPitch, g_camYaw);
            }

            g_sky->Render(g_rd->context, aspectRatio, g_camPitch, g_camYaw);

            {
                using namespace DirectX;
                struct TransparentInstance {
                    XMFLOAT4X4 model;
                    float farthestPointDistance;
                };

                const XMVECTOR camPos = XMVectorSet(0.0f, 0.0f, -3.0f, 1.0f);
                const XMVECTOR localCorners[4] = {
                    XMVectorSet(-0.5f, -0.5f, 0.0f, 1.0f),
                    XMVectorSet( 0.5f, -0.5f, 0.0f, 1.0f),
                    XMVectorSet( 0.5f,  0.5f, 0.0f, 1.0f),
                    XMVectorSet(-0.5f,  0.5f, 0.0f, 1.0f),
                };

                auto computeFarthestDistance = [&](const XMMATRIX& model) -> float {
                    float d = 0.0f;
                    for (int i = 0; i < 4; ++i) {
                        XMVECTOR worldCorner = XMVector3TransformCoord(localCorners[i], model);
                        float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(worldCorner, camPos)));
                        d = (dist > d) ? dist : d;
                    }
                    return d;
                };

                std::vector<TransparentInstance> transparentInstances;
                transparentInstances.reserve(2);

                {
                    XMMATRIX m = XMMatrixScaling(1.55f, 1.55f, 1.55f) * XMMatrixRotationY(time * 0.2f) * XMMatrixTranslation(0.0f, 0.0f, -0.25f);
                    TransparentInstance inst = {};
                    XMStoreFloat4x4(&inst.model, m);
                    inst.farthestPointDistance = computeFarthestDistance(m);
                    transparentInstances.push_back(inst);
                }
                {
                    XMMATRIX m = XMMatrixScaling(1.15f, 1.15f, 1.15f) * XMMatrixRotationY(-time * 0.27f) * XMMatrixTranslation(0.35f, -0.2f, 0.8f);
                    TransparentInstance inst = {};
                    XMStoreFloat4x4(&inst.model, m);
                    inst.farthestPointDistance = computeFarthestDistance(m);
                    transparentInstances.push_back(inst);
                }

                std::sort(transparentInstances.begin(), transparentInstances.end(),
                    [](const TransparentInstance& a, const TransparentInstance& b) {
                        return a.farthestPointDistance > b.farthestPointDistance;
                    });

                FLOAT blendFactor[4] = { 1, 1, 1, 1 };
                g_rd->context->OMSetBlendState(g_rd->blendStateAlpha, blendFactor, 0xFFFFFFFF);
                g_rd->context->OMSetDepthStencilState(g_rd->transparentDepthState, 0);

                for (const TransparentInstance& inst : transparentInstances) {
                    DirectX::XMMATRIX model = DirectX::XMLoadFloat4x4(&inst.model);
                    g_transparentQuad->RenderWithModel(g_rd->context, model, aspectRatio, g_camPitch, g_camYaw);
                }

                g_rd->context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
                g_rd->context->OMSetDepthStencilState(g_rd->opaqueDepthState, 0);
            }

            g_rd->EndFrame();
        }
    }

    g_transparentQuad->Cleanup();
    g_cube->Cleanup();
    g_tri->Cleanup();
    g_sky->Cleanup();
    g_rd->Cleanup();

    delete g_transparentQuad;
    delete g_cube;
    delete g_tri;
    delete g_sky;
    delete g_rd;

    return (int)msg.wParam;
}