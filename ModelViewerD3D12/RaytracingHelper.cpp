#include "RaytracingHelper.h"

#include <stdexcept>

void QueryInterfaces(ID3D12Device* device, ID3D12GraphicsCommandList* list,
	ID3D12Device5*& device5, ID3D12GraphicsCommandList4*& list4)
{
	HRESULT hr = device->QueryInterface(__uuidof(ID3D12Device5),
		reinterpret_cast<void**>(&device5));

	if (FAILED(hr))
		throw std::runtime_error("Could not query device 5 interface");

	hr = list->QueryInterface(__uuidof(ID3D12GraphicsCommandList4),
		reinterpret_cast<void**>(&list4));

	if (FAILED(hr))
		throw std::runtime_error("Could not query list 4 interface");
}


bool CreateCommitedBuffer(ID3D12Device* device, ID3D12Resource*& toSet,
	D3D12_HEAP_TYPE heapType, size_t bufferSize, D3D12_RESOURCE_FLAGS flags,
	D3D12_RESOURCE_STATES initialState)
{
	D3D12_HEAP_PROPERTIES heapProperties;
	ZeroMemory(&heapProperties, sizeof(heapProperties));
	heapProperties.Type = heapType;

	D3D12_RESOURCE_DESC bufferDesc;
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	bufferDesc.Width = bufferSize;
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufferDesc.SampleDesc.Count = 1;
	bufferDesc.SampleDesc.Quality = 0;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufferDesc.Flags = flags;

	HRESULT hr = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
		&bufferDesc, initialState, nullptr, IID_PPV_ARGS(&toSet));

	return hr == S_OK;
}

bool BuildAccelerationStructureHelper(ID3D12Device5* device,
	ID3D12GraphicsCommandList4* list,
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs,
	UINT64 resultBufferSize, ID3D12Resource*& resultBuffer,
	UINT64 scratchBufferSize, ID3D12Resource*& scratchBuffer)
{
	if (!CreateCommitedBuffer(device, resultBuffer, D3D12_HEAP_TYPE_DEFAULT,
		resultBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE))
	{
		return false;
	}

	if (!CreateCommitedBuffer(device, scratchBuffer, D3D12_HEAP_TYPE_DEFAULT,
		scratchBufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_COMMON)) // Warning if actually unordered access that effectively created as common
	{
		resultBuffer->Release();
		return false;
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC accelerationStructureDesc;
	accelerationStructureDesc.DestAccelerationStructureData =
		resultBuffer->GetGPUVirtualAddress();
	accelerationStructureDesc.Inputs = inputs;
	accelerationStructureDesc.SourceAccelerationStructureData = NULL;
	accelerationStructureDesc.ScratchAccelerationStructureData =
		scratchBuffer->GetGPUVirtualAddress();

	list->BuildRaytracingAccelerationStructure(&accelerationStructureDesc, 0, nullptr);

	return true;
}

bool BuildBottomLevelAccelerationStructure(ID3D12Device5* device, 
	ID3D12GraphicsCommandList4* list,
	std::vector<std::pair<D3D12_GPU_VIRTUAL_ADDRESS, UINT>> positions,
	std::vector<std::pair<D3D12_GPU_VIRTUAL_ADDRESS, UINT>> indices,
	ID3D12Resource*& resultAccelerationStructureBuffer,
	ID3D12Resource*& scratchAccelerationStructureBuffer)
{
	UINT nrOfBuffers = static_cast<UINT>(positions.size());
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottomLevelInputs;
	bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	bottomLevelInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	bottomLevelInputs.NumDescs = nrOfBuffers;
	bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometryDescs;
	geometryDescs.resize(nrOfBuffers);

	for (unsigned int i = 0; i < nrOfBuffers; ++i)
	{
		geometryDescs[i].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geometryDescs[i].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
		geometryDescs[i].Triangles.Transform3x4 = NULL;
		geometryDescs[i].Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
		geometryDescs[i].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		geometryDescs[i].Triangles.IndexCount = indices[i].second;
		geometryDescs[i].Triangles.VertexCount = positions[i].second;
		geometryDescs[i].Triangles.IndexBuffer = indices[i].first;
		geometryDescs[i].Triangles.VertexBuffer.StartAddress = positions[i].first;
		geometryDescs[i].Triangles.VertexBuffer.StrideInBytes = sizeof(float) * 3;
	}

	bottomLevelInputs.pGeometryDescs = geometryDescs.data();

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo;
	device->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &prebuildInfo);

	return BuildAccelerationStructureHelper(device, list, bottomLevelInputs,
		prebuildInfo.ResultDataMaxSizeInBytes, resultAccelerationStructureBuffer,
		prebuildInfo.ScratchDataSizeInBytes, scratchAccelerationStructureBuffer);
}

