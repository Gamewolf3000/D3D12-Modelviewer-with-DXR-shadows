#pragma once

#include <d3d12.h>
#include <optional>

#include "FrameResourceComponent.h"
#include "BufferComponent.h"
#include "BufferComponentData.h"

struct BufferCreationOperation
{
	size_t nrOfElements;
	BufferReplacementViews replacementViews;
};

template<short Frames>
class FrameBufferComponent : public 
	FrameResourceComponent<BufferComponent, Frames, BufferCreationOperation>
{
private:

	typedef typename FrameResourceComponent<BufferComponent, Frames,
		BufferCreationOperation>::LifetimeOperationType BufferLifetimeOperationType;

	size_t bufferSize = 0;
	size_t bufferAlignment = 0;
	BufferComponentData componentData;

	void HandleStoredOperations() override;

public:
	FrameBufferComponent() = default;
	virtual ~FrameBufferComponent() = default;
	FrameBufferComponent(const FrameBufferComponent& other) = delete;
	FrameBufferComponent& operator=(const FrameBufferComponent& other) = delete;
	FrameBufferComponent(FrameBufferComponent&& other) noexcept;
	FrameBufferComponent& operator=(FrameBufferComponent&& other) noexcept;

	void Initialize(ID3D12Device* deviceToUse, UpdateType componentUpdateType,
		const BufferComponentInfo& bufferInfo,
		const std::vector<DescriptorAllocationInfo<BufferViewDesc>>& descriptorInfo);

	ResourceIndex CreateBuffer(size_t nrOfElements,
		const BufferReplacementViews& replacementViews = BufferReplacementViews());

	void RemoveComponent(ResourceIndex indexToRemove) override;

	void SetUpdateData(ResourceIndex resourceIndex, void* dataAdress);
	void PrepareResourcesForUpdates(std::vector<D3D12_RESOURCE_BARRIER>& barriers);
	void PerformUpdates(ID3D12GraphicsCommandList* commandList,
		ResourceUploader& uploader);

	D3D12_RESOURCE_STATES GetCurrentState();
	void ChangeToState(std::vector<D3D12_RESOURCE_BARRIER>& barriers,
		D3D12_RESOURCE_STATES newState);

	D3D12_GPU_VIRTUAL_ADDRESS GetVirtualAdress(ResourceIndex index);
	BufferHandle GetBufferHandle(ResourceIndex index);
};

template<short Frames>
inline void FrameBufferComponent<Frames>::HandleStoredOperations()
{
	size_t nrToErase = 0;
	for (auto& operation : this->storedLifetimeOperations)
	{
		if (operation.type == BufferLifetimeOperationType::CREATION)
		{
			auto& creationData = operation.creation;
			this->resourceComponents[this->activeFrame].CreateBuffer(
				creationData.nrOfElements, creationData.replacementViews);
		}
		else
		{
			this->resourceComponents[this->activeFrame].RemoveComponent(
				operation.removal.indexToRemove);
		}

		--operation.framesLeft;
		nrToErase += operation.framesLeft == 0 ? 1 : 0;
	}

	this->storedLifetimeOperations.erase(this->storedLifetimeOperations.begin(),
		this->storedLifetimeOperations.begin() + nrToErase);
}

template<short Frames>
inline FrameBufferComponent<Frames>::FrameBufferComponent(
	FrameBufferComponent&& other) noexcept : FrameResourceComponent<
	BufferComponent, Frames, BufferCreationOperation>(std::move(other)),
	bufferSize(other.bufferSize), bufferAlignment(other.bufferAlignment),
	componentData(std::move(other.componentData))
{
	other.bufferSize = 0;
	other.bufferAlignment = 0;
}

template<short Frames>
inline FrameBufferComponent<Frames>& FrameBufferComponent<Frames>::operator=(
	FrameBufferComponent&& other) noexcept
{
	if (this != &other)
	{
		FrameResourceComponent<BufferComponent,
			Frames, BufferCreationOperation>::operator=(std::move(other));
		bufferSize = other.bufferSize;
		bufferAlignment = other.bufferAlignment;
		componentData = std::move(other.componentData);

		other.bufferSize = 0;
		other.bufferAlignment = 0;
	}

	return *this;
}

