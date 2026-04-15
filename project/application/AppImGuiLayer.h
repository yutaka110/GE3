#pragma once
#include <functional>
#include <Windows.h>
#include <d3d12.h>

struct AppRuntimeState;

class AppImGuiLayer {
public:
    bool Initialize(HWND hwnd, ID3D12Device* device, int bufferCount,
        DXGI_FORMAT rtvFormat, ID3D12DescriptorHeap* srvHeap);

    void BeginFrame();
    void BuildUi(AppRuntimeState& runtimeState, const std::function<void()>& onAddParticle);
    void EndFrame();

    void Render(ID3D12GraphicsCommandList* cmdList);

    void Shutdown();

private:
    bool initialized_ = false;
};
