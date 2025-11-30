//#include "renderer/DebugSceneRenderer.h"
//
//#include <cassert>
//#include <cmath>
//
//using Microsoft::WRL::ComPtr;
//
//bool DebugSceneRenderer::Initialize(
//    ID3D12Device* device,
//    ID3D12DescriptorHeap* srvHeap,
//    UINT srvDescriptorSize,
//    IDxcUtils* dxcUtils,
//    IDxcCompiler* dxcCompiler,
//    IDxcIncludeHandler* includeHandler
//) {
//    device_ = device;
//    srvHeap_ = srvHeap;
//    srvDescSize_ = srvDescriptorSize;
//
//    // main.cpp 側のクライアントサイズに合わせるなら外から渡すのがベストだが、
//    // ここでは一旦固定値 or 後で Setter を用意してもOK。
//    clientWidth_ = 1280;
//    clientHeight_ = 720;
//
//    assert(device_);
//    assert(srvHeap_);
//
//    if (!CreateRootSignatureAndPSO(dxcUtils, dxcCompiler, includeHandler)) {
//        return false;
//    }
//    if (!CreateSpriteGeometry()) {
//        return false;
//    }
//    if (!CreateConstantBuffers()) {
//        return false;
//    }
//    if (!LoadTextureUVChecker()) {
//        return false;
//    }
//
//    // カメラ初期化（main.cpp の DebugCamera セットアップと同等に）
//    debugCamera_.Initialize({ 0.0f, 0.0f, -5.0f }, { 0.0f, 0.0f, 0.0f });
//
//    // ワールド行列の初期値
//    worldTransform_.scale = { 1.0f, 1.0f, 1.0f };
//    worldTransform_.rotate = { 0.0f, 0.0f, 0.0f };
//    worldTransform_.translate = { 0.0f, 0.0f, 0.0f };
//
//    uvTransformSprite_.scale = { 1.0f, 1.0f, 1.0f };
//    uvTransformSprite_.rotate = { 0.0f, 0.0f, 0.0f };
//    uvTransformSprite_.translate = { 0.0f, 0.0f, 0.0f };
//
//    // DirectionalLight 初期値（main.cpp で directionalLightData に入れていたものと同じ）
//    directionalLightInit_.color = { 1.0f, 1.0f, 1.0f, 1.0f };
//    Vector3 dir = { 0.0f, -1.0f, 0.0f };
//    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
//    dir.x /= len; dir.y /= len; dir.z /= len;
//    directionalLightInit_.direction = dir;
//    directionalLightInit_.intensity = 1.0f;
//
//    *directionalLightData_ = directionalLightInit_;
//
//    // WVP 初期値（単位行列）
//    *wvpData_ = MakeIdentity4x4();
//
//    return true;
//}
//
////----------------------------------------
//// RootSignature と PSO の作成
////----------------------------------------
//bool DebugSceneRenderer::CreateRootSignatureAndPSO(
//    IDxcUtils* dxcUtils,
//    IDxcCompiler* dxcCompiler,
//    IDxcIncludeHandler* includeHandler
//) {
//    HRESULT hr = S_OK;
//
//    // ============================
//    // ★★ ここが main.cpp からの移植ポイント ★★
//    // ============================
//    //
//    // main.cpp の
//    //
//    //   //**************************
//    //   //RootSignature
//    //   //**************************
//    //   // RootSignature作成
//    //   D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
//    //   ...
//    //   D3D12_ROOT_PARAMETER rootParameters[6] = {};
//    //   ...
//    //   descriptionRootSignature.pParameters = rootParameters;
//    //   descriptionRootSignature.NumParameters = _countof(rootParameters);
//    //
//    //   // WVP用のリソースを作る ...
//    //   ComPtr<ID3D12Resource> wvpResource = CreateBufferResource(...);
//    //   ...
//    //   D3D12SerializeRootSignature(...);
//    //   device->CreateRootSignature(..., &rootSignature);
//    //
//    // を **ほぼそのまま** ここにコピーしてください。
//    //
//    // 変更点は次の2つだけ：
//    //   1) ローカル変数 rootSignature → メンバ rootSignature_
//    //   2) ローカルの wvpResource / wvpData → メンバ wvpResource_ / wvpData_
//    //
//    // ようするに、
//    //
//    //   ComPtr<ID3D12Resource> wvpResource = CreateBufferResource(...);
//    //   Matrix4x4* wvpData = nullptr;
//    //   wvpResource->Map(..., (void**)&wvpData);
//    //
//    // となっている所を
//    //
//    //   wvpResource_ = CreateBufferResource(device_, sizeof(Matrix4x4));
//    //   wvpResource_->Map(..., reinterpret_cast<void**>(&wvpData_));
//    //
//    // に書き換える形です。
//
//    // ▼▼ サンプル形だけ書いておく（実際は main.cpp の内容に合わせて書き換えて） ▼▼
//
//    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
//    // ... staticSamplers / descriptorRange / receivedRange / motionMaskRange / rootParameters[]
//    // ... descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
//
//    // WVP 用 CBuffer 作成
//    wvpResource_ = CreateBufferResource(device_, sizeof(Matrix4x4));
//    wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
//    *wvpData_ = MakeIdentity4x4();
//
//    // シリアライズ & RootSignature 作成
//    ComPtr<ID3DBlob> signatureBlob;
//    ComPtr<ID3DBlob> errorBlob;
//    hr = D3D12SerializeRootSignature(
//        &descriptionRootSignature,
//        D3D_ROOT_SIGNATURE_VERSION_1,
//        signatureBlob.GetAddressOf(),
//        errorBlob.GetAddressOf());
//    assert(SUCCEEDED(hr));
//
//    hr = device_->CreateRootSignature(
//        0,
//        signatureBlob->GetBufferPointer(),
//        signatureBlob->GetBufferSize(),
//        IID_PPV_ARGS(rootSignature_.GetAddressOf()));
//    assert(SUCCEEDED(hr));
//
//    // ============================
//    // PSO 作成
//    // ============================
//
//    // Object3D.VS/PS.hlsl をコンパイル
//    ComPtr<IDxcBlob> vertexShaderBlob = CompileShader(
//        L"Object3D.VS.hlsl", L"vs_6_0", dxcUtils, dxcCompiler, includeHandler);
//    assert(vertexShaderBlob);
//
//    ComPtr<IDxcBlob> pixelShaderBlob = CompileShader(
//        L"Object3D.PS.hlsl", L"ps_6_0", dxcUtils, dxcCompiler, includeHandler);
//    assert(pixelShaderBlob);
//
//    // InputLayout や BlendState 等も main.cpp の
//    //   D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
//    // 以下をほぼそのまま持ってくる。違うのは
//    //   graphicsPipelineState → graphicsPipelineState_
//    // にするだけ。
//
//    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
//    // RootSignature
//    graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
//    // InputLayout = main.cpp と同じ layoutDesc
//    // graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
//    // VS / PS
//    graphicsPipelineStateDesc.VS = {
//        vertexShaderBlob->GetBufferPointer(),
//        vertexShaderBlob->GetBufferSize()
//    };
//    graphicsPipelineStateDesc.PS = {
//        pixelShaderBlob->GetBufferPointer(),
//        pixelShaderBlob->GetBufferSize()
//    };
//    // Rasterizer, Blend, DepthStencil, RTVFormats なども main.cpp と同じ
//    // ...
//
//    hr = device_->CreateGraphicsPipelineState(
//        &graphicsPipelineStateDesc,
//        IID_PPV_ARGS(graphicsPipelineState_.GetAddressOf()));
//    assert(SUCCEEDED(hr));
//
//    return true;
//}
//
////----------------------------------------
//// スプライト用 VB/IB の作成
////----------------------------------------
//bool DebugSceneRenderer::CreateSpriteGeometry() {
//    // main.cpp の「Sprite用頂点リソース作成」周りをほぼそのまま移植します。
//    //
//    // 元のコード：
//    //   ComPtr<ID3D12Resource> vertexResourceSprite =
//    //       CreateBufferResource(device, sizeof(VertexData) * 6);
//    //   ComPtr<ID3D12Resource> indexResourceSprite =
//    //       CreateBufferResource(device, sizeof(uint32_t) * 6);
//    //   D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
//    //   ...
//    //   D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
//    //   ...
//    //   vertexResourceSprite->Map(..., (void**)&vertexDataSprite);
//    //   indexResourceSprite->Map(..., (void**)&indexDataSprite);
//    //
//    // これをメンバに置き換え：
//    //
//    //   vertexResourceSprite_ = CreateBufferResource(device_, sizeof(VertexData) * 6);
//    //   indexResourceSprite_  = CreateBufferResource(device_, sizeof(uint32_t) * 6);
//    //   vertexBufferViewSprite_... = ...
//    //   indexBufferViewSprite_ ... = ...
//    //   vertexResourceSprite_->Map(...);
//    //   indexResourceSprite_->Map(...);
//
//    vertexResourceSprite_ = CreateBufferResource(device_, sizeof(VertexData) * 6);
//    indexResourceSprite_ = CreateBufferResource(device_, sizeof(uint32_t) * 6);
//
//    vertexBufferViewSprite_.BufferLocation = vertexResourceSprite_->GetGPUVirtualAddress();
//    vertexBufferViewSprite_.SizeInBytes = sizeof(VertexData) * 6;
//    vertexBufferViewSprite_.StrideInBytes = sizeof(VertexData);
//
//    indexBufferViewSprite_.BufferLocation = indexResourceSprite_->GetGPUVirtualAddress();
//    indexBufferViewSprite_.SizeInBytes = sizeof(uint32_t) * 6;
//    indexBufferViewSprite_.Format = DXGI_FORMAT_R32_UINT;
//
//    // 頂点 / インデックスの内容は main.cpp と同じでOK
//    VertexData* vtx = nullptr;
//    vertexResourceSprite_->Map(0, nullptr, reinterpret_cast<void**>(&vtx));
//
//    uint32_t* idx = nullptr;
//    indexResourceSprite_->Map(0, nullptr, reinterpret_cast<void**>(&idx));
//    idx[0] = 0; idx[1] = 1; idx[2] = 2;
//    idx[3] = 1; idx[4] = 3; idx[5] = 2;
//
//    // vtx[0..3] の position / normal / texcoord は
//    // main.cpp に書いてあるパターンをそのままここに移植してね。
//
//    return true;
//}
//
////----------------------------------------
//// CBuffer 作成（Material / WVP / Light）
////----------------------------------------
//bool DebugSceneRenderer::CreateConstantBuffers() {
//    // Material CBuffer
//    materialResourceSprite_ = CreateBufferResource(device_, sizeof(MaterialData));
//    materialResourceSprite_->Map(
//        0, nullptr, reinterpret_cast<void**>(&materialDataSprite_));
//
//    // WVP（CreateRootSignatureAndPSO で作っても OK。どちらか片方で）
//    if (!wvpResource_) {
//        wvpResource_ = CreateBufferResource(device_, sizeof(Matrix4x4));
//        wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
//        *wvpData_ = MakeIdentity4x4();
//    }
//
//    // Light CBuffer
//    directionalLightResource_ = CreateBufferResource(device_, sizeof(DirectionalLight));
//    directionalLightResource_->Map(
//        0, nullptr, reinterpret_cast<void**>(&directionalLightData_));
//
//    return true;
//}
//
////----------------------------------------
//// テクスチャ読み込み（uvChecker）
////----------------------------------------
//bool DebugSceneRenderer::LoadTextureUVChecker() {
//    // ここは main.cpp で uvChecker.png を読み込んでいたコードを
//    // そのまま移植してOK。
//    //
//    // すでに TextureHelper.cpp/h があるので、それに合わせて
//    //   - CreateTextureResourceResolution
//    //   - CreateUploadHeap
//    //   - CreateTextureSRV
//    // などを使う。
//
//    // ここでは SRV ハンドルだけ確保してメンバに保持する例だけ書いておく。
//
//    D3D12_CPU_DESCRIPTOR_HANDLE srvStart =
//        srvHeap_->GetCPUDescriptorHandleForHeapStart();
//    D3D12_GPU_DESCRIPTOR_HANDLE srvStartGPU =
//        srvHeap_->GetGPUDescriptorHandleForHeapStart();
//
//    // ひとまず「0番目」を使う例（実際は TextureManager 等で取得した index を使う）
//    textureSrvCPU_ = srvStart;
//    textureSrvGPU_ = srvStartGPU;
//
//    return true;
//}
//
////----------------------------------------
//// Update
////----------------------------------------
//void DebugSceneRenderer::Update(float deltaTime) {
//    // カメラ更新
//    debugCamera_.Update();
//
//    // ライトの正規化
//    directionalLightData_->color = directionalLightInit_.color;
//    directionalLightData_->direction = directionalLightInit_.direction;
//    directionalLightData_->intensity = directionalLightInit_.intensity;
//
//    // WVP 行列を更新（world * view * proj）
//    Matrix4x4 world =
//        MakeAffineMatrix(worldTransform_.scale, worldTransform_.rotate, worldTransform_.translate);
//    Matrix4x4 view = debugCamera_.GetViewMatrix();
//    Matrix4x4 proj = debugCamera_.GetProjectionMatrix();
//    *wvpData_ = Multiply(world, Multiply(view, proj));
//
//    // UV Transform などが必要なら materialDataSprite_ に書き込む
//}
//
////----------------------------------------
//// Render
////----------------------------------------
//void DebugSceneRenderer::Render(
//    ge3::graphics::RenderContext& ctx,
//    ID3D12GraphicsCommandList* cmdList,
//    ID3D12Resource* backBuffer,
//    UINT backBufferIndex
//) {
//    assert(cmdList);
//    assert(backBuffer);
//
//    // ここでは「BeginFrame は main 側で呼ばれている」前提。
//    // RootSignature / PSO / VBV,IBV / CBV / SRV 設定をまとめる。
//
//    // RootSignature / PSO
//    cmdList->SetGraphicsRootSignature(rootSignature_.Get());
//    cmdList->SetPipelineState(graphicsPipelineState_.Get());
//
//    // VB / IB
//    cmdList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite_);
//    cmdList->IASetIndexBuffer(&indexBufferViewSprite_);
//    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//
//    // CBV
//    cmdList->SetGraphicsRootConstantBufferView(0, materialResourceSprite_->GetGPUVirtualAddress());
//    cmdList->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());
//    cmdList->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());
//
//    // SRV ヒープ
//    ID3D12DescriptorHeap* heaps[] = { srvHeap_ };
//    cmdList->SetDescriptorHeaps(1, heaps);
//
//    // t0 用 SRV
//    cmdList->SetGraphicsRootDescriptorTable(2, textureSrvGPU_);
//
//    // 描画
//    cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
//}
