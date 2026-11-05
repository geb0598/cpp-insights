#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include "insights/insights_d3d11.h"

// ---------------------------------------------------
// Stat declarations
// ---------------------------------------------------
INSIGHTS_DECLARE_STATGROUP("Rendering", STATGROUP_RENDERING);

// CPU - top level
INSIGHTS_DECLARE_STAT("CPU_MainLoop",    STAT_CPU_MAIN,      STATGROUP_RENDERING);

// CPU - update phase
INSIGHTS_DECLARE_STAT("CPU_Update",      STAT_CPU_UPDATE,    STATGROUP_RENDERING);
INSIGHTS_DECLARE_STAT("CPU_Physics",     STAT_CPU_PHYSICS,   STATGROUP_RENDERING);
INSIGHTS_DECLARE_STAT("CPU_Animation",   STAT_CPU_ANIM,      STATGROUP_RENDERING);

// CPU - render phase
INSIGHTS_DECLARE_STAT("CPU_Render",      STAT_CPU_RENDER,    STATGROUP_RENDERING);
INSIGHTS_DECLARE_STAT("CPU_ShadowPass",  STAT_CPU_SHADOW,    STATGROUP_RENDERING);
INSIGHTS_DECLARE_STAT("CPU_ShadowDraw",  STAT_CPU_SHADOW_DC, STATGROUP_RENDERING);
INSIGHTS_DECLARE_STAT("CPU_GBuffer",     STAT_CPU_GBUFFER,   STATGROUP_RENDERING);
INSIGHTS_DECLARE_STAT("CPU_DrawMesh",    STAT_CPU_MESH,      STATGROUP_RENDERING);
INSIGHTS_DECLARE_STAT("CPU_Lighting",    STAT_CPU_LIGHT,     STATGROUP_RENDERING);
INSIGHTS_DECLARE_STAT("CPU_PostProcess", STAT_CPU_POST,      STATGROUP_RENDERING);
INSIGHTS_DECLARE_STAT("CPU_Bloom",       STAT_CPU_BLOOM,     STATGROUP_RENDERING);
INSIGHTS_DECLARE_STAT("CPU_ToneMap",     STAT_CPU_TONE,      STATGROUP_RENDERING);
INSIGHTS_DECLARE_STAT("CPU_UI",          STAT_CPU_UI,        STATGROUP_RENDERING);
INSIGHTS_DECLARE_STAT("CPU_Present",     STAT_CPU_PRES,      STATGROUP_RENDERING);

// GPU
INSIGHTS_DECLARE_STAT("GPU_Clear",       STAT_GPU_CLEAR,     STATGROUP_RENDERING);
INSIGHTS_DECLARE_STAT("GPU_ShadowPass",  STAT_GPU_SHADOW,    STATGROUP_RENDERING);
INSIGHTS_DECLARE_STAT("GPU_ShadowDraw",  STAT_GPU_SHADOW_DC, STATGROUP_RENDERING);
INSIGHTS_DECLARE_STAT("GPU_GBuffer",     STAT_GPU_GBUFFER,   STATGROUP_RENDERING);
INSIGHTS_DECLARE_STAT("GPU_DrawMesh",    STAT_GPU_MESH,      STATGROUP_RENDERING);
INSIGHTS_DECLARE_STAT("GPU_Lighting",    STAT_GPU_LIGHT,     STATGROUP_RENDERING);
INSIGHTS_DECLARE_STAT("GPU_PostProcess", STAT_GPU_POST,      STATGROUP_RENDERING);
INSIGHTS_DECLARE_STAT("GPU_Present",     STAT_GPU_PRES,      STATGROUP_RENDERING);

static bool g_is_running = true;

LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_DESTROY:
        g_is_running = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

struct Vertex {
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT4 color;
};

struct ConstantBuffer {
    DirectX::XMMATRIX mvp;
};

const char* g_shader_code = R"(
cbuffer CB : register(b0) {
    matrix MVP;
};
struct VS_IN {
    float3 pos : POSITION;
    float4 col : COLOR;
};
struct PS_IN {
    float4 pos : SV_POSITION;
    float4 col : COLOR;
};
PS_IN VSMain(VS_IN input) {
    PS_IN output;
    output.pos = mul(float4(input.pos, 1.0f), MVP);
    output.col = input.col;
    return output;
}
float4 PSMain(PS_IN input) : SV_Target {
    return input.col;
}
)";

