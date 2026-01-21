#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl/client.h>

#include "core/ShaderCompiler.h"

// AppMain.cpp から RootSignature / PSO / ShaderCompile を切り出す。
// 対象: Object3D, MotionDetect(CS), Particle。
class AppPipelines {
public:
    bool Initialize(ID3D12Device* device);

    ID3D12RootSignature* GetMainRootSignature() const { return mainRootSignature_.Get(); }
    ID3D12RootSignature* GetParticleRootSignature() const { return particleRootSignature_.Get(); }
    ID3D12RootSignature* GetComputeRootSignature() const { return computeRootSignature_.Get(); }

    ID3D12PipelineState* GetMainPSO() const { return mainPso_.Get(); }
    ID3D12PipelineState* GetMainOpaquePSO() const { return mainOpaquePso_.Get(); }
    ID3D12PipelineState* GetMainAlphaPSO() const { return mainAlphaPso_.Get(); }

    ID3D12PipelineState* GetComputePSO() const { return computePso_.Get(); }

    ID3D12PipelineState* GetParticlePSO() const { return particlePso_.Get(); }
    ID3D12PipelineState* GetParticleOpaquePSO() const { return particleOpaquePso_.Get(); }
    ID3D12PipelineState* GetParticleAlphaPSO() const { return particleAlphaPso_.Get(); }

private:
    // RootSignature
    Microsoft::WRL::ComPtr<ID3D12RootSignature> mainRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> particleRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;

    // PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mainPso_;      // 元の graphicsPipelineState
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mainOpaquePso_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mainAlphaPso_;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> computePso_;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> particlePso_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> particleOpaquePso_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> particleAlphaPso_;

    // Shader blobs are kept alive while PSO uses them
    ge3::core::ShaderCompiler shaderCompiler_;
    Microsoft::WRL::ComPtr<IDxcBlob> vs_;
    Microsoft::WRL::ComPtr<IDxcBlob> ps_;
    Microsoft::WRL::ComPtr<IDxcBlob> cs_;
    Microsoft::WRL::ComPtr<IDxcBlob> particleVs_;
    Microsoft::WRL::ComPtr<IDxcBlob> particlePs_;

    Microsoft::WRL::ComPtr<IDxcBlob> Compile_(const std::wstring& filePath, const wchar_t* profile);
};
