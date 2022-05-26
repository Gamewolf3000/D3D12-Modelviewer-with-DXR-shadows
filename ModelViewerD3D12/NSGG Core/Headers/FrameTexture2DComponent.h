#pragma once

#include <d3d12.h>
#include <optional>

#include "FrameResourceComponent.h"
#include "Texture2DComponent.h"
#include "Texture2DComponentData.h"

struct Texture2DCreationOperation
{
	TextureAllocationInfo allocationInfo;
	Texture2DComponentTemplate::TextureReplacementViews replacementViews;
};

template<FrameType Frames>
class FrameTexture2DComponent : public FrameResourceComponent<
	Texture2DComponent, Frames, Texture2DCreationOperation>
{
private:
	ID3D12Device* device = nullptr;

	typedef typename FrameResourceComponent<Texture2DComponent, Frames,
		Texture2DCreationOperation>::LifetimeOperationType Texture2DLifetimeOperationType;

	std::uint8_t texelSize = 0;
	DXGI_FORMAT textureFormat = DXGI_FORMAT_UNKNOWN;
	Texture2DComponentData componentData;

	void HandleStoredOperations() override;

public:
	FrameTexture2DComponent() = default;
	FrameTexture2DComponent(const FrameTexture2DComponent& other) = delete;
	FrameTexture2DComponent& operator=(const FrameTexture2DComponent& other) = delete;
	FrameTexture2DComponent(FrameTexture2DComponent&& other) noexcept;
	FrameTexture2DComponent& operator=(FrameTexture2DComponent&& other) noexcept;

	void Initialize(ID3D12Device* deviceToUse, UpdateType componentUpdateType,
		const TextureComponentInfo& textureInfo,
		const std::vector<DescriptorAllocationInfo<Texture2DViewDesc>>&
		descriptorInfo);

	ResourceIndex CreateTexture(const TextureAllocationInfo& allocationInfo,
		const Texture2DComponentTemplate::TextureReplacementViews&
		replacementViews = {});

	void RemoveComponent(ResourceIndex indexToRemove) override;

	void SetUpdateData(ResourceIndex resourceIndex, void* dataAdress,
		std::uint8_t subresource);
	void PrepareResourcesForUpdates(std::vector<D3D12_RESOURCE_BARRIER>& barriers);
	void PerformUpdates(ID3D12GraphicsCommandList* commandList,
		ResourceUploader& uploader);

	D3D12_RESOURCE_STATES GetCurrentState(ResourceIndex resourceIndex);
	void ChangeToState(ResourceIndex resourceIndex,
		std::vector<D3D12_RESOURCE_BARRIER>& barriers,
		D3D12_RESOURCE_STATES newState);

	TextureHandle GetTextureHandle(ResourceIndex index);
};