static void BusyWait(int microseconds) {
    auto end = std::chrono::steady_clock::now()
             + std::chrono::microseconds(microseconds);
    while (std::chrono::steady_clock::now() < end) {}
}

int main(int argc, char** argv) {
    // --frames N: run N frames, then exit with code 0. Without it, run until the window closes.
    int frame_count = -1;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc && std::atoi(argv[i + 1]) > 0) {
            frame_count = std::atoi(argv[++i]);
        } else {
            std::cerr << "usage: gpu-stub [--frames N]\n";
            return 1;
        }
    }

    std::cout << "Starting DX11 GPU Stub...\n";

    HINSTANCE hinstance = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc   = {};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hinstance;
    wc.lpszClassName = L"insight-gpu-stub";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowW(
        wc.lpszClassName, L"DX11 GPU Stub",
        WS_OVERLAPPEDWINDOW,
        100, 100, 1280, 720,
        nullptr, nullptr, hinstance, nullptr);

    Microsoft::WRL::ComPtr<ID3D11Device>           device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>    context;
    Microsoft::WRL::ComPtr<IDXGISwapChain>         swap_chain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv;

    DXGI_SWAP_CHAIN_DESC sd                       = {};
    sd.BufferCount                                = 2;
    sd.BufferDesc.Width                           = 1280;
    sd.BufferDesc.Height                          = 720;
    sd.BufferDesc.Format                          = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator           = 60;
    sd.BufferDesc.RefreshRate.Denominator         = 1;
    sd.Flags                                      = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage                                = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow                               = hwnd;
    sd.SampleDesc.Count                           = 1;
    sd.Windowed                                   = TRUE;
    sd.SwapEffect                                 = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL       feature_level;
    const D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0 };

    if (FAILED(D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        0, feature_levels, 1, D3D11_SDK_VERSION,
        &sd, swap_chain.GetAddressOf(), device.GetAddressOf(),
        &feature_level, context.GetAddressOf()))) {
        return -1;
    }

    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
        swap_chain->GetBuffer(0, IID_PPV_ARGS(back_buffer.GetAddressOf()));
        device->CreateRenderTargetView(back_buffer.Get(), nullptr, rtv.GetAddressOf());
    }

    {
        D3D11_TEXTURE2D_DESC ds_desc   = {};
        ds_desc.Width                  = 1280;
        ds_desc.Height                 = 720;
        ds_desc.MipLevels              = 1;
        ds_desc.ArraySize              = 1;
        ds_desc.Format                 = DXGI_FORMAT_D24_UNORM_S8_UINT;
        ds_desc.SampleDesc.Count       = 1;
        ds_desc.Usage                  = D3D11_USAGE_DEFAULT;
        ds_desc.BindFlags              = D3D11_BIND_DEPTH_STENCIL;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_stencil;
        device->CreateTexture2D(&ds_desc, nullptr, depth_stencil.GetAddressOf());
        device->CreateDepthStencilView(depth_stencil.Get(), nullptr, dsv.GetAddressOf());
    }

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  pixel_shader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  input_layout;

    {
        Microsoft::WRL::ComPtr<ID3D10Blob> vs_blob;
        Microsoft::WRL::ComPtr<ID3D10Blob> ps_blob;
        D3DCompile(g_shader_code, strlen(g_shader_code), nullptr, nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, vs_blob.GetAddressOf(), nullptr);
        D3DCompile(g_shader_code, strlen(g_shader_code), nullptr, nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, ps_blob.GetAddressOf(), nullptr);
        device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, vertex_shader.GetAddressOf());
        device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, pixel_shader.GetAddressOf());

        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        device->CreateInputLayout(layout, 2, vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), input_layout.GetAddressOf());
    }

    // Triangle
    Vertex tri_vertices[] = {
        { {  0.0f,  0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
        { {  0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
    };
 
    // Cube
    Vertex cube_vertices[] = {
        { { -1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f, 1.0f, 1.0f } },
        { {  1.0f,  1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { {  1.0f,  1.0f,  1.0f }, { 1.0f, 0.0f, 1.0f, 1.0f } },
        { { -1.0f,  1.0f,  1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
        { { -1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { {  1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f } },
        { {  1.0f, -1.0f,  1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
        { { -1.0f, -1.0f,  1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f } }
    };
    WORD cube_indices[] = {
        3,1,0, 2,1,3, 0,5,4, 1,5,0, 3,4,7, 0,4,3,
        1,6,5, 2,6,1, 2,7,6, 3,7,2, 6,4,5, 7,4,6
    };

    Microsoft::WRL::ComPtr<ID3D11Buffer> tri_vb;
    {
        D3D11_BUFFER_DESC      d  = { sizeof(tri_vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER };
        D3D11_SUBRESOURCE_DATA sd = { tri_vertices };
        device->CreateBuffer(&d, &sd, tri_vb.GetAddressOf());
    }

    Microsoft::WRL::ComPtr<ID3D11Buffer> cube_vb;
    {
        D3D11_BUFFER_DESC      d  = { sizeof(cube_vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER };
        D3D11_SUBRESOURCE_DATA sd = { cube_vertices };
        device->CreateBuffer(&d, &sd, cube_vb.GetAddressOf());
    }

    Microsoft::WRL::ComPtr<ID3D11Buffer> cube_ib;
    {
        D3D11_BUFFER_DESC      d  = { sizeof(cube_indices), D3D11_USAGE_DEFAULT, D3D11_BIND_INDEX_BUFFER };
        D3D11_SUBRESOURCE_DATA sd = { cube_indices };
        device->CreateBuffer(&d, &sd, cube_ib.GetAddressOf());
    }

    Microsoft::WRL::ComPtr<ID3D11Buffer> cb;
    {
        D3D11_BUFFER_DESC d = { sizeof(ConstantBuffer), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER };
        device->CreateBuffer(&d, nullptr, cb.GetAddressOf());
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    INSIGHTS_GPU_INIT_D3D11(device.Get(), context.Get());
    INSIGHTS_INITIALIZE();

    std::cout << "Insight connected. Running DX11 loop...\n";

    const DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(
        DirectX::XM_PIDIV4, 1280.0f / 720.0f, 0.01f, 100.0f);
    float angle = 0.0f;

    auto SetMVP = [&](float tx, float ty, float tz, float rx = 0.f, float ry = 0.f) {
        ConstantBuffer data;
        data.mvp = DirectX::XMMatrixTranspose(
            DirectX::XMMatrixRotationX(rx) *
            DirectX::XMMatrixRotationY(ry) *
            DirectX::XMMatrixTranslation(tx, ty, tz) * proj);
        context->UpdateSubresource(cb.Get(), 0, nullptr, &data, 0, 0);
    };

    auto SetupPipeline = [&]() {
        D3D11_VIEWPORT vp = { 0.f, 0.f, 1280.f, 720.f, 0.f, 1.f };
        context->RSSetViewports(1, &vp);
        context->OMSetRenderTargets(1, rtv.GetAddressOf(), dsv.Get());
        context->IASetInputLayout(input_layout.Get());
        context->VSSetShader(vertex_shader.Get(), nullptr, 0);
        context->PSSetShader(pixel_shader.Get(), nullptr, 0);
        context->VSSetConstantBuffers(0, 1, cb.GetAddressOf());
    };

    MSG msg   = {};
    int frame = 0;
    while (g_is_running && (frame_count < 0 || frame < frame_count)) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!g_is_running) break;

        angle += 0.01f;

        INSIGHTS_FRAME_BEGIN();
        {
            INSIGHTS_SCOPE(STAT_CPU_MAIN);

            // --------------------------------------------------
            // Update
            // --------------------------------------------------
            {
                INSIGHTS_SCOPE(STAT_CPU_UPDATE);

                {
                    INSIGHTS_SCOPE(STAT_CPU_PHYSICS);
                    BusyWait(800);
                }
                {
                    INSIGHTS_SCOPE(STAT_CPU_ANIM);
                    BusyWait(400);
                }
            }

            // --------------------------------------------------
            // Render
            // --------------------------------------------------
            {
                INSIGHTS_SCOPE(STAT_CPU_RENDER);

                SetupPipeline();

                // Clear
                {
                    INSIGHTS_GPU_SCOPE(STAT_GPU_CLEAR);
                    const float clear_color[] = { 0.1f, 0.15f, 0.2f, 1.0f };
                    context->ClearRenderTargetView(rtv.Get(), clear_color);
                    context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
                }

                // Shadow pass — 8 shadow casters
                {
                    INSIGHTS_SCOPE(STAT_CPU_SHADOW);
                    INSIGHTS_GPU_SCOPE(STAT_GPU_SHADOW);

                    UINT stride = sizeof(Vertex), offset = 0;
                    context->IASetVertexBuffers(0, 1, cube_vb.GetAddressOf(), &stride, &offset);
                    context->IASetIndexBuffer(cube_ib.Get(), DXGI_FORMAT_R16_UINT, 0);
                    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                    for (int i = 0; i < 8; ++i) {
                        INSIGHTS_SCOPE(STAT_CPU_SHADOW_DC);
                        INSIGHTS_GPU_SCOPE(STAT_GPU_SHADOW_DC);
                        BusyWait(50);
                        float tx = -7.0f + i * 2.0f;
                        SetMVP(tx, 0.0f, 8.0f, angle * 0.3f, angle + i * 0.4f);
                        context->DrawIndexed(36, 0, 0);
                    }
                }

                // GBuffer pass — 8 cubes + 8 triangles
                {
                    INSIGHTS_SCOPE(STAT_CPU_GBUFFER);
                    INSIGHTS_GPU_SCOPE(STAT_GPU_GBUFFER);

                    for (int i = 0; i < 8; ++i) {
                        INSIGHTS_SCOPE(STAT_CPU_MESH);
                        INSIGHTS_GPU_SCOPE(STAT_GPU_MESH);
                        BusyWait(40);

                        UINT stride = sizeof(Vertex), offset = 0;
                        context->IASetVertexBuffers(0, 1, cube_vb.GetAddressOf(), &stride, &offset);
                        context->IASetIndexBuffer(cube_ib.Get(), DXGI_FORMAT_R16_UINT, 0);
                        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                        float tx = -7.0f + i * 2.0f;
                        SetMVP(tx, 0.0f, 6.0f, angle * 0.5f, angle + i * 0.8f);
                        context->DrawIndexed(36, 0, 0);
                    }

                    for (int i = 0; i < 8; ++i) {
                        INSIGHTS_SCOPE(STAT_CPU_MESH);
                        INSIGHTS_GPU_SCOPE(STAT_GPU_MESH);
                        BusyWait(30);

                        UINT stride = sizeof(Vertex), offset = 0;
                        context->IASetVertexBuffers(0, 1, tri_vb.GetAddressOf(), &stride, &offset);
                        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                        float tx = -7.0f + i * 2.0f;
                        SetMVP(tx, 1.5f, 6.0f, 0.f, angle * 1.2f + i * 0.5f);
                        context->Draw(3, 0);
                    }
                }

                // Lighting
                {
                    INSIGHTS_SCOPE(STAT_CPU_LIGHT);
                    INSIGHTS_GPU_SCOPE(STAT_GPU_LIGHT);
                    BusyWait(300);
                }

                // Post-process
                {
                    INSIGHTS_SCOPE(STAT_CPU_POST);
                    INSIGHTS_GPU_SCOPE(STAT_GPU_POST);
                    {
                        INSIGHTS_SCOPE(STAT_CPU_BLOOM);
                        BusyWait(500);
                    }
                    {
                        INSIGHTS_SCOPE(STAT_CPU_TONE);
                        BusyWait(200);
                    }
                }

                // UI
                {
                    INSIGHTS_SCOPE(STAT_CPU_UI);
                    BusyWait(200);
                }
            }

            // --------------------------------------------------
            // Present
            // --------------------------------------------------
            {
                INSIGHTS_SCOPE(STAT_CPU_PRES);
                INSIGHTS_GPU_SCOPE(STAT_GPU_PRES);
                swap_chain->Present(1, 0);
            }
        }
        INSIGHTS_FRAME_END();
        ++frame;
    }

    INSIGHTS_SHUTDOWN();
    return 0;
}