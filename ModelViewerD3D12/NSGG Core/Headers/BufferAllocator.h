#pragma once

#include <d3d12.h>
#include <dxgi.h>
#include <vector>
#include <utility>

#include "ResourceAllocator.h"
#include "D3DPtr.h"
#include "HeapHelper.h"
#include "ResourceUploader.h"

struct BufferInfo
{
	size_t alignment = size_t(-1);
	size_t elementSize = size_t(-1);
};

struct BufferHandle
{
	ID3D12Resource* resource;
	size_t startOffset;
	size_t nrOfElements;
};

class BufferAllocator : public ResourceAllocator
{
private:

	struct BufferEntry
	{
		size_t nrOfElements = 0;
	};

	D3DPtr<ID3D12Resource> resource;
	unsigned char* mappedStart = nullptr;

	BufferInfo bufferInfo;
	HeapHelper<BufferEntry> buffers;
	D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;

	ID3D12Resource* AllocateResource(size_t size, ID3D12Device* device);

public:
	BufferAllocator() = default;
	virtual ~BufferAllocator() = default;
	BufferAllocator(const BufferAllocator& other) = delete;
	BufferAllocator& operator=(const BufferAllocator& other) = delete;
	BufferAllocator(BufferAllocator&& other) noexcept;
	BufferAllocator& operator=(BufferAllocator&& other) noexcept;

	void Initialize(const BufferInfo& bufferInfoToUse, ID3D12Device* device,
		bool mappedUpdateable, const AllowedViews& allowedViews,
		ID3D12Heap* heap, size_t startOffset, size_t endOffset);
	void Initialize(const BufferInfo& bufferInfoToUse, ID3D12Device* device,
		bool mappedUpdateable, const AllowedViews& allowedViews, size_t heapSize);

	size_t AllocateBuffer(size_t nrOfElements);
	void DeallocateBuffer(size_t index);

	D3D12_RESOURCE_BARRIER CreateTransitionBarrier(D3D12_RESOURCE_STATES newState,
		D3D12_RESOURCE_BARRIER_FLAGS flag = D3D12_RESOURCE_BARRIER_FLAG_NONE);

	unsigned char* GetMappedPtr();
	BufferHandle GetHandle(size_t index);
	size_t GetElementSize();
	size_t GetElementAlignment();
	D3D12_RESOURCE_STATES GetCurrentState();

	void UpdateMappedBuffer(size_t index, void* data); // Map/Unmap method
	//size_t DefragResources(ID3D12GraphicsCommandList* list);
};