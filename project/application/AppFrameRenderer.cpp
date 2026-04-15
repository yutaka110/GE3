#include "AppFrameRenderer.h"

void AppFrameRenderer::BeginFrame(
    ID3D12GraphicsCommandList* commandList,
    ID3D12Resource* backBuffer,
    D3D12_CPU_DESCRIPTOR_HANDLE rtv,
    D3D12_CPU_DESCRIPTOR_HANDLE dsv,
    const float clearColor[4]) const {
    if (commandList == nullptr || backBuffer == nullptr || clearColor == nullptr) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = backBuffer;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    commandList->ClearDepthStencilView(
        dsv,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,
        0,
        0,
        nullptr);
}

void AppFrameRenderer::PrepareMainPass(
    ID3D12GraphicsCommandList* commandList,
    const D3D12_VIEWPORT& viewport,
    const D3D12_RECT& scissorRect,
    ID3D12RootSignature* rootSignature,
    ID3D12PipelineState* pipelineState) const {
    if (commandList == nullptr || rootSignature == nullptr || pipelineState == nullptr) {
        return;
    }

    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);
    commandList->SetGraphicsRootSignature(rootSignature);
    commandList->SetPipelineState(pipelineState);
}

void AppFrameRenderer::DrawMainModel(
    ID3D12GraphicsCommandList* commandList,
    const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView,
    D3D12_GPU_VIRTUAL_ADDRESS materialBufferAddress,
    D3D12_GPU_VIRTUAL_ADDRESS transformBufferAddress,
    D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
    D3D12_GPU_VIRTUAL_ADDRESS directionalLightBufferAddress,
    D3D12_GPU_VIRTUAL_ADDRESS cameraBufferAddress,
    uint32_t vertexCount) const {
    if (commandList == nullptr || vertexCount == 0) {
        return;
    }

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandList->SetGraphicsRootConstantBufferView(0, materialBufferAddress);
    commandList->SetGraphicsRootConstantBufferView(1, transformBufferAddress);
    commandList->SetGraphicsRootDescriptorTable(2, textureHandle);
    commandList->SetGraphicsRootConstantBufferView(3, directionalLightBufferAddress);
    commandList->SetGraphicsRootConstantBufferView(6, cameraBufferAddress);
    commandList->DrawInstanced(vertexCount, 1, 0, 0);
}

void AppFrameRenderer::DrawSprite(
    ID3D12GraphicsCommandList* commandList,
    ID3D12DescriptorHeap* descriptorHeap,
    const D3D12_INDEX_BUFFER_VIEW& indexBufferView,
    const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView,
    D3D12_GPU_VIRTUAL_ADDRESS materialBufferAddress,
    D3D12_GPU_VIRTUAL_ADDRESS transformBufferAddress,
    D3D12_GPU_VIRTUAL_ADDRESS directionalLightBufferAddress,
    D3D12_GPU_VIRTUAL_ADDRESS cameraBufferAddress,
    D3D12_GPU_VIRTUAL_ADDRESS pointLightBufferAddress,
    D3D12_GPU_VIRTUAL_ADDRESS spotLightBufferAddress) const {
    if (commandList == nullptr || descriptorHeap == nullptr) {
        return;
    }

    commandList->IASetIndexBuffer(&indexBufferView);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetGraphicsRootConstantBufferView(0, materialBufferAddress);
    commandList->SetGraphicsRootConstantBufferView(1, transformBufferAddress);
    commandList->SetGraphicsRootConstantBufferView(3, directionalLightBufferAddress);
    commandList->SetGraphicsRootConstantBufferView(6, cameraBufferAddress);
    commandList->SetGraphicsRootConstantBufferView(7, pointLightBufferAddress);
    commandList->SetGraphicsRootConstantBufferView(8, spotLightBufferAddress);
    ID3D12DescriptorHeap* descriptorHeaps[] = { descriptorHeap };
    commandList->SetDescriptorHeaps(1, descriptorHeaps);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
}

void AppFrameRenderer::PrepareSphere(
    ID3D12GraphicsCommandList* commandList,
    ID3D12RootSignature* rootSignature,
    ID3D12PipelineState* pipelineState,
    D3D12_GPU_VIRTUAL_ADDRESS cameraBufferAddress,
    const D3D12_VERTEX_BUFFER_VIEW& sphereVertexBufferView) const {
    if (commandList == nullptr || rootSignature == nullptr || pipelineState == nullptr) {
        return;
    }

    commandList->SetGraphicsRootSignature(rootSignature);
    commandList->SetPipelineState(pipelineState);
    commandList->SetGraphicsRootConstantBufferView(6, cameraBufferAddress);
    commandList->IASetVertexBuffers(0, 1, &sphereVertexBufferView);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void AppFrameRenderer::DrawParticles(
    ID3D12GraphicsCommandList* commandList,
    ID3D12RootSignature* particleRootSignature,
    ID3D12PipelineState* particlePipelineState,
    ID3D12PipelineState* particleAlphaPipelineState,
    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandle,
    const D3D12_VERTEX_BUFFER_VIEW& particleVertexBufferView,
    const D3D12_INDEX_BUFFER_VIEW& indexBufferView,
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle,
    uint32_t drawCount) const {
    if (commandList == nullptr || particleRootSignature == nullptr ||
        particlePipelineState == nullptr || particleAlphaPipelineState == nullptr ||
        drawCount == 0) {
        return;
    }

    commandList->SetGraphicsRootSignature(particleRootSignature);
    commandList->SetPipelineState(particlePipelineState);
    commandList->SetPipelineState(particleAlphaPipelineState);
    commandList->SetGraphicsRootDescriptorTable(1, instancingSrvHandle);
    commandList->IASetVertexBuffers(0, 1, &particleVertexBufferView);
    commandList->IASetIndexBuffer(&indexBufferView);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandle);
    commandList->DrawIndexedInstanced(6, drawCount, 0, 0, 0);
}

void AppFrameRenderer::EndFrame(
    ID3D12GraphicsCommandList* commandList,
    ID3D12Resource* backBuffer) const {
    if (commandList == nullptr || backBuffer == nullptr) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = backBuffer;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
}
