#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>

#include "DescriptorAllocator.h"
#include "ResourceUploader.h"

enum class HeapType
{
	EXTERNAL,
	OWNED
};

struct ResourceHeapInfo
{
	HeapType heapType;

	union
	{
		struct ExternalInfo
		{
			ID3D12Heap* heap;
			size_t startOffset;
			size_t endOffset;
		} external;

		struct OwnedInfo
		{
			size_t heapSize;
		} owned;

	} info;

	ResourceHeapInfo(size_t heapSizeInBytes)
	{
		heapType = HeapType::OWNED;
		info.owned.heapSize = heapSizeInBytes;
	}

	ResourceHeapInfo(ID3D12Heap* heap, size_t endOffset,
		size_t startOffset = 0)
	{
		heapType = HeapType::EXTERNAL;
		info.external.heap = heap;
		info.external.startOffset = startOffset;
		info.external.endOffset = endOffset;
	}
};

enum class ViewType
{
	CBV = 0,
	SRV = 1,
	UAV = 2,
	RTV = 3,
	DSV = 4,
};

template<typename ViewDesc>
struct DescriptorAllocationInfo
{
	ViewType viewType;
	ViewDesc viewDesc;
	HeapType heapType;

	union
	{
		struct ExternalInfo
		{
			ID3D12DescriptorHeap* heap;
			size_t startIndex;
			size_t nrOfDescriptors;
		} external;

		struct OwnedInfo
		{
			size_t nrOfDescriptors;
		} owned;
	} descriptorHeapInfo;

	DescriptorAllocationInfo(ViewType type, const ViewDesc& viewDesc,
		size_t nrOfDescriptors) : viewType(type), viewDesc(viewDesc)
	{
		heapType = HeapType::OWNED;
		descriptorHeapInfo.owned.nrOfDescriptors = nrOfDescriptors;
	}

	DescriptorAllocationInfo(ViewType type, ID3D12DescriptorHeap* heap,
		size_t nrOfDescriptors, size_t startDescriptorIndex = 0) : viewType(type)
	{
		heapType = HeapType::EXTERNAL;
		descriptorHeapInfo.external.heap = heap;
		descriptorHeapInfo.external.startIndex = startDescriptorIndex;
		descriptorHeapInfo.external.nrOfDescriptors = nrOfDescriptors;
	}
};

typedef size_t ResourceIndex;

class ResourceComponent
{
protected:
	std::vector<DescriptorAllocator> descriptorAllocators;

public:
	ResourceComponent() = default;
	virtual ~ResourceComponent() = default;
	ResourceComponent(const ResourceComponent& other) = delete;
	ResourceComponent& operator=(const ResourceComponent& other) = delete;
	ResourceComponent(ResourceComponent&& other) noexcept;
	ResourceComponent& operator=(ResourceComponent&& other) noexcept;

	virtual void RemoveComponent(ResourceIndex indexToRemove) = 0;

	virtual const D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHeapCBV(
		ResourceIndex indexOffset = 0) const;
	virtual const D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHeapSRV(
		ResourceIndex indexOffset = 0) const;
	virtual const D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHeapUAV(
		ResourceIndex indexOffset = 0) const;
	virtual const D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHeapRTV(
		ResourceIndex indexOffset = 0) const;
	virtual const D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHeapDSV(
		ResourceIndex indexOffset = 0) const;

	virtual bool HasDescriptorsOfType(ViewType type) const = 0;

	virtual size_t NrOfDescriptors() const;
};