template<short Frames>
inline void FrameBufferComponent<Frames>::Initialize(ID3D12Device* deviceToUse,
	UpdateType componentUpdateType, const BufferComponentInfo& bufferInfo,
	const std::vector<DescriptorAllocationInfo<BufferViewDesc>>& descriptorInfo)
{
	FrameResourceComponent<BufferComponent, Frames,
		BufferCreationOperation>::Initialize(deviceToUse, bufferInfo, descriptorInfo);
	bufferSize = bufferInfo.bufferInfo.elementSize;
	bufferAlignment = bufferInfo.bufferInfo.alignment;
	if (componentUpdateType != UpdateType::INITIALISE_ONLY &&
		componentUpdateType != UpdateType::NONE)
	{
		unsigned int totalSize = 0;
		if (bufferInfo.heapInfo.heapType == HeapType::EXTERNAL)
		{
			totalSize = static_cast<unsigned int>(
				bufferInfo.heapInfo.info.external.endOffset
				- bufferInfo.heapInfo.info.external.startOffset);
		}
		else
		{
			totalSize = static_cast<unsigned int>(
				bufferInfo.heapInfo.info.owned.heapSize);
		}

		this->componentData.Initialize(deviceToUse, Frames, 
			componentUpdateType, totalSize);
	}
	else
	{
		this->componentData.Initialize(deviceToUse, Frames,
			componentUpdateType, 0);
	}
}

template<short Frames>
inline ResourceIndex FrameBufferComponent<Frames>::CreateBuffer(
	size_t nrOfElements, const BufferReplacementViews& replacementViews)
{
	ResourceIndex toReturn = 
		this->resourceComponents[this->activeFrame].CreateBuffer(
		nrOfElements, replacementViews);

	if (toReturn == ResourceIndex(-1))
		return ResourceIndex(-1);

	if constexpr (Frames != 1)
	{
		typename FrameResourceComponent<BufferComponent, Frames,
			BufferCreationOperation>::StoredLifetimeOperation lifetimeOperation;
		lifetimeOperation.type = BufferLifetimeOperationType::CREATION;
		lifetimeOperation.framesLeft = Frames - 1;
		lifetimeOperation.creation = { nrOfElements, replacementViews };
		this->storedLifetimeOperations.push_back(lifetimeOperation);
	}

	auto handle =
		this->resourceComponents[this->activeFrame].GetBufferHandle(toReturn);
	this->componentData.AddComponent(toReturn, handle.startOffset,
		static_cast<unsigned int>(nrOfElements * bufferSize));

	return toReturn;
}

template<short Frames>
inline void FrameBufferComponent<Frames>::RemoveComponent(
	ResourceIndex indexToRemove)
{
	FrameResourceComponent<BufferComponent, Frames,
		BufferCreationOperation>::RemoveComponent(indexToRemove);
	componentData.RemoveComponent(indexToRemove);
}

template<short Frames>
inline void FrameBufferComponent<Frames>::SetUpdateData(
	ResourceIndex resourceIndex, void* dataAdress)
{
	this->componentData.UpdateComponentData(resourceIndex, dataAdress);
}

template<short Frames>
inline void FrameBufferComponent<Frames>::PrepareResourcesForUpdates(
	std::vector<D3D12_RESOURCE_BARRIER>& barriers)
{
	this->componentData.PrepareUpdates(barriers,
		this->resourceComponents[this->activeFrame]);
}

template<short Frames>
inline void FrameBufferComponent<Frames>::PerformUpdates(
	ID3D12GraphicsCommandList* commandList, ResourceUploader& uploader)
{
	this->componentData.UpdateComponentResources(commandList, uploader,
		this->resourceComponents[this->activeFrame], bufferAlignment);
}

template<short Frames>
inline D3D12_RESOURCE_STATES FrameBufferComponent<Frames>::GetCurrentState()
{
	return this->resourceComponents[this->activeFrame].GetCurrentState();
}

template<short Frames>
inline void FrameBufferComponent<Frames>::ChangeToState(
	std::vector<D3D12_RESOURCE_BARRIER>& barriers, D3D12_RESOURCE_STATES newState)
{
	if (newState != this->resourceComponents[this->activeFrame].GetCurrentState())
	{
		barriers.push_back(
			this->resourceComponents[this->activeFrame].CreateTransitionBarrier(
				newState));
	}
}

template<short Frames>
inline D3D12_GPU_VIRTUAL_ADDRESS 
FrameBufferComponent<Frames>::GetVirtualAdress(ResourceIndex index)
{
	auto handle = 
		this->resourceComponents[this->activeFrame].GetBufferHandle(index);
	D3D12_GPU_VIRTUAL_ADDRESS toReturn = 
		handle.resource->GetGPUVirtualAddress();
	toReturn += handle.startOffset;

	return toReturn;
}

template<short Frames>
inline BufferHandle FrameBufferComponent<Frames>::GetBufferHandle(ResourceIndex index)
{
	return this->resourceComponents[this->activeFrame].GetBufferHandle(index);
}