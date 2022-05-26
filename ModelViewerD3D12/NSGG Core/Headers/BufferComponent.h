#pragma once

#include <array>
#include <vector>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <optional>

#include "ResourceComponent.h"
#include "BufferAllocator.h"


struct BufferComponentInfo
{
	BufferInfo bufferInfo;
	bool mappedResource;
	ResourceHeapInfo heapInfo;
};

union BufferViewDesc
{
	struct ConstantBufferDesc
	{
		std::int64_t byteOffset = 0;
		std::int64_t sizeModifier = 0;
	} cb;

	struct ShaderResourceDesc
	{
		size_t firstElement = 0;
		size_t nrOfElements = size_t(-1);
		D3D12_BUFFER_SRV_FLAGS flags = D3D12_BUFFER_SRV_FLAG_NONE;
	} sr = ShaderResourceDesc();

	struct UnorderedAccessDesc
	{
		size_t firstElement = 0;
		size_t nrOfElements = size_t(-1);
		size_t counterOffsetInBytes = 0;
		ID3D12Resource* counterResource = nullptr;
		D3D12_BUFFER_UAV_FLAGS flags = D3D12_BUFFER_UAV_FLAG_NONE;
	} ua;

	struct RenderTargetDesc
	{
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		std::int64_t byteOffset = 0;
		std::int64_t nrOfElements = size_t(-1);
	} rt;

	BufferViewDesc(ViewType type)
	{
		if (type == ViewType::CBV)
			cb = ConstantBufferDesc();
		else if (type == ViewType::SRV)
			sr = ShaderResourceDesc();
		else if (type == ViewType::UAV)
			ua = UnorderedAccessDesc();
		else if (type == ViewType::RTV)
			rt = RenderTargetDesc();
	}

};

struct BufferReplacementViews
{
	std::optional<BufferViewDesc::ConstantBufferDesc> cb = std::nullopt;
	std::optional<BufferViewDesc::ShaderResourceDesc> sr = std::nullopt;
	std::optional<BufferViewDesc::UnorderedAccessDesc> ua = std::nullopt;
	std::optional<BufferViewDesc::RenderTargetDesc> rt = std::nullopt;
};

class BufferComponent : public ResourceComponent
{
private:
	BufferAllocator bufferAllocator;

	struct 
	{
		std::uint8_t index = std::uint8_t(-1);
		BufferViewDesc::ConstantBufferDesc desc;
	} cbv;

	struct
	{
		std::uint8_t index = std::uint8_t(-1);
		BufferViewDesc::ShaderResourceDesc desc;
	} srv;

	struct
	{
		std::uint8_t index = std::uint8_t(-1);
		BufferViewDesc::UnorderedAccessDesc desc;
	} uav;

	struct
	{
		std::uint8_t index = std::uint8_t(-1);
		BufferViewDesc::RenderTargetDesc desc;
	} rtv;

	D3D12_CONSTANT_BUFFER_VIEW_DESC CreateCBV(
		const BufferViewDesc::ConstantBufferDesc& desc, const BufferHandle& handle);
	D3D12_SHADER_RESOURCE_VIEW_DESC CreateSRV(
		const BufferViewDesc::ShaderResourceDesc& desc, const BufferHandle& handle);
	D3D12_UNORDERED_ACCESS_VIEW_DESC CreateUAV(
		const BufferViewDesc::UnorderedAccessDesc& desc, const BufferHandle& handle);
	D3D12_RENDER_TARGET_VIEW_DESC CreateRTV(
		const BufferViewDesc::RenderTargetDesc& desc, const BufferHandle& handle);

	bool CreateViews(const BufferReplacementViews& replacements,
		const BufferHandle& handle, ResourceIndex& resourceIndex);

	void InitializeBufferAllocator(ID3D12Device* device,
		const BufferComponentInfo& bufferInfo);
	void InitializeDescriptorAllocators(ID3D12Device* device,
		const std::vector<DescriptorAllocationInfo<BufferViewDesc>>& descriptorInfo);

public:
	BufferComponent() = default;
	~BufferComponent() = default;
	BufferComponent(const BufferComponent& other) = delete;
	BufferComponent& operator=(const BufferComponent& other) = delete;
	BufferComponent(BufferComponent&& other) noexcept;
	BufferComponent& operator=(BufferComponent&& other) noexcept;

	void Initialize(ID3D12Device* device, const BufferComponentInfo& bufferInfo,
		const std::vector<DescriptorAllocationInfo<BufferViewDesc>>& descriptorInfo);

	ResourceIndex CreateBuffer(size_t nrOfElements,
		const BufferReplacementViews& replacementViews = BufferReplacementViews());

	void RemoveComponent(ResourceIndex indexToRemove);

	const D3D12_CPU_DESCRIPTOR_HANDLE 
		GetDescriptorHeapCBV(ResourceIndex indexOffset = 0) const override;
	const D3D12_CPU_DESCRIPTOR_HANDLE 
		GetDescriptorHeapSRV(ResourceIndex indexOffset = 0) const override;
	const D3D12_CPU_DESCRIPTOR_HANDLE
		GetDescriptorHeapUAV(ResourceIndex indexOffset = 0) const override;
	const D3D12_CPU_DESCRIPTOR_HANDLE 
		GetDescriptorHeapRTV(ResourceIndex indexOffset = 0) const override;
	bool HasDescriptorsOfType(ViewType type) const override;

	BufferHandle GetBufferHandle(const ResourceIndex& index);
	unsigned char* GetMappedPtr();

	D3D12_RESOURCE_STATES GetCurrentState();
	D3D12_RESOURCE_BARRIER CreateTransitionBarrier(D3D12_RESOURCE_STATES newState,
		D3D12_RESOURCE_BARRIER_FLAGS flag = D3D12_RESOURCE_BARRIER_FLAG_NONE);

	void UpdateMappedBuffer(ResourceIndex index, void* data);
};