bool BuildTopLevelAccelerationStructure(ID3D12Device5* device,
	ID3D12GraphicsCommandList4* list,
	ID3D12Resource* bottomLevelResultAccelerationStructureBuffer,
	ID3D12Resource*& topLevelInstanceBuffer,
	ID3D12Resource*& resultAccelerationStructureBuffer,
	ID3D12Resource*& scratchAccelerationStructureBuffer)
{
	if (!CreateCommitedBuffer(device, topLevelInstanceBuffer, D3D12_HEAP_TYPE_UPLOAD,
		sizeof(D3D12_RAYTRACING_INSTANCE_DESC), D3D12_RESOURCE_FLAG_NONE,
		D3D12_RESOURCE_STATE_GENERIC_READ))
	{
		return false;
	}

	D3D12_RAYTRACING_INSTANCE_DESC instancingDesc;
	ZeroMemory(&instancingDesc.Transform, sizeof(float) * 12);
	instancingDesc.Transform[0][0] = instancingDesc.Transform[1][1] =
		instancingDesc.Transform[2][2] = 1;
	instancingDesc.InstanceID = 0;
	instancingDesc.InstanceMask = 0xFF;
	instancingDesc.InstanceContributionToHitGroupIndex = 0;
	instancingDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
	instancingDesc.AccelerationStructure =
		bottomLevelResultAccelerationStructureBuffer->GetGPUVirtualAddress();

	D3D12_RANGE nothing = { 0, 0 };
	unsigned char* mappedPtr = nullptr;
	HRESULT hr = topLevelInstanceBuffer->Map(0, &nothing, reinterpret_cast<void**>(&mappedPtr));

	if (FAILED(hr))
		return false;

	memcpy(mappedPtr, &instancingDesc, sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
	topLevelInstanceBuffer->Unmap(0, nullptr);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs;
	topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	topLevelInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	topLevelInputs.NumDescs = 1;
	topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	topLevelInputs.InstanceDescs = topLevelInstanceBuffer->GetGPUVirtualAddress();

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo;
	device->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &prebuildInfo);

	return BuildAccelerationStructureHelper(device, list, topLevelInputs,
		prebuildInfo.ResultDataMaxSizeInBytes, resultAccelerationStructureBuffer,
		prebuildInfo.ScratchDataSizeInBytes, scratchAccelerationStructureBuffer);
}


