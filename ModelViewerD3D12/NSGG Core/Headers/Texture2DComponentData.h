#pragma once

#include <vector>
#include <cstdint>

#include <d3d12.h>

#include "ComponentData.h"
#include "Texture2DComponent.h"
#include "ResourceUploader.h"
#include "FrameBased.h"

struct Texture2DSpecific
{
	size_t startSubresource;
	std::uint8_t nrOfSubresources;
	bool needUpdating;
};

class Texture2DComponentData : public ComponentData<Texture2DSpecific>
{
private:
	struct SubresourceHeader
	{
		FrameType framesLeft;
		size_t startOffset;
		unsigned int width;
		unsigned int height;
	};

	std::vector<SubresourceHeader> subresourceHeaders;

	void UpdateExistingSubresourceHeaders(size_t indexOfOriginalChange,
		std::int16_t entryDifference);

	void SetSubresourceHeaders(size_t firstIndex, std::uint8_t nrOfSubresources,
		ID3D12Resource* resource);

	std::uint8_t CalculateSubresourceCount(ID3D12Resource* resource);

public:
	Texture2DComponentData() = default;
	~Texture2DComponentData() = default;
	Texture2DComponentData(const Texture2DComponentData& other) = delete;
	Texture2DComponentData& operator=(const Texture2DComponentData& other) = delete;
	Texture2DComponentData(Texture2DComponentData&& other) = default;
	Texture2DComponentData& operator=(Texture2DComponentData&& other) = default;

	virtual void Initialize(ID3D12Device* deviceToUse, FrameType totalNrOfFrames,
		UpdateType componentUpdateType, unsigned int totalSize) override;

	void AddComponent(ResourceIndex resourceIndex, unsigned int dataSize,
		ID3D12Resource* resource);
	void RemoveComponent(ResourceIndex resourceIndex) override;
	void UpdateComponentData(ResourceIndex resourceIndex, void* dataPtr,
		std::uint8_t texelSizeInBytes, std::uint8_t subresource = 0);

	void PrepareUpdates(std::vector<D3D12_RESOURCE_BARRIER>& barriers,
		Texture2DComponent& componentToUpdate);
	void UpdateComponentResources(ID3D12GraphicsCommandList* commandList,
		ResourceUploader& uploader, Texture2DComponent& componentToUpdate,
		std::uint8_t texelSize, DXGI_FORMAT textureFormat);
};