#include "AppRunLoop.h"

#include "../../externals/imgui/imgui.h"
#include <DirectXMath.h>

#include "AppFrameRenderer.h"
#include "AppImGuiLayer.h"
#include "AppParticleSystem.h"
#include "AppPipelines.h"
#include "AppRenderResources.h"
#include "AppRuntimeState.h"
#include "AppSceneResources.h"
#include "EngineContext.h"
#include "particle/BeamRenderer.h"

using namespace DirectX;
using namespace Microsoft::WRL;

namespace {

BeamRenderer g_beam;
float g_beamTime = 0.0f;

} // namespace

AppRunLoop::AppRunLoop(
    DebugCamera& debugCamera,
    AppRuntimeState& runtimeState,
    AppSceneResources& scene,
    AppParticleSystem& particleSystem,
    AppImGuiLayer& imguiLayer,
    AppFrameRenderer& frameRenderer,
    AppPipelines& appPipelines,
    AppRenderResources& renderResources,
    ge3::graphics::SwapChain& swapChain,
    ge3::core::CommandListPool& clPool,
    EngineContext& engineContext,
    ge3::core::DescriptorHeapSet& heaps,
    ge3::core::Device& dev,
    ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap,
    Matrix4x4* wvpData,
    uint32_t windowWidth,
    uint32_t windowHeight,
    FrameLoopState& frameState,
    ID3D12CommandQueue* commandQueue,
    ID3D12Fence* fence,
    HANDLE fenceEvent)
    : debugCamera_(debugCamera),
      runtimeState_(runtimeState),
      scene_(scene),
      particleSystem_(particleSystem),
      imguiLayer_(imguiLayer),
      frameRenderer_(frameRenderer),
      appPipelines_(appPipelines),
      renderResources_(renderResources),
      swapChain_(swapChain),
      clPool_(clPool),
      engineContext_(engineContext),
      heaps_(heaps),
      dev_(dev),
      srvDescriptorHeap_(srvDescriptorHeap),
      wvpData_(wvpData),
      windowWidth_(windowWidth),
      windowHeight_(windowHeight),
      frameState_(frameState),
      commandQueue_(commandQueue),
      fence_(fence),
      fenceEvent_(fenceEvent) {}

void AppRunLoop::InitializeBeam(
    ID3D12Device* device,
    ID3D12DescriptorHeap* srvDescriptorHeap,
    uint32_t descriptorSizeSRV,
    DXGI_FORMAT rtvFormat,
    DXGI_FORMAT dsvFormat) {
    g_beam.Initialize(
        device,
        srvDescriptorHeap,
        descriptorSizeSRV,
        scene_.textureSrvHandleCPU,
        scene_.textureSrvHandleCPU2,
        rtvFormat,
        dsvFormat);
}

void AppRunLoop::UpdateFrame() {
    debugCamera_.Update();
    runtimeState_.cameraWorldPosition = debugCamera_.translation_;
    scene_.UpdateCameraWorldPosition(runtimeState_.cameraWorldPosition);
    frameState_.viewMatrix = debugCamera_.GetViewMatrix();
    frameState_.projMatrix = debugCamera_.GetProjectionMatrix();

    g_beamTime += 0.016f;
    g_beam.SetTime(g_beamTime);

    BYTE key[256] = {};
    (void)key;

    frameState_.viewProjectionMatrix = Multiply(frameState_.viewMatrix, frameState_.projMatrix);
    frameState_.deltaTime += 0.016f;
    frameState_.drawCount = particleSystem_.UpdateInstances(
        frameState_.viewProjectionMatrix,
        frameState_.deltaTime);
}

