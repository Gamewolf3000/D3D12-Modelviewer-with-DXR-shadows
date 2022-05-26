#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>

#include "StableVector.h"

class DescriptorAllocator
{
private:
	struct DescriptorHeapData
	{
		bool heapOwned = false;
		ID3D12DescriptorHeap* heap = nullptr;
		D3D12_DESCRIPTOR_HEAP_TYPE descriptorType = 
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		size_t descriptorSize = 0;
		size_t startIndex = size_t(-1);
		size_t endIndex = size_t(-1);
	} heapData;

	struct StoredDescriptor
	{
		// Empty
	};

	ID3D12Device* device = nullptr;
	StableVector<StoredDescriptor> descriptors;

	ID3D12DescriptorHeap* AllocateHeap(size_t nrOfDescriptors);
	size_t GetFreeDescriptorIndex(size_t indexInHeap);
	bool AllocationHelper(size_t& index, D3D12_CPU_DESCRIPTOR_HANDLE& handle,
		size_t indexInHeap);

public:
	DescriptorAllocator() = default;
	~DescriptorAllocator();
	DescriptorAllocator(const DescriptorAllocator& other) = delete;
	DescriptorAllocator& operator=(const DescriptorAllocator& other) = delete;
	DescriptorAllocator(DescriptorAllocator&& other) noexcept;
	DescriptorAllocator& operator=(DescriptorAllocator&& other) noexcept;

	void Initialize(D3D12_DESCRIPTOR_HEAP_TYPE descriptorType, ID3D12Device* deviceToUse,
		ID3D12DescriptorHeap* heap, size_t startIndex, size_t nrOfDescriptors);
	void Initialize(D3D12_DESCRIPTOR_HEAP_TYPE descriptorType,
		ID3D12Device* deviceToUse, size_t nrOfDescriptors);

	size_t AllocateSRV(ID3D12Resource* resource,
		D3D12_SHADER_RESOURCE_VIEW_DESC* desc = nullptr, 
		size_t indexInHeap = size_t(-1));
	size_t AllocateDSV(ID3D12Resource* resource,
		D3D12_DEPTH_STENCIL_VIEW_DESC* desc = nullptr,
		size_t indexInHeap = size_t(-1));
	size_t AllocateRTV(ID3D12Resource* resource,
		D3D12_RENDER_TARGET_VIEW_DESC* desc = nullptr,
		size_t indexInHeap = size_t(-1));
	size_t AllocateUAV(ID3D12Resource* resource,
		D3D12_UNORDERED_ACCESS_VIEW_DESC* desc = nullptr,
		ID3D12Resource* counterResource = nullptr, 
		size_t indexInHeap = size_t(-1));
	size_t AllocateCBV(D3D12_CONSTANT_BUFFER_VIEW_DESC* desc = nullptr,
		size_t indexInHeap = size_t(-1));

	void DeallocateDescriptor(size_t index);

	const D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle(size_t index) const;

	size_t NrOfStoredDescriptors() const;
};