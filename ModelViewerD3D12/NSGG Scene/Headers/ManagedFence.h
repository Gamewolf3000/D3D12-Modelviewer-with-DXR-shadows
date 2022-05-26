#pragma once

#include <d3d12.h>

class ManagedFence
{
private:
	ID3D12Fence* fence = nullptr;
	size_t currentValue = 0;
	HANDLE fenceHandle = nullptr;

public:
	ManagedFence() = default;
	~ManagedFence();

	void Initialize(ID3D12Device* device, size_t initialValue = 0);
	void Signal(ID3D12CommandQueue* queue);
	void WaitGPU(ID3D12CommandQueue* queue);
	void WaitCPU();

	bool Completed();
};