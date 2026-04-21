#include "AppPipelines.h"

#include <cassert>
#include <cstdio>
#include <filesystem>

using Microsoft::WRL::ComPtr;

namespace {

std::filesystem::path GetModuleDirectory() {
    wchar_t modulePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(modulePath).parent_path();
}

std::filesystem::path ResolveShaderPath(const std::wstring& filePath) {
    const std::filesystem::path requested(filePath);
    if (std::filesystem::exists(requested)) {
        return requested;
    }

    const std::filesystem::path bases[] = {
        std::filesystem::current_path(),
        GetModuleDirectory(),
    };

    for (const auto& base : bases) {
        std::filesystem::path probe = base;
        for (int depth = 0; depth < 6; ++depth) {
            const std::filesystem::path direct = probe / requested;
            if (std::filesystem::exists(direct)) {
                return direct;
            }

            const std::filesystem::path projectRelative = probe / L"project" / requested;
            if (std::filesystem::exists(projectRelative)) {
                return projectRelative;
            }

            if (!probe.has_parent_path()) {
                break;
            }
            probe = probe.parent_path();
        }
    }

    return requested;
}

bool FailHr(const char* stage, HRESULT hr) {
    char message[256]{};
    std::snprintf(message, sizeof(message), "[AppPipelines] %s failed. HRESULT=0x%08X\n",
                  stage, static_cast<unsigned int>(hr));
    OutputDebugStringA(message);
    return false;
}

} // namespace

ComPtr<IDxcBlob> AppPipelines::Compile_(const std::wstring& filePath, const wchar_t* profile) {
    if (!shaderCompiler_.Initialize()) {
        OutputDebugStringA("[Error] ShaderCompiler.Initialize failed\n");
        return nullptr;
    }

    const std::wstring entryPoint = L"main";
    const std::filesystem::path resolvedPath = ResolveShaderPath(filePath);
    if (!std::filesystem::exists(resolvedPath)) {
        OutputDebugStringW(L"[AppPipelines] Shader file not found: ");
        OutputDebugStringW(filePath.c_str());
        OutputDebugStringW(L"\n");
    }

    auto blob = shaderCompiler_.CompileFromFile(resolvedPath.wstring(), entryPoint, profile);
    if (!blob) {
        OutputDebugStringW(L"[AppPipelines] Shader compile failed: ");
        OutputDebugStringW(resolvedPath.wstring().c_str());
        OutputDebugStringW(L"\n");
        return nullptr;
    }
    return blob;
}