AccelerationStructure CreateAccelerationStructure(ID3D12Device* device,
	ID3D12GraphicsCommandList* list, MeshIndex meshIndex, 
	MeshResourceLoader& resourceLoader, FrameBufferComponent<1>& positionComponents,
	FrameBufferComponent<1>& indexComponents)
{
    ID3D12Device5* device5;
    ID3D12GraphicsCommandList4* list4;
	QueryInterfaces(device, list, device5, list4);

	std::vector<std::pair<D3D12_GPU_VIRTUAL_ADDRESS, UINT>> positions;
	std::vector<std::pair<D3D12_GPU_VIRTUAL_ADDRESS, UINT>> indices;

	auto& mesh = resourceLoader.GetMeshInfo(meshIndex);
	
	for (auto& submesh : mesh.subMeshes)
	{
		auto positionHandle = positionComponents.GetBufferHandle(submesh.position);
		auto indexHandle = indexComponents.GetBufferHandle(submesh.indices);

		positions.push_back(std::make_pair(
			positionHandle.resource->GetGPUVirtualAddress() + positionHandle.startOffset,
			static_cast<UINT>(positionHandle.nrOfElements)));
		indices.push_back(std::make_pair(
			indexHandle.resource->GetGPUVirtualAddress() + indexHandle.startOffset,
			static_cast<UINT>(indexHandle.nrOfElements)));
	}

	AccelerationStructure toReturn;

	if (!BuildBottomLevelAccelerationStructure(device5, list4,
		positions, indices, toReturn.bottomLevel.resultBuffer,
		toReturn.bottomLevel.scratchBuffer))
	{
		throw std::runtime_error("Could not create bottom level acceleration structure");
	}

	D3D12_RESOURCE_BARRIER uavBarrier = {};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = toReturn.bottomLevel.resultBuffer;
	list4->ResourceBarrier(1, &uavBarrier);

	if (!BuildTopLevelAccelerationStructure(device5, list4,
		toReturn.bottomLevel.resultBuffer, toReturn.instanceBuffer,
		toReturn.topLevel.resultBuffer, toReturn.topLevel.scratchBuffer))
	{
		throw std::runtime_error("Could not create top level acceleration structure");

	}

	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = toReturn.topLevel.resultBuffer;
	list4->ResourceBarrier(1, &uavBarrier);

	device5->Release();
	list4->Release();

    return toReturn;
}

bool SetTopLevelTransform(AccelerationStructure& structure,
	ID3D12GraphicsCommandList* list, DirectX::XMFLOAT3X4& transform)
{
	ID3D12GraphicsCommandList4* list4 = nullptr;
	HRESULT hr = list->QueryInterface(__uuidof(ID3D12GraphicsCommandList4),
		reinterpret_cast<void**>(&list4));

	if (FAILED(hr))
		return false;

	D3D12_RAYTRACING_INSTANCE_DESC instancingDesc;
	memcpy(instancingDesc.Transform, &transform, 
		sizeof(instancingDesc.Transform));
	instancingDesc.InstanceID = 0;
	instancingDesc.InstanceMask = 0xFF;
	instancingDesc.InstanceContributionToHitGroupIndex = 0;
	instancingDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
	instancingDesc.AccelerationStructure =
		structure.bottomLevel.resultBuffer->GetGPUVirtualAddress();

	D3D12_RANGE nothing = { 0, 0 };
	unsigned char* mappedPtr = nullptr;
	hr = structure.instanceBuffer->Map(0, &nothing, reinterpret_cast<void**>(&mappedPtr));

	if (FAILED(hr))
		return false;

	memcpy(mappedPtr, &instancingDesc, sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
	structure.instanceBuffer->Unmap(0, nullptr);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs;
	topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	topLevelInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	topLevelInputs.NumDescs = 1;
	topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	topLevelInputs.InstanceDescs = structure.instanceBuffer->GetGPUVirtualAddress();

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC accelerationStructureDesc;
	accelerationStructureDesc.DestAccelerationStructureData =
		structure.topLevel.resultBuffer->GetGPUVirtualAddress();
	accelerationStructureDesc.Inputs = topLevelInputs;
	accelerationStructureDesc.SourceAccelerationStructureData = NULL;
	accelerationStructureDesc.ScratchAccelerationStructureData =
		structure.topLevel.scratchBuffer->GetGPUVirtualAddress();

	list4->BuildRaytracingAccelerationStructure(&accelerationStructureDesc, 0, nullptr);
	list4->Release();

	D3D12_RESOURCE_BARRIER uavBarrier = {};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = structure.topLevel.resultBuffer;
	list->ResourceBarrier(1, &uavBarrier);

	return true;
}