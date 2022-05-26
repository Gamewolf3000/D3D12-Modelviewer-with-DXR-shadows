#pragma once

#include <vector>

#include <d3d12.h>

#include "ResourceComponent.h"
#include "FrameBased.h"

enum class UpdateType
{
	NONE,
	INITIALISE_ONLY,
	MAP_UPDATE,
	COPY_UPDATE
};

template<typename SpecificData>
class ComponentData
{
protected:
	struct DataHeader
	{
		size_t startOffset = static_cast<size_t>(-1);
		unsigned int dataSize = static_cast<unsigned int>(-1);
		ResourceIndex resourceIndex = ResourceIndex(-1);
		SpecificData specifics = SpecificData();
	};

	ID3D12Device* device;
	bool updateNeeded = false;
	FrameType nrOfFrames = 0;
	std::vector<DataHeader> headers;
	std::vector<unsigned char> data;
	UpdateType type = UpdateType::NONE;
	size_t usedDataSize = 0;

	void UpdateExistingHeaders(size_t indexOfOriginalChange, 
		std::int64_t sizeDifference);

public:
	ComponentData() = default;
	~ComponentData() = default;
	ComponentData(const ComponentData& other) = delete;
	ComponentData& operator=(const ComponentData& other) = delete;
	ComponentData(ComponentData&& other) = default;
	ComponentData& operator=(ComponentData&& other) = default;

	virtual void Initialize(ID3D12Device* deviceToUse, FrameType totalNrOfFrames, 
		UpdateType componentUpdateType, unsigned int totalSize);

	virtual void RemoveComponent(ResourceIndex resourceIndex) = 0;

	void* GetComponentData(ResourceIndex resourceIndex);
};

template<typename SpecificData>
void ComponentData<SpecificData>::UpdateExistingHeaders(size_t indexOfOriginalChange,
	std::int64_t sizeDifference)
{
	size_t jointSize = 0;
	for (size_t i = indexOfOriginalChange + 1; i < headers.size(); ++i)
	{
		headers[i].startOffset = sizeDifference >= 0 ?
			headers[i].startOffset + static_cast<size_t>(sizeDifference) :
			headers[i].startOffset - static_cast<size_t>(-sizeDifference);
		jointSize += headers[i].dataSize;
	}

	unsigned char* destination = data.data();
	destination += headers[indexOfOriginalChange].startOffset;
	destination += headers[indexOfOriginalChange].dataSize;
	unsigned char* source = destination;
	destination = sizeDifference >= 0 ?
		destination + static_cast<size_t>(sizeDifference) :
		destination - static_cast<size_t>(-sizeDifference);
	std::memmove(destination, source, jointSize);
}

template<typename SpecificData>
void ComponentData<SpecificData>::Initialize(ID3D12Device* deviceToUse, 
	FrameType totalNrOfFrames, UpdateType componentUpdateType,
	unsigned int totalSize)
{
	device = deviceToUse;
	nrOfFrames = totalNrOfFrames;
	type = componentUpdateType;
	if (type != UpdateType::INITIALISE_ONLY && type != UpdateType::NONE)
	{
		data.resize(totalSize);
		usedDataSize = totalSize;
	}
}

template<typename SpecificData>
inline void* ComponentData<SpecificData>::GetComponentData(
	ResourceIndex resourceIndex)
{
	unsigned char* toReturn = nullptr;

	if (type != UpdateType::INITIALISE_ONLY && type != UpdateType::NONE
		&& resourceIndex < headers.size())
	{
		toReturn = data.data() + headers[resourceIndex].startOffset;
	}
	else
	{
		for (auto& header : headers)
		{
			if (header.resourceIndex == resourceIndex)
			{
				toReturn = data.data() + header.startOffset;
				break;
			}
		}
	}

	return toReturn;
}
