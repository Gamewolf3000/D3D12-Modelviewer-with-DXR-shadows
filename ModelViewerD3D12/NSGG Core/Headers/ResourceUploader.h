#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <utility>

#include "D3DPtr.h"
#include "HeapHelper.h"

struct TextureUploadInfo
{
	unsigned int width = 1;
	unsigned int height = 1;
	unsigned int depth = 1;
	size_t texelSizeInBytes = 0;

	unsigned int offsetWidth = 0;
	unsigned int offsetHeight = 0;
	unsigned int offsetDepth = 0;

	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
};

class ResourceUploader
{
private:
	ID3D12Device* device = nullptr;
	D3DPtr<ID3D12Resource> buffer;
	unsigned char* mappedPtr = nullptr;
	size_t latestUploadId = 0;
	size_t totalMemory = 0;
	AllocationStrategy allocationStrategy;

	struct UploadChunk
	{
		// EMPTY
	};

	HeapHelper<UploadChunk> uploadChunks;

	void AllocateBuffer(ID3D12Heap* heap, size_t heapOffset);
	void AllocateBuffer();

	size_t AlignAdress(size_t dataSize, size_t alignment);

	void CopyBufferRegionToResource(ID3D12Resource* toUploadTo, 
		ID3D12GraphicsCommandList* commandList, void* data, size_t offsetFromStart,
		size_t dataSize, size_t alignment, size_t freeChunkIndex);

	void MemcpyTextureData(unsigned char* destinationStart,
		unsigned char* sourceStart, const TextureUploadInfo& uploadInfo);
	void CopyTextureRegionToResource(ID3D12Resource* toUploadTo, 
		ID3D12GraphicsCommandList* commandList, void* data,
		const TextureUploadInfo& uploadInfo, unsigned int subresourceIndex,
		size_t freeChunkIndex);

public:
	ResourceUploader() = default;
	~ResourceUploader() = default;

	ResourceUploader(const ResourceUploader& other) = delete;
	ResourceUploader& operator=(const ResourceUploader& other) = delete;
	ResourceUploader(ResourceUploader&& other) noexcept;
	ResourceUploader& operator=(ResourceUploader&& other) noexcept;

	void Initialize(ID3D12Device* deviceToUse, ID3D12Heap* heap, size_t startOffset,
		size_t endOffset, AllocationStrategy strategy);
	void Initialize(ID3D12Device* deviceToUse, size_t heapSize,
		AllocationStrategy strategy);

	bool UploadBufferResourceData(ID3D12Resource* toUploadTo, 
		ID3D12GraphicsCommandList* commandList, void* data,
		size_t offsetFromStart, size_t dataSize, size_t alignment);

	bool UploadTextureResourceData(ID3D12Resource* toUploadTo,
		ID3D12GraphicsCommandList* commandList, void* data,
		const TextureUploadInfo& TextureUploadInfo, 
		unsigned int subresourceIndex = 0);

	//bool UploadResource(ID3D12Resource* toUploadTo,
	//	ID3D12GraphicsCommandList* commandList, void* dataPtr, size_t dataSize,
	//	size_t alignment, unsigned int xOffset = 0, unsigned int yOffset = 0,
	//	unsigned int zOffset = 0, unsigned int subresource = 0);

	void RestoreUsedMemory();
};