bool AppPipelines::Initialize(ID3D12Device* device) {
    if (!device) return false;

    HRESULT hr = S_OK;

    // ------------------------------
    // Main RootSignature (Object3D)
    // ------------------------------
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};

    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0;
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE receivedRange = {};
    receivedRange.BaseShaderRegister = 4;
    receivedRange.NumDescriptors = 1;
    receivedRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    receivedRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE motionMaskRange = {};
    motionMaskRange.BaseShaderRegister = 2;
    motionMaskRange.NumDescriptors = 1;
    motionMaskRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    motionMaskRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER rootParameters[9] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].Descriptor.ShaderRegister = 0;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister = 1;

    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[4].DescriptorTable.pDescriptorRanges = &receivedRange;

    rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[5].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[5].DescriptorTable.pDescriptorRanges = &motionMaskRange;

    rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[6].Descriptor.ShaderRegister = 2;

    rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[7].Descriptor.ShaderRegister = 3;

    rootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[8].Descriptor.ShaderRegister = 4;

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1,
                                    &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA(reinterpret_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
                                    IID_PPV_ARGS(&mainRootSignature_));
    if (FAILED(hr)) return FailHr("CreateRootSignature(Main)", hr);

    // ------------------------------
    // Sprite RootSignature
    // ------------------------------
    D3D12_DESCRIPTOR_RANGE spriteTextureRange{};
    spriteTextureRange.BaseShaderRegister = 0;
    spriteTextureRange.NumDescriptors = 1;
    spriteTextureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    spriteTextureRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER spriteRootParams[3] = {};
    spriteRootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    spriteRootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    spriteRootParams[0].Descriptor.ShaderRegister = 0;

    spriteRootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    spriteRootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    spriteRootParams[1].Descriptor.ShaderRegister = 0;

    spriteRootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    spriteRootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    spriteRootParams[2].DescriptorTable.NumDescriptorRanges = 1;
    spriteRootParams[2].DescriptorTable.pDescriptorRanges = &spriteTextureRange;

    D3D12_ROOT_SIGNATURE_DESC spriteRsDesc{};
    spriteRsDesc.NumParameters = _countof(spriteRootParams);
    spriteRsDesc.pParameters = spriteRootParams;
    spriteRsDesc.NumStaticSamplers = _countof(staticSamplers);
    spriteRsDesc.pStaticSamplers = staticSamplers;
    spriteRsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> spriteSigBlob;
    ComPtr<ID3DBlob> spriteErrBlob;
    hr = D3D12SerializeRootSignature(&spriteRsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                    &spriteSigBlob, &spriteErrBlob);
    if (FAILED(hr)) {
        if (spriteErrBlob) {
            OutputDebugStringA(reinterpret_cast<const char*>(spriteErrBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = device->CreateRootSignature(0, spriteSigBlob->GetBufferPointer(), spriteSigBlob->GetBufferSize(),
                                    IID_PPV_ARGS(&spriteRootSignature_));
    if (FAILED(hr)) return FailHr("CreateRootSignature(Sprite)", hr);

    // ------------------------------
    // Particle RootSignature
    // ------------------------------
    D3D12_DESCRIPTOR_RANGE particleInstancingRange{};
    particleInstancingRange.BaseShaderRegister = 0;
    particleInstancingRange.NumDescriptors = 1;
    particleInstancingRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    particleInstancingRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_DESCRIPTOR_RANGE particleTextureRange{};
    particleTextureRange.BaseShaderRegister = 0;
    particleTextureRange.NumDescriptors = 1;
    particleTextureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    particleTextureRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER particleRootParams[3] = {};
    particleRootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    particleRootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    particleRootParams[0].Descriptor.ShaderRegister = 0;

    particleRootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    particleRootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    particleRootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    particleRootParams[1].DescriptorTable.pDescriptorRanges = &particleInstancingRange;

    particleRootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    particleRootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    particleRootParams[2].DescriptorTable.NumDescriptorRanges = 1;
    particleRootParams[2].DescriptorTable.pDescriptorRanges = &particleTextureRange;

    D3D12_ROOT_SIGNATURE_DESC particleRsDesc{};
    particleRsDesc.NumParameters = _countof(particleRootParams);
    particleRsDesc.pParameters = particleRootParams;
    particleRsDesc.NumStaticSamplers = _countof(staticSamplers);
    particleRsDesc.pStaticSamplers = staticSamplers;
    particleRsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> particleSigBlob;
    ComPtr<ID3DBlob> particleErrBlob;
    hr = D3D12SerializeRootSignature(&particleRsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                    &particleSigBlob, &particleErrBlob);
    if (FAILED(hr)) {
        if (particleErrBlob) {
            OutputDebugStringA(reinterpret_cast<const char*>(particleErrBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = device->CreateRootSignature(0, particleSigBlob->GetBufferPointer(), particleSigBlob->GetBufferSize(),
                                    IID_PPV_ARGS(&particleRootSignature_));
    if (FAILED(hr)) return FailHr("CreateRootSignature(Particle)", hr);

    // ------------------------------
    // Compute RootSignature (MotionDetect)
    // ------------------------------
    // t4: Y, t5: UV, u0: output
    D3D12_DESCRIPTOR_RANGE ranges[2] = {};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].NumDescriptors = 2;
    ranges[0].BaseShaderRegister = 4;
    ranges[0].OffsetInDescriptorsFromTableStart = 0;

    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[1].NumDescriptors = 1;
    ranges[1].BaseShaderRegister = 0;
    ranges[1].OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER csParams[2] = {};
    csParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    csParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    csParams[0].DescriptorTable.NumDescriptorRanges = 1;
    csParams[0].DescriptorTable.pDescriptorRanges = &ranges[0];

    csParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    csParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    csParams[1].DescriptorTable.NumDescriptorRanges = 1;
    csParams[1].DescriptorTable.pDescriptorRanges = &ranges[1];

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
    rootSigDesc.NumParameters = _countof(csParams);
    rootSigDesc.pParameters = csParams;
    rootSigDesc.NumStaticSamplers = 0;
    rootSigDesc.pStaticSamplers = nullptr;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> csSigBlob;
    ComPtr<ID3DBlob> csErrBlob;
    hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &csSigBlob, &csErrBlob);
    if (FAILED(hr)) {
        if (csErrBlob) {
            OutputDebugStringA(reinterpret_cast<const char*>(csErrBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = device->CreateRootSignature(0, csSigBlob->GetBufferPointer(), csSigBlob->GetBufferSize(),
                                    IID_PPV_ARGS(&computeRootSignature_));
    if (FAILED(hr)) return FailHr("CreateRootSignature(Compute)", hr);

    // ------------------------------
    // Compile shaders
    // ------------------------------
    vs_ = Compile_(L"resources/Object3D.VS.hlsl", L"vs_6_0");
    ps_ = Compile_(L"resources/Object3D.PS.hlsl", L"ps_6_0");
    spriteVs_ = Compile_(L"resources/Sprite.VS.hlsl", L"vs_6_0");
    spritePs_ = Compile_(L"resources/Sprite.PS.hlsl", L"ps_6_0");
    cs_ = Compile_(L"MotionDetect.CS.hlsl", L"cs_6_0");
    particleVs_ = Compile_(L"resources/Particle.VS.hlsl", L"vs_6_0");
    particlePs_ = Compile_(L"resources/Particle.PS.hlsl", L"ps_6_0");

    if (!vs_ || !ps_ || !spriteVs_ || !spritePs_ || !cs_ || !particleVs_ || !particlePs_) {
        OutputDebugStringA("[AppPipelines] Shader compilation failed.\n");
        return false;
    }

    // ------------------------------
    // InputLayout
    // ------------------------------
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    // BlendState
    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // Rasterizer
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    // Depth
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // ------------------------------
    // Main PSO (graphicsPipelineState)
    // ------------------------------
    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = mainRootSignature_.Get();
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
    graphicsPipelineStateDesc.VS = { vs_->GetBufferPointer(), vs_->GetBufferSize() };
    graphicsPipelineStateDesc.PS = { ps_->GetBufferPointer(), ps_->GetBufferSize() };
    graphicsPipelineStateDesc.BlendState = blendDesc;
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&mainPso_));
    if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(Main)", hr);

    // ------------------------------
    // Sprite PSO
    // ------------------------------
    D3D12_INPUT_ELEMENT_DESC spriteElements[2] = {};
    spriteElements[0].SemanticName = "POSITION";
    spriteElements[0].SemanticIndex = 0;
    spriteElements[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    spriteElements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    spriteElements[1].SemanticName = "TEXCOORD";
    spriteElements[1].SemanticIndex = 0;
    spriteElements[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    spriteElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC spriteInputLayout{};
    spriteInputLayout.pInputElementDescs = spriteElements;
    spriteInputLayout.NumElements = _countof(spriteElements);

    D3D12_BLEND_DESC spriteBlend{};
    spriteBlend.RenderTarget[0].BlendEnable = TRUE;
    spriteBlend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    spriteBlend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    spriteBlend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    spriteBlend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    spriteBlend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    spriteBlend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    spriteBlend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_DEPTH_STENCIL_DESC spriteDepth{};
    spriteDepth.DepthEnable = TRUE;
    spriteDepth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    spriteDepth.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC spriteDesc{};
    spriteDesc.pRootSignature = spriteRootSignature_.Get();
    spriteDesc.InputLayout = spriteInputLayout;
    spriteDesc.VS = { spriteVs_->GetBufferPointer(), spriteVs_->GetBufferSize() };
    spriteDesc.PS = { spritePs_->GetBufferPointer(), spritePs_->GetBufferSize() };
    spriteDesc.BlendState = spriteBlend;
    spriteDesc.RasterizerState = rasterizerDesc;
    spriteDesc.DepthStencilState = spriteDepth;
    spriteDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    spriteDesc.NumRenderTargets = 1;
    spriteDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    spriteDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    spriteDesc.SampleDesc.Count = 1;
    spriteDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    hr = device->CreateGraphicsPipelineState(&spriteDesc, IID_PPV_ARGS(&spritePso_));
    if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(Sprite)", hr);

    // ------------------------------
    // Compute PSO
    // ------------------------------
    D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc{};
    computePsoDesc.pRootSignature = computeRootSignature_.Get();
    computePsoDesc.CS = { cs_->GetBufferPointer(), cs_->GetBufferSize() };
    hr = device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&computePso_));
    if (FAILED(hr)) return FailHr("CreateComputePipelineState(MotionDetect)", hr);

    // ------------------------------
    // Particle PSOs
    // ------------------------------
    auto MakeParticleOpaqueBlend = []() {
        D3D12_BLEND_DESC d{};
        d.AlphaToCoverageEnable = FALSE;
        d.IndependentBlendEnable = FALSE;
        auto& rt = d.RenderTarget[0];
        rt.BlendEnable = FALSE;
        rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        return d;
    };

    auto MakeParticleAlphaBlend = []() {
        D3D12_BLEND_DESC d{};
        d.AlphaToCoverageEnable = FALSE;
        d.IndependentBlendEnable = FALSE;
        auto& rt = d.RenderTarget[0];
        rt.BlendEnable = TRUE;
        rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        rt.BlendOp = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        return d;
    };

    // Particle quad uses VertexData: position(float4), texcoord(float2), normal(float3).
    D3D12_INPUT_ELEMENT_DESC particleElements[3] = {};
    particleElements[0].SemanticName = "POSITION";
    particleElements[0].SemanticIndex = 0;
    particleElements[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    particleElements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    particleElements[1].SemanticName = "TEXCOORD";
    particleElements[1].SemanticIndex = 0;
    particleElements[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    particleElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    particleElements[2].SemanticName = "NORMAL";
    particleElements[2].SemanticIndex = 0;
    particleElements[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    particleElements[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC particleInputLayout{};
    particleInputLayout.pInputElementDescs = particleElements;
    particleInputLayout.NumElements = _countof(particleElements);

    // Particle rasterizer: no cull is common for billboard
    D3D12_RASTERIZER_DESC particleRaster{};
    particleRaster.CullMode = D3D12_CULL_MODE_NONE;
    particleRaster.FillMode = D3D12_FILL_MODE_SOLID;

    // Particle depth: enable but no write for transparent
    D3D12_DEPTH_STENCIL_DESC particleDepth{};
    particleDepth.DepthEnable = TRUE;
    particleDepth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    particleDepth.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // Base particle desc
    D3D12_GRAPHICS_PIPELINE_STATE_DESC particleDesc{};
    particleDesc.pRootSignature = particleRootSignature_.Get();
    particleDesc.InputLayout = particleInputLayout;
    particleDesc.VS = { particleVs_->GetBufferPointer(), particleVs_->GetBufferSize() };
    particleDesc.PS = { particlePs_->GetBufferPointer(), particlePs_->GetBufferSize() };
    particleDesc.RasterizerState = particleRaster;
    particleDesc.DepthStencilState = particleDepth;
    particleDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    particleDesc.NumRenderTargets = 1;
    particleDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    particleDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    particleDesc.SampleDesc.Count = 1;
    particleDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // Legacy: particlePso_ (equivalent to particlePipelineState)
    particleDesc.BlendState = MakeParticleAlphaBlend();
    hr = device->CreateGraphicsPipelineState(&particleDesc, IID_PPV_ARGS(&particlePso_));
    if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(Particle)", hr);

    // Opaque
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = particleDesc;
        d.BlendState = MakeParticleOpaqueBlend();
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&particleOpaquePso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(ParticleOpaque)", hr);
    }

    // Alpha
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = particleDesc;
        d.BlendState = MakeParticleAlphaBlend();
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&particleAlphaPso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(ParticleAlpha)", hr);
    }

    // ------------------------------
    // Main Opaque/Alpha variants (for sprite or UI etc)
    // These were in AppMain as psoOpaque/psoAlpha using mainRootSignature.
    // We create them as variants of main PSO by only changing BlendState.
    // ------------------------------
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = graphicsPipelineStateDesc;
        auto MakeOpaqueBlend = []() {
            D3D12_BLEND_DESC bd{};
            bd.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            return bd;
        };
        d.BlendState = MakeOpaqueBlend();
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&mainOpaquePso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(MainOpaque)", hr);
    }

    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC d = graphicsPipelineStateDesc;
        D3D12_BLEND_DESC bd{};
        bd.RenderTarget[0].BlendEnable = TRUE;
        bd.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        bd.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        bd.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        bd.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        d.BlendState = bd;
        d.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        hr = device->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&mainAlphaPso_));
        if (FAILED(hr)) return FailHr("CreateGraphicsPipelineState(MainAlpha)", hr);
    }

    return true;
}
