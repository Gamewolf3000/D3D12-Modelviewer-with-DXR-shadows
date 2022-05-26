#pragma once

#include "ComponentData.h"
#include "BufferComponent.h"
#include "ResourceUploader.h"

struct BufferSpecific
{
	FrameType framesLeft;
};

class BufferComponentData : public ComponentData<BufferSpecific>
{
private:
	void HandleInitializeOnlyUpdate(ID3D12GraphicsCommandList* commandList,
		ResourceUploader& uploader, BufferComponent& componentToUpdate,
		size_t componentAlignment);
	void HandleCopyUpdate(ID3D12GraphicsCommandList* commandList,
		ResourceUploader& uploader, BufferComponent& componentToUpdate,
		size_t componentAlignment);
	void HandleMapUpdate(BufferComponent& componentToUpdate);

public:
	BufferComponentData() = default;
	~BufferComponentData() = default;
	BufferComponentData(const BufferComponentData& other) = delete;
	BufferComponentData& operator=(const BufferComponentData& other) = delete;
	BufferComponentData(BufferComponentData&& other) = default;
	BufferComponentData& operator=(BufferComponentData&& other) = default;

	void AddComponent(ResourceIndex resourceIndex, unsigned int dataSize);
	void AddComponent(ResourceIndex resourceIndex, size_t startOffset,
		unsigned int dataSize, void* initialData = nullptr);
	void RemoveComponent(ResourceIndex resourceIndex) override;
	void UpdateComponentData(ResourceIndex resourceIndex, void* dataPtr);

	void PrepareUpdates(std::vector<D3D12_RESOURCE_BARRIER>& barriers,
		BufferComponent& componentToUpdate);
	void UpdateComponentResources(ID3D12GraphicsCommandList* commandList,
		ResourceUploader& uploader, BufferComponent& componentToUpdate,
		size_t componentAlignment);
};