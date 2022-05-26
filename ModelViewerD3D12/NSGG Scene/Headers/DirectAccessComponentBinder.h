#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>

#include <stdexcept>
#include <cstdint>
#include <cmath>

#include "FrameObject.h"
#include "D3DPtr.h"
#include "ResourceComponent.h"
#include "ResourceUploader.h"

template<typename ComponentIndex, FrameType Frames>
class DirectAccessComponentBinder : public FrameBased<Frames>
{
public:

	struct ComponentToBind
	{
		ComponentIndex index;
		ViewType viewType;
		UINT maxComponents;
	};

private:
	ID3D12Device* device = nullptr;
	std::vector<ComponentToBind> componentsToBind;
	std::vector<std::uint32_t> indices;
	D3DPtr<ID3D12DescriptorHeap> cpuHeap;
	std::uint32_t descriptorsInHeap = 0;
	size_t descriptorSize = 0;
	FrameObject<D3DPtr<ID3D12Resource>, Frames> componentIndexBuffer;
	D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_COMMON;

	void CreateComponentIndexBuffer();
	void CreateDescriptorHeap();

	D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHeap(ResourceComponent* component,
		ViewType type);

public:
	DirectAccessComponentBinder() = default;

	DirectAccessComponentBinder(const DirectAccessComponentBinder& other) = delete;
	DirectAccessComponentBinder& operator=(const DirectAccessComponentBinder& other)
		= delete;
	DirectAccessComponentBinder(DirectAccessComponentBinder&& other);
	DirectAccessComponentBinder& operator=(DirectAccessComponentBinder&& other);

	void Initialize(ID3D12Device* deviceToUse, size_t shaderBindDescriptorSize,
		const std::vector<ComponentToBind>& toBind);

	~DirectAccessComponentBinder();

	void BindComponents(ResourceUploader* uploader, 
		ID3D12GraphicsCommandList* commandList, ID3D12DescriptorHeap* toCopyTo,
		size_t heapStartOffset, const std::vector<ResourceComponent*>& components);

	D3D12_RESOURCE_BARRIER TransitionToCopyDest(
		D3D12_RESOURCE_BARRIER_FLAGS flag = D3D12_RESOURCE_BARRIER_FLAG_NONE);
	D3D12_RESOURCE_BARRIER TransitionToShaderResource(
		D3D12_RESOURCE_BARRIER_FLAGS flag = D3D12_RESOURCE_BARRIER_FLAG_NONE);

	D3D12_GPU_VIRTUAL_ADDRESS GetBufferAdress();

	void SwapFrame() override;
};

template<typename ComponentIndex, FrameType Frames>
inline void 
DirectAccessComponentBinder<ComponentIndex, Frames>::CreateComponentIndexBuffer()
{
	D3D12_HEAP_PROPERTIES heapProperties;
	ZeroMemory(&heapProperties, sizeof(heapProperties));
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	desc.Width = sizeof(std::uint32_t) * indices.size();
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	std::function<void(D3DPtr<ID3D12Resource>&)> initFunc =
		[&](D3DPtr<ID3D12Resource>& resource)
	{
		HRESULT hr = device->CreateCommittedResource(&heapProperties, 
			D3D12_HEAP_FLAG_NONE,&desc, resourceState, nullptr,
			IID_PPV_ARGS(&resource));

		if (FAILED(hr))
			throw std::runtime_error("Could not create component index buffer");
	};

	componentIndexBuffer.Initialize(initFunc);
}

template<typename ComponentIndex, FrameType Frames>
inline void 
DirectAccessComponentBinder<ComponentIndex, Frames>::CreateDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NodeMask = 0;
	desc.NumDescriptors = static_cast<UINT>(descriptorsInHeap);
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&cpuHeap));

	if (FAILED(hr))
		throw std::runtime_error("Could not create direct access binder cpu heap");
}

template<typename ComponentIndex, FrameType Frames>
inline D3D12_CPU_DESCRIPTOR_HANDLE 
DirectAccessComponentBinder<ComponentIndex, Frames>::GetDescriptorHeap(
	ResourceComponent* component, ViewType type)
{
	switch (type)
	{
	case ViewType::CBV:
		return component->GetDescriptorHeapCBV();
	case ViewType::SRV:
		return component->GetDescriptorHeapSRV();
	case ViewType::UAV:
		return component->GetDescriptorHeapUAV();
	case ViewType::RTV:
		return component->GetDescriptorHeapRTV();
	case ViewType::DSV:
		return component->GetDescriptorHeapDSV();
	default:
		throw std::runtime_error("Incorrect view type in direct access binder");
	}
}