void AppRunLoop::RenderFrame() {
    imguiLayer_.BeginFrame();
    UINT backBufferIndex = swapChain_.CurrentIndex();
    ComPtr<ID3D12GraphicsCommandList> commandList =
        clPool_.Begin(backBufferIndex, appPipelines_.GetMainPSO());

    ID3D12Resource* backBuffer = swapChain_.BackBuffer(backBufferIndex);
    auto dsvHandle = heaps_.dsv.GetHandle(engineContext_.GetMainDsvIndex()).cpu;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = swapChain_.RTV(backBufferIndex);
    frameRenderer_.BeginFrame(
        commandList.Get(),
        backBuffer,
        rtv,
        dsvHandle,
        runtimeState_.clearColor);

    UpdateFrame();

    scene_.UpdateTransforms(
        runtimeState_,
        wvpData_,
        frameState_.viewMatrix,
        frameState_.projMatrix,
        windowWidth_,
        windowHeight_);

    imguiLayer_.BuildUi(runtimeState_, [&]() {
        Emitter emitterState{};
        emitterState.transform = runtimeState_.emitter.transform;
        emitterState.count = runtimeState_.emitter.count;
        emitterState.frequency = runtimeState_.emitter.frequency;
        emitterState.frequencyTime = runtimeState_.emitter.frequencyTime;
        particleSystem_.Emit(emitterState);
    });
    imguiLayer_.EndFrame();

    scene_.SyncRuntimeState(runtimeState_, frameState_.deltaTime);
    particleSystem_.SetAccelerationField({
        runtimeState_.accelerationField.acceleration,
        {runtimeState_.accelerationField.area.min, runtimeState_.accelerationField.area.max}
    });

    frameRenderer_.PrepareMainPass(
        commandList.Get(),
        runtimeState_.viewport,
        runtimeState_.scissorRect,
        appPipelines_.GetMainRootSignature(),
        appPipelines_.GetMainPSO());

    frameRenderer_.DrawSprite(
        commandList.Get(),
        srvDescriptorHeap_.Get(),
        scene_.indexBufferViewSprite,
        scene_.vertexBufferViewSprite,
        scene_.materialResourceSprite->GetGPUVirtualAddress(),
        scene_.transformationMatrixResourceSprite->GetGPUVirtualAddress(),
        scene_.directionalLightResource->GetGPUVirtualAddress(),
        scene_.cameraResource->GetGPUVirtualAddress(),
        scene_.pointLightResource->GetGPUVirtualAddress(),
        scene_.spotLightResource->GetGPUVirtualAddress());

    frameRenderer_.DrawMainModel(
        commandList.Get(),
        scene_.modelVBV,
        scene_.materialResource->GetGPUVirtualAddress(),
        scene_.sphere.cbvResource->GetGPUVirtualAddress(),
        scene_.textureSrvHandleGPU2,
        scene_.directionalLightResource->GetGPUVirtualAddress(),
        scene_.cameraResource->GetGPUVirtualAddress(),
        scene_.modelVertexCount);

    if (runtimeState_.enableParticles) {
        frameRenderer_.DrawParticles(
            commandList.Get(),
            appPipelines_.GetParticleRootSignature(),
            appPipelines_.GetParticlePSO(),
            appPipelines_.GetParticleAlphaPSO(),
            particleSystem_.InstancingSrvGpuHandle(),
            renderResources_.ParticleVertexBufferView(),
            scene_.indexBufferViewSprite,
            runtimeState_.useMonsterBall ? scene_.textureSrvHandleGPU2 : scene_.textureSrvHandleGPU,
            frameState_.drawCount);
    }

    frameRenderer_.PrepareSphere(
        commandList.Get(),
        appPipelines_.GetMainRootSignature(),
        appPipelines_.GetMainPSO(),
        scene_.cameraResource->GetGPUVirtualAddress(),
        scene_.sphere.vbv);

    {
        XMMATRIX world = XMMatrixScaling(2.0f, 4.0f, 1.0f);
        world = XMMatrixMultiply(world, XMMatrixTranslation(0.0f, 0.0f, 5.0f));
        XMMATRIX view = XMMatrixLookAtLH(
            XMVectorSet(0.f, 0.f, -5.f, 1.f),
            XMVectorSet(0.f, 0.f, 0.f, 1.f),
            XMVectorSet(0.f, 1.f, 0.f, 0.f));
        XMMATRIX proj = XMMatrixPerspectiveFovLH(
            XMConvertToRadians(60.0f),
            static_cast<float>(1280.0f) / static_cast<float>(720.0f),
            0.1f,
            100.0f);
        XMMATRIX wvp = world * view * proj;
        XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f};
        float intensity = 1.0f;
        (void)wvp;
        (void)color;
        (void)intensity;
    }

    imguiLayer_.Render(commandList.Get());
    frameRenderer_.EndFrame(commandList.Get(), backBuffer);

    clPool_.EndAndExecute(dev_);
    swapChain_.Present(dev_, 1);

    uint64_t fenceValue = engineContext_.GetFenceValue() + 1;
    engineContext_.SetFenceValue(fenceValue);
    commandQueue_->Signal(fence_, fenceValue);
    if (fence_->GetCompletedValue() < fenceValue) {
        fence_->SetEventOnCompletion(fenceValue, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}
