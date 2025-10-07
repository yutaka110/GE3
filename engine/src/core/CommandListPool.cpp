#include "core/CommandListPool.h"
#include "core/Device.h"
#include <cassert>

using Microsoft::WRL::ComPtr;
using namespace core;

bool CommandListPool::Initialize(Device& dev, UINT frameCount)
{
    frameCount_ = frameCount;
    allocators_.resize(frameCount_);

    auto* d3d = dev.GetDevice();

    for (UINT i = 0; i < frameCount_; ++i) {
        HRESULT hr = d3d->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocators_[i]));
        if (FAILED(hr)) return false;
    }

    // 単一の GraphicsCommandList を共用（毎フレーム Reset する）
    HRESULT hr = d3d->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocators_[0].Get(),
        nullptr, IID_PPV_ARGS(&list_));
    if (FAILED(hr)) return false;

    // 生成直後は Open 状態なので一度 Close して整える
    list_->Close();
    return true;
}

ID3D12GraphicsCommandList* CommandListPool::Begin(UINT frameIndex, ID3D12PipelineState* pso)
{
    assert(frameIndex < frameCount_);
    currentFrame_ = frameIndex;

    // アロケータは毎フレーム Reset 必須
    allocators_[currentFrame_]->Reset();
    list_->Reset(allocators_[currentFrame_].Get(), pso);
    return list_.Get();
}

void CommandListPool::EndAndExecute(Device& dev)
{
    list_->Close();
    ID3D12CommandList* lists[] = { list_.Get() };
    dev.GetCommandQueue()->ExecuteCommandLists(1, lists);
}
