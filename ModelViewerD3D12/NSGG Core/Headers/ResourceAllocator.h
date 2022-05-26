#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>

struct AllowedViews
{
	bool srv = true;
	bool uav = false;
	bool rtv = false;
	bool dsv = false;
};

class ResourceAllocator
{
protected:

	struct ResourceHeapData
	{
		bool heapOwned = false;
		ID3D12Heap* heap = nullptr;
		size_t startOffset = size_t(-1);
		size_t endOffset = size_t(-1);
	} heapData;

	D3D12_RESOURCE_FLAGS CreateBindFlag();
	ID3D12Heap* AllocateHeap(size_t size, bool uploadHeap, D3D12_HEAP_FLAGS flags,
		ID3D12Device* device);
	ID3D12Resource* AllocateResource(const D3D12_RESOURCE_DESC& desc, 
		D3D12_RESOURCE_STATES initialState, const D3D12_CLEAR_VALUE* clearValue,
		size_t heapOffset, ID3D12Device* device);

	AllowedViews views;

public:
	ResourceAllocator() = default;
	virtual ~ResourceAllocator();
	ResourceAllocator(const ResourceAllocator& other) = default;
	ResourceAllocator& operator=(const ResourceAllocator& other) = default;
	ResourceAllocator(ResourceAllocator&& other) noexcept;
	ResourceAllocator& operator=(ResourceAllocator&& other) noexcept;

	void Initialize(const AllowedViews& allowedViews);
};