template<typename ComponentIndex, FrameType Frames>
inline
DirectAccessComponentBinder<ComponentIndex, Frames>::DirectAccessComponentBinder(
	DirectAccessComponentBinder&& other) : device(other.device), 
	componentsToBind(std::move(other.componentsToBind)), 
	indices(std::move(other.indices)), cpuHeap(std::move(other.cpuHeap)), 
	descriptorsInHeap(other.descriptorsInHeap), 
	descriptorSize(other.descriptorSize), 
	componentIndexBuffer(std::move(other.componentIndexBuffer)), 
	resourceState(other.resourceState)
{
	other.device = nullptr;
	other.descriptorsInHeap = 0;
	other.descriptorSize = 0;
	for (FrameType i = 0; i < Frames; ++i)
	{
		other.componentIndexBuffer.Active() = nullptr;
		componentIndexBuffer.SwapFrame();
	}
	other.resourceState = D3D12_RESOURCE_STATE_COMMON;
}

template<typename ComponentIndex, FrameType Frames>
inline DirectAccessComponentBinder<ComponentIndex, Frames>& 
DirectAccessComponentBinder<ComponentIndex, Frames>::operator=(
	DirectAccessComponentBinder&& other)
{
	if (this != &other)
	{
		device = other.device;
		componentsToBind = std::move(other.componentsToBind);
		indices = std::move(other.indices);
		cpuHeap = std::move(other.cpuHeap);
		descriptorsInHeap = other.descriptorsInHeap;
		descriptorSize = other.descriptorSize;
		componentIndexBuffer = std::move(other.componentIndexBuffer);
		resourceState = other.resourceState;

		other.device = nullptr;
		other.descriptorsInHeap = 0;
		other.descriptorSize = 0;
		for (FrameType i = 0; i < Frames; ++i)
		{
			other.componentIndexBuffer.Active() = nullptr;
			componentIndexBuffer.SwapFrame();
		}
		other.resourceState = D3D12_RESOURCE_STATE_COMMON;
	}

	return *this;
}

template<typename ComponentIndex, FrameType Frames>
inline void DirectAccessComponentBinder<ComponentIndex, Frames>::Initialize(
	ID3D12Device* deviceToUse, size_t shaderBindDescriptorSize,
	const std::vector<ComponentToBind>& toBind)
{
	device = deviceToUse;
	descriptorSize = shaderBindDescriptorSize;
	componentsToBind = toBind;

	for (auto& componentToBind : componentsToBind)
		descriptorsInHeap += componentToBind.maxComponents;

	size_t totalSize = componentsToBind.size() * sizeof(std::uint32_t);
	totalSize = static_cast<size_t>(std::ceil((1.0 * totalSize) / 256) * 256);
	totalSize /= sizeof(std::uint32_t);
	indices.resize(totalSize);
	CreateComponentIndexBuffer();
	CreateDescriptorHeap();
}

template<typename ComponentIndex, FrameType Frames>
inline 
DirectAccessComponentBinder<ComponentIndex, Frames>::~DirectAccessComponentBinder()
{
	for (FrameType i = 0; i < Frames; ++i)
	{
		if (componentIndexBuffer.Active() != nullptr)
			componentIndexBuffer.Active()->Release();
		componentIndexBuffer.SwapFrame();
	}
}

template<typename ComponentIndex, FrameType Frames>
inline void DirectAccessComponentBinder<ComponentIndex, Frames>::BindComponents(
	ResourceUploader* uploader, ID3D12GraphicsCommandList* commandList,
	ID3D12DescriptorHeap* toCopyTo, size_t heapStartOffset,
	const std::vector<ResourceComponent*>& components)
{
	auto start = cpuHeap->GetCPUDescriptorHandleForHeapStart();
	size_t currentOffset = 0;

	for (size_t i = 0; i < componentsToBind.size(); ++i)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE destination = start;
		destination.ptr += currentOffset * descriptorSize;
		ResourceComponent* component = components[componentsToBind[i].index];
		auto source = GetDescriptorHeap(component, componentsToBind[i].viewType);

		device->CopyDescriptorsSimple(componentsToBind[i].maxComponents,
			destination, source, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		indices[i] = static_cast<std::uint32_t>(currentOffset + heapStartOffset / descriptorSize);
		currentOffset += componentsToBind[i].maxComponents;
	}

	auto destination = toCopyTo->GetCPUDescriptorHandleForHeapStart();
	destination.ptr += heapStartOffset;
	device->CopyDescriptorsSimple(descriptorsInHeap, destination,
		cpuHeap->GetCPUDescriptorHandleForHeapStart(),
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	size_t chunk = uploader->UploadBufferResourceData(
		componentIndexBuffer.Active(), commandList, &indices[0], 0,
		sizeof(std::uint32_t) * indices.size(), alignof(std::uint32_t));

	if (chunk == size_t(-1))
		throw std::runtime_error("Could not upload to component index buffer");
}

template<typename ComponentIndex, FrameType Frames>
inline D3D12_GPU_VIRTUAL_ADDRESS 
DirectAccessComponentBinder<ComponentIndex, Frames>::GetBufferAdress()
{
	return componentIndexBuffer.Active()->GetGPUVirtualAddress();
}

template<typename ComponentIndex, FrameType Frames>
inline void DirectAccessComponentBinder<ComponentIndex, Frames>::SwapFrame()
{
	FrameBased<Frames>::SwapFrame();
	componentIndexBuffer.SwapFrame();
}
