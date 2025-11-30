#include "particle/BeamRenderer.h"

#include <vector>
#include <stdexcept>

using namespace DirectX;
using Microsoft::WRL::ComPtr;
using ge3::core::ShaderCompiler;

// 簡易ヘルパ
static void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr)) { throw std::runtime_error("D3D12 error"); }
}

// 頂点フォーマット
struct BeamVertex
{
    XMFLOAT3 pos;
    XMFLOAT2 uv;
};

void BeamRenderer::Initialize(
    ID3D12Device* device,
    ID3D12DescriptorHeap* srvHeap,
    UINT srvDescriptorSize,
    D3D12_CPU_DESCRIPTOR_HANDLE rampCpuHandle,
    D3D12_CPU_DESCRIPTOR_HANDLE noiseCpuHandle,
    DXGI_FORMAT rtvFormat,
    DXGI_FORMAT dsvFormat
)
{
    //==============================
    // 1) RootSignature 作成
    //==============================

    D3D12_ROOT_PARAMETER rootParams[3] = {};

    // [0] VS CBV (b0)
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParams[0].Descriptor.ShaderRegister = 0; // b0

    // [1] PS CBV (b1)
    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[1].Descriptor.ShaderRegister = 1; // b1

    // [2] SRV テーブル (t0〜t3くらいまで想定)
    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 4;   // t0～t3 まで確保（今は t0,t1 使用）
    srvRange.BaseShaderRegister = 0;   // t0
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = 0;

    rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[2].DescriptorTable.pDescriptorRanges = &srvRange;

    // Static Sampler (s0)
    D3D12_STATIC_SAMPLER_DESC staticSamp{};
    staticSamp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamp.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamp.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamp.ShaderRegister = 0; // s0
    staticSamp.RegisterSpace = 0;
    staticSamp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rsDesc{};
    rsDesc.NumParameters = _countof(rootParams);
    rsDesc.pParameters = rootParams;
    rsDesc.NumStaticSamplers = 1;
    rsDesc.pStaticSamplers = &staticSamp;
    rsDesc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> rsBlob;
    ComPtr<ID3DBlob> rsError;
    ThrowIfFailed(D3D12SerializeRootSignature(
        &rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        &rsBlob, &rsError));

    ThrowIfFailed(device->CreateRootSignature(
        0,
        rsBlob->GetBufferPointer(),
        rsBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSig_)));

    //==============================
    // 2) シェーダ読み込み / コンパイル
    //==============================

    ComPtr<IDxcBlob> vsBlob;
    ComPtr<IDxcBlob> psBlob;

    if (!shaderCompiler_.Initialize()) {
        throw std::runtime_error("ShaderCompiler initialization failed");
    }

    // 例：ShaderCompiler が DXC を使っている前提
     vsBlob = shaderCompiler_.CompileFromFile(
        L"Resources/Beam3D.VS.hlsl",
        L"VSMain",          // ★ エントリポイント
        L"vs_6_0");         // or "vs_5_1"

    psBlob = shaderCompiler_.CompileFromFile(
        L"Resources/Beam3D.PS.hlsl",
        L"PSMain",          // ★ エントリポイント
        L"ps_6_0");         // or "ps_5_1"


    //==============================
    // 3) 入力レイアウト
    //==============================
    D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        // POSITION : float3
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
          D3D12_APPEND_ALIGNED_ELEMENT,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

          // TEXCOORD : float2
          { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    //==============================
    // 4) ブレンド/ラスタ/深度ステンシル
    //==============================

    // 加算ブレンド (SrcAlpha, One)
    D3D12_BLEND_DESC blendDesc{};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    auto& rt0 = blendDesc.RenderTarget[0];
    rt0.BlendEnable = TRUE;
    rt0.SrcBlend = D3D12_BLEND_ONE;
    rt0.DestBlend = D3D12_BLEND_ONE;
    rt0.BlendOp = D3D12_BLEND_OP_ADD;
    rt0.SrcBlendAlpha = D3D12_BLEND_ONE;
    rt0.DestBlendAlpha = D3D12_BLEND_ZERO;
    rt0.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    rt0.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // ラスタライザ
    D3D12_RASTERIZER_DESC rastDesc{};
    rastDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rastDesc.CullMode = D3D12_CULL_MODE_NONE;
    rastDesc.FrontCounterClockwise = FALSE;
    rastDesc.DepthClipEnable = TRUE;

    // 深度ステンシル
    D3D12_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = FALSE;
    depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 深度は読むだけ
    depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    depthDesc.StencilEnable = FALSE;

    //==============================
    // 5) PSO 作成
    //==============================

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSig_.Get();
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    psoDesc.BlendState = blendDesc;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = rastDesc;
    psoDesc.DepthStencilState = depthDesc;
    psoDesc.InputLayout = { inputElements, _countof(inputElements) };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = rtvFormat;
    psoDesc.DSVFormat = dsvFormat;
    psoDesc.SampleDesc.Count = 1;

    ThrowIfFailed(device->CreateGraphicsPipelineState(
        &psoDesc, IID_PPV_ARGS(&pso_)));

    //==============================
    // 6) 板ポリ (VB/IB) 作成
    //==============================
    // ローカル空間で Y+ 方向に伸びる 1x1 の板
    // y: 0→1 がビームの長さ方向、x: -0.5→+0.5 が太さ方向
    BeamVertex vertices[] = {
        { XMFLOAT3(-0.5f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) }, // 0: 左下
        { XMFLOAT3(0.5f, 0.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) }, // 1: 右下
        { XMFLOAT3(0.5f, 1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }, // 2: 右上
        { XMFLOAT3(-0.5f, 1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) }, // 3: 左上
    };
    uint16_t indices[] = {
        0, 1, 2,
        0, 2, 3
    };
    indexCount_ = _countof(indices);

    // ---- VB ----
    {
        const UINT vbSize = sizeof(vertices);

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC bufDesc{};
        bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Alignment = 0;
        bufDesc.Width = vbSize;
        bufDesc.Height = 1;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.MipLevels = 1;
        bufDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufDesc.SampleDesc.Count = 1;
        bufDesc.SampleDesc.Quality = 0;
        bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        ThrowIfFailed(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&vb_)));

        void* mapped = nullptr;
        D3D12_RANGE range{};
        range.Begin = 0;
        range.End = 0;
        ThrowIfFailed(vb_->Map(0, &range, &mapped));
        memcpy(mapped, vertices, vbSize);
        vb_->Unmap(0, nullptr);

        vbView_.BufferLocation = vb_->GetGPUVirtualAddress();
        vbView_.SizeInBytes = vbSize;
        vbView_.StrideInBytes = sizeof(BeamVertex);
    }

    // ---- IB ----
    {
        const UINT ibSize = sizeof(indices);

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC bufDesc{};
        bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Alignment = 0;
        bufDesc.Width = ibSize;
        bufDesc.Height = 1;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.MipLevels = 1;
        bufDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufDesc.SampleDesc.Count = 1;
        bufDesc.SampleDesc.Quality = 0;
        bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        ThrowIfFailed(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&ib_)));

        void* mapped = nullptr;
        D3D12_RANGE range{};
        range.Begin = 0;
        range.End = 0;
        ThrowIfFailed(ib_->Map(0, &range, &mapped));
        memcpy(mapped, indices, ibSize);
        ib_->Unmap(0, nullptr);

        ibView_.BufferLocation = ib_->GetGPUVirtualAddress();
        ibView_.SizeInBytes = ibSize;
        ibView_.Format = DXGI_FORMAT_R16_UINT;
    }

    ////==============================
    //// 7) 定数バッファ (VS/PS) 作成 & Map
    ////==============================

   // ---- VS CBV ----
    {
        const UINT cbSize = (sizeof(BeamVSConstants) + 255) & ~255u; // 256バイトアライン

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC bufDesc{};
        bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Alignment = 0;
        bufDesc.Width = cbSize;
        bufDesc.Height = 1;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.MipLevels = 1;
        bufDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufDesc.SampleDesc.Count = 1;
        bufDesc.SampleDesc.Quality = 0;
        bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        ThrowIfFailed(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&vsCb_)));

        D3D12_RANGE range{};
        range.Begin = 0;
        range.End = 0;
        ThrowIfFailed(vsCb_->Map(0, &range, reinterpret_cast<void**>(&vsCbMapped_)));
    }

    // ---- PS CBV ----
    {
        const UINT cbSize = (sizeof(BeamPSConstants) + 255) & ~255u;

        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProps.CreationNodeMask = 1;
        heapProps.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC bufDesc{};
        bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Alignment = 0;
        bufDesc.Width = cbSize;
        bufDesc.Height = 1;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.MipLevels = 1;
        bufDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufDesc.SampleDesc.Count = 1;
        bufDesc.SampleDesc.Quality = 0;
        bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        ThrowIfFailed(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&psCb_)));

        D3D12_RANGE range{};
        range.Begin = 0;
        range.End = 0;
        ThrowIfFailed(psCb_->Map(0, &range, reinterpret_cast<void**>(&psCbMapped_)));
    }

    //==============================
    // 8) SRVテーブル (t0=ramp, t1=noise) を確保 & コピー
    //==============================

    // ★ ヒープの 3番・4番をビーム専用に使う（0:ImGui, 1:テクスチャ1, 2:テクスチャ2）
    const UINT baseIndex = 1;

    D3D12_CPU_DESCRIPTOR_HANDLE heapCpuStart = srvHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE heapGpuStart = srvHeap->GetGPUDescriptorHandleForHeapStart();

    // t0 用 (baseIndex)
    D3D12_CPU_DESCRIPTOR_HANDLE rampDest = heapCpuStart;
    rampDest.ptr += static_cast<SIZE_T>(srvDescriptorSize) * baseIndex;

    // t1 用 (baseIndex+1)
    D3D12_CPU_DESCRIPTOR_HANDLE noiseDest = heapCpuStart;
    noiseDest.ptr += static_cast<SIZE_T>(srvDescriptorSize) * (baseIndex + 1);

    // コピー
    device->CopyDescriptorsSimple(
        1, rampDest, rampCpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    device->CopyDescriptorsSimple(
        1, noiseDest, noiseCpuHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // GPU 側も同じインデックスに合わせる
    srvTableGpuStart_ = heapGpuStart;
    srvTableGpuStart_.ptr += static_cast<SIZE_T>(srvDescriptorSize) * baseIndex;
}

void BeamRenderer::SetTime(float t)
{
    if (psCbMapped_) {
        psCbMapped_->time = t;
    }
}

//------------------------------------------------------
// テスト用 1本描画
//------------------------------------------------------
void BeamRenderer::DrawTest(
    ID3D12GraphicsCommandList* cmdList,
    const XMMATRIX& worldViewProj,
    const XMFLOAT4& baseColor,
    float intensity
)
{
    // RootSignature / PSO
    cmdList->SetGraphicsRootSignature(rootSig_.Get());
    cmdList->SetPipelineState(pso_.Get());

    // IA 設定
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &vbView_);
    cmdList->IASetIndexBuffer(&ibView_);

    // CB の中身を書き換え
    XMStoreFloat4x4(&vsCbMapped_->worldViewProj, XMMatrixTranspose(worldViewProj));
    psCbMapped_->baseColor = baseColor;
    psCbMapped_->intensity = intensity;
    //psCbMapped_->time = 0.0f; // STEP2 で経過時間を入れる

    // CBV セット (b0, b1)
    cmdList->SetGraphicsRootConstantBufferView(
        0, vsCb_->GetGPUVirtualAddress());
    cmdList->SetGraphicsRootConstantBufferView(
        1, psCb_->GetGPUVirtualAddress());

    // SRVテーブル (t0,t1)
    cmdList->SetGraphicsRootDescriptorTable(
        2, srvTableGpuStart_);

    // 描画
    cmdList->DrawIndexedInstanced(indexCount_, 1, 0, 0, 0);
}