template<FrameType Frames>
inline void FrameTexture2DComponent<Frames>::HandleStoredOperations()
{
	size_t nrToErase = 0;
	for (auto& operation : this->storedLifetimeOperations)
	{
		if (operation.type == Texture2DLifetimeOperationType::CREATION)
		{
			this->resourceComponents[this->activeFrame].CreateTexture(
				operation.creation.allocationInfo,
				operation.creation.replacementViews);
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

template<FrameType Frames>
inline FrameTexture2DComponent<Frames>::FrameTexture2DComponent(
	FrameTexture2DComponent&& other) noexcept : FrameResourceComponent<
	Texture2DComponent, Frames, Texture2DCreationOperation>(std::move(other)),
	device(other.device), texelSize(other.texelSize), 
	textureFormat(other.textureFormat), componentData(std::move(other.componentData))
{
	other.device = nullptr;
	other.texelSize = 0;
	other.textureFormat = DXGI_FORMAT_UNKNOWN;
}

template<FrameType Frames>
inline FrameTexture2DComponent<Frames>& 
FrameTexture2DComponent<Frames>::operator=(FrameTexture2DComponent&& other) noexcept
{
	if (this != &other)
	{
		FrameResourceComponent<Texture2DComponent,
			Frames, Texture2DCreationOperation>::operator=(std::move(other));
		device = other.device;
		texelSize = other.texelSize;
		textureFormat = other.textureFormat;
		componentData = std::move(other.componentData);

		other.device = nullptr;
		other.texelSize = 0;
		other.textureFormat = DXGI_FORMAT_UNKNOWN;
	}

	return *this;
}

template<FrameType Frames>
inline void FrameTexture2DComponent<Frames>::Initialize(ID3D12Device* deviceToUse,
	UpdateType componentUpdateType, const TextureComponentInfo& textureInfo,
	const std::vector<DescriptorAllocationInfo<Texture2DViewDesc>>&
	descriptorInfo)
{
	FrameResourceComponent<Texture2DComponent, Frames,
		Texture2DCreationOperation>::Initialize(deviceToUse, textureInfo,
			descriptorInfo);
	device = deviceToUse;
	texelSize = textureInfo.textureInfo.texelSize;
	textureFormat = textureInfo.textureInfo.format;
	if (componentUpdateType != UpdateType::INITIALISE_ONLY &&
		componentUpdateType != UpdateType::NONE)
	{
		unsigned int totalSize = 0;
		if (textureInfo.heapInfo.heapType == HeapType::EXTERNAL)
		{
			totalSize = static_cast<unsigned int>(
				textureInfo.heapInfo.info.external.endOffset
				- textureInfo.heapInfo.info.external.startOffset);
		}
		else
		{
			totalSize = static_cast<unsigned int>(
				textureInfo.heapInfo.info.owned.heapSize);
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

template<FrameType Frames>
inline ResourceIndex FrameTexture2DComponent<Frames>::CreateTexture(
	const TextureAllocationInfo& allocationInfo,
	const TextureComponent<Texture2DShaderResourceDesc,
	Texture2DUnorderedAccessDesc, Texture2DRenderTargetDesc,
	Texture2DDepthStencilDesc>::TextureReplacementViews& replacementViews)
{
	ResourceIndex toReturn =
		this->resourceComponents[this->activeFrame].CreateTexture(
			allocationInfo, replacementViews);

	if (toReturn == ResourceIndex(-1))
		return ResourceIndex(-1);

	if constexpr (Frames != 1)
	{
		typename FrameResourceComponent<Texture2DComponent, Frames,
			Texture2DCreationOperation>::StoredLifetimeOperation lifetimeOperation;
		lifetimeOperation.type = Texture2DLifetimeOperationType::CREATION;
		lifetimeOperation.framesLeft = Frames - 1;
		lifetimeOperation.creation = { allocationInfo, replacementViews };
		this->storedLifetimeOperations.push_back(lifetimeOperation);
	}

	TextureHandle handle = 
		this->resourceComponents[this->activeFrame].GetTextureHandle(toReturn);
	auto desc = handle.resource->GetDesc();
	unsigned int totalSize = 0;

	D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = { desc.Format, 0 };
	std::uint8_t planeCount = 1;
	HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO,
		&formatInfo, sizeof(formatInfo));
	if (SUCCEEDED(hr))
		planeCount = formatInfo.PlaneCount;

	UINT nrOfSubresources = planeCount * desc.DepthOrArraySize * desc.MipLevels;
	std::vector<UINT> rows(nrOfSubresources);
	std::vector<UINT64> rowSizes(nrOfSubresources);

	device->GetCopyableFootprints(&desc, 0, nrOfSubresources, 0, nullptr,
		rows.data(), rowSizes.data(), nullptr);

	for (unsigned int subresource = 0; subresource < nrOfSubresources; ++subresource)
	{
		totalSize += static_cast<unsigned int>(rows[subresource] *
			rowSizes[subresource]);
	}

	this->componentData.AddComponent(toReturn, totalSize, handle.resource);

	return toReturn;
}

template<FrameType Frames>
inline void FrameTexture2DComponent<Frames>::RemoveComponent(
	ResourceIndex indexToRemove)
{
	FrameResourceComponent<Texture2DComponent, Frames,
		Texture2DCreationOperation>::RemoveComponent(indexToRemove);
	componentData.RemoveComponent(indexToRemove);
}

template<FrameType Frames>
inline void FrameTexture2DComponent<Frames>::SetUpdateData(
	ResourceIndex resourceIndex, void* dataAdress, std::uint8_t subresource)
{
	this->componentData.UpdateComponentData(resourceIndex, dataAdress,
		texelSize, subresource);
}

template<FrameType Frames>
inline void FrameTexture2DComponent<Frames>::PrepareResourcesForUpdates(
	std::vector<D3D12_RESOURCE_BARRIER>& barriers)
{
	this->componentData.PrepareUpdates(barriers,
		this->resourceComponents[this->activeFrame]);
}

template<FrameType Frames>
inline void FrameTexture2DComponent<Frames>::PerformUpdates(
	ID3D12GraphicsCommandList* commandList, ResourceUploader& uploader)
{
	this->componentData.UpdateComponentResources(commandList, uploader,
		this->resourceComponents[this->activeFrame], texelSize, textureFormat);
}

template<FrameType Frames>
inline D3D12_RESOURCE_STATES FrameTexture2DComponent<Frames>::GetCurrentState(
	ResourceIndex resourceIndex)
{
	return this->resourceComponents[this->activeFrame].GetCurrentState(
		resourceIndex);
}

template<FrameType Frames>
inline void FrameTexture2DComponent<Frames>::ChangeToState(
	ResourceIndex resourceIndex, std::vector<D3D12_RESOURCE_BARRIER>& barriers,
	D3D12_RESOURCE_STATES newState)
{
	auto currentState =
		this->resourceComponents[this->activeFrame].GetCurrentState(resourceIndex);
	if (newState != currentState)
	{
		barriers.push_back(
			this->resourceComponents[this->activeFrame].CreateTransitionBarrier(
				resourceIndex, newState));
	}
}

template<FrameType Frames>
inline TextureHandle FrameTexture2DComponent<Frames>::GetTextureHandle(
	ResourceIndex index)
{
	return this->resourceComponents[this->activeFrame].GetTextureHandle(index);
}
