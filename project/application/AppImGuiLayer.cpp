#include "AppImGuiLayer.h"

#include "AppRuntimeState.h"

#include "../../externals/imgui/imgui.h"
#include "../../externals/imgui/imgui_impl_dx12.h"
#include "../../externals/imgui/imgui_impl_win32.h"

bool AppImGuiLayer::Initialize(HWND hwnd,
    ID3D12Device* device,
    int bufferCount,
    DXGI_FORMAT rtvFormat,
    ID3D12DescriptorHeap* srvHeap) {
    if (initialized_) {
        return true;
    }

    if (!hwnd || !device || !srvHeap || bufferCount <= 0) {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(hwnd)) {
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplDX12_Init(device,
        bufferCount,
        rtvFormat,
        srvHeap,
        srvHeap->GetCPUDescriptorHandleForHeapStart(),
        srvHeap->GetGPUDescriptorHandleForHeapStart())) {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    initialized_ = true;
    return true;
}

void AppImGuiLayer::BeginFrame() {
    if (!initialized_) {
        return;
    }

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void AppImGuiLayer::BuildUi(AppRuntimeState& runtimeState, const std::function<void()>& onAddParticle) {
    if (!initialized_) {
        return;
    }

    ImGui::ShowDemoWindow();

    ImGui::ColorEdit3("Light Color",
        reinterpret_cast<float*>(&runtimeState.directionalLightData.color));

    ImGui::SliderFloat3(
        "Light Direction",
        reinterpret_cast<float*>(&runtimeState.directionalLightData.direction),
        -1.0f,
        1.0f);

    ImGui::SliderFloat(
        "Intensity",
        &runtimeState.directionalLightData.intensity,
        0.0f,
        10.0f);
    ImGui::Checkbox("Show Particles", &runtimeState.enableParticles);

    ImGui::Begin("Material Settings");
    ImGui::ColorEdit4("Material Color",
        reinterpret_cast<float*>(&runtimeState.materialData.color));
    ImGui::Checkbox("Enable Lighting", reinterpret_cast<bool*>(&runtimeState.materialData.enableLighting));
    ImGui::SliderFloat("Shininess", &runtimeState.materialData.shininess, 1.0f, 64.0f);

    ImGui::Text("Recommended: shininess 8-16");

    ImGui::Text("Scale");
    ImGui::DragFloat3("Scale", reinterpret_cast<float*>(&runtimeState.transform.scale),
        0.01f, 0.01f, 10.0f);

    ImGui::Text("Rotate");
    ImGui::DragFloat3("Rotate", reinterpret_cast<float*>(&runtimeState.transform.rotate),
        0.01f, -3.14f, 3.14f);

    ImGui::Text("Translate");
    ImGui::DragFloat3("Translate",
        reinterpret_cast<float*>(&runtimeState.transform.translate), 0.01f,
        -100.0f, 100.0f);

    ImGui::DragFloat2("UVTranslate", &runtimeState.uvTransformSprite.translate.x, 0.01f,
        -10.0f, 10.0f);
    ImGui::DragFloat2("UVScale", &runtimeState.uvTransformSprite.scale.x, 0.01f, -10.0f,
        10.0f);
    ImGui::SliderAngle("UVRotate", &runtimeState.uvTransformSprite.rotate.z);

    ImGui::DragFloat3(
        "EmitterTranslate",
        &runtimeState.emitter.transform.translate.x,
        0.01f,
        -100.0f,
        100.0f);

    ImGui::DragFloat3("Field Accel", &runtimeState.accelerationField.acceleration.x, 0.1f);
    ImGui::DragFloat3("Field Min", &runtimeState.accelerationField.area.min.x, 0.1f);
    ImGui::DragFloat3("Field Max", &runtimeState.accelerationField.area.max.x, 0.1f);

    if (ImGui::Button("Add Particle (Emitter)") && onAddParticle) {
        onAddParticle();
    }

    ImGui::DragFloat3("Point Pos", &runtimeState.pointLightData.position.x, 0.05f);
    ImGui::DragFloat("Point Intensity", &runtimeState.pointLightData.intensity, 0.05f, 0.0f, 50.0f);
    ImGui::DragFloat("Point Radius", &runtimeState.pointLightData.radius, 0.1f, 0.1f, 100.0f);
    ImGui::DragFloat("Point Decay", &runtimeState.pointLightData.decay, 0.05f, 0.1f, 8.0f);

    ImGui::End();
}

void AppImGuiLayer::EndFrame() {
    if (!initialized_) {
        return;
    }

    ImGui::Render();
}

void AppImGuiLayer::Render(ID3D12GraphicsCommandList* cmdList) {
    if (!initialized_ || !cmdList) {
        return;
    }

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
}

void AppImGuiLayer::Shutdown() {
    if (!initialized_) {
        return;
    }

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    initialized_ = false;
}
