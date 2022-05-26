#pragma once

#include <utility>

#include "NSGG Core\Headers\FrameBufferComponent.h"

#include "MeshResourceLoader.h"


struct AccelerationStructureBuffers
{
	ID3D12Resource* resultBuffer = nullptr;
	ID3D12Resource* scratchBuffer = nullptr;

	AccelerationStructureBuffers() = default;
	AccelerationStructureBuffers(const AccelerationStructureBuffers&) = delete;
	AccelerationStructureBuffers& operator=(const AccelerationStructureBuffers&) = delete;
	
	AccelerationStructureBuffers(AccelerationStructureBuffers&& other) noexcept : 
		resultBuffer(other.resultBuffer), scratchBuffer(other.scratchBuffer)
	{
		other.resultBuffer = nullptr;
		other.scratchBuffer = nullptr;
	}

	AccelerationStructureBuffers& operator=(AccelerationStructureBuffers&& other) noexcept
	{
		if (this != &other)
		{
			this->resultBuffer = other.resultBuffer;
			this->scratchBuffer = other.scratchBuffer;
			other.resultBuffer = nullptr;
			other.scratchBuffer = nullptr;
		}

		return *this;
	}

	~AccelerationStructureBuffers()
	{
		if (resultBuffer != nullptr)
			resultBuffer->Release();

		if (scratchBuffer != nullptr)
			scratchBuffer->Release();
	}
};

struct AccelerationStructure
{
	ID3D12Resource* instanceBuffer = nullptr;
	AccelerationStructureBuffers topLevel;
	AccelerationStructureBuffers bottomLevel;

	AccelerationStructure() = default;
	AccelerationStructure(const AccelerationStructure&) = delete;
	AccelerationStructure& operator=(const AccelerationStructure&) = delete;

	AccelerationStructure(AccelerationStructure&& other) :
		instanceBuffer(other.instanceBuffer)
	{
		other.instanceBuffer = nullptr;
		this->topLevel = std::move(other.topLevel);
		this->bottomLevel = std::move(other.bottomLevel);
	}

	AccelerationStructure& operator=(AccelerationStructure&& other)
	{
		if (this != &other)
		{
			this->instanceBuffer = other.instanceBuffer;
			other.instanceBuffer = nullptr;
			this->topLevel = std::move(other.topLevel);
			this->bottomLevel = std::move(other.bottomLevel);
		}

		return *this;
	}

	~AccelerationStructure()
	{
		if (instanceBuffer != nullptr)
			instanceBuffer->Release();
	}
};

AccelerationStructure CreateAccelerationStructure(ID3D12Device* device,
	ID3D12GraphicsCommandList* list, MeshIndex meshIndex,
	MeshResourceLoader& resourceLoader, FrameBufferComponent<1>& positionComponents,
	FrameBufferComponent<1>& indexComponents);

bool SetTopLevelTransform(AccelerationStructure& structure, 
	ID3D12GraphicsCommandList* list, DirectX::XMFLOAT3X4& transform);
