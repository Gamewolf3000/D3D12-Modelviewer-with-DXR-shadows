#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <utility>
#include <type_traits>
#include <functional>

#include "ResourceComponent.h"
#include "FrameBased.h"
#include "BufferComponent.h"
#include "TextureComponent.h"
#include "ComponentData.h"

template<typename Component, FrameType Frames, typename CreationOperation>
class FrameResourceComponent : public ResourceComponent, public FrameBased<Frames>
{
protected:
	std::array<Component, Frames> resourceComponents;

	enum class LifetimeOperationType
	{
		CREATION,
		REMOVAL
	};

	struct StoredLifetimeOperation
	{
		LifetimeOperationType type;
		std::uint8_t framesLeft;

		union
		{
			CreationOperation creation;
			struct RemovalOperation
			{
				ResourceIndex indexToRemove;
			} removal;
		};

		StoredLifetimeOperation()
		{
			ZeroMemory(this, sizeof(StoredLifetimeOperation));
		}
	};

	std::vector<StoredLifetimeOperation> storedLifetimeOperations;

	virtual void HandleStoredOperations() = 0;

public:
	FrameResourceComponent() = default;
	virtual ~FrameResourceComponent() = default;
	FrameResourceComponent(const FrameResourceComponent& other) = delete;
	FrameResourceComponent& operator=(const FrameResourceComponent& other) = delete;
	FrameResourceComponent(FrameResourceComponent&& other) noexcept;
	FrameResourceComponent& operator=(FrameResourceComponent&& other) noexcept;

	template<typename... InitialisationArguments>
	void Initialize(InitialisationArguments... initialisationArguments);

	void RemoveComponent(ResourceIndex indexToRemove) override;

	const D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHeapCBV(
		ResourceIndex indexOffset = 0) const override;
	const D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHeapSRV(
		ResourceIndex indexOffset = 0) const override;
	const D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHeapUAV(
		ResourceIndex indexOffset = 0) const override;
	const D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHeapRTV(
		ResourceIndex indexOffset = 0) const override;
	const D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHeapDSV(
		ResourceIndex indexOffset = 0) const override;

	bool HasDescriptorsOfType(ViewType type) const override;

	size_t NrOfDescriptors() const override;

	void SwapFrame() override;
};

template<typename Component, FrameType Frames, typename CreationOperation>
template<typename ...InitialisationArguments>
inline void 
FrameResourceComponent<Component, Frames, CreationOperation>::Initialize(
	InitialisationArguments ...initialisationArguments)
{
	for (auto& component : resourceComponents)
		component.Initialize(initialisationArguments...);
}

template<typename Component, FrameType Frames, typename CreationOperation>
inline 
FrameResourceComponent<Component, Frames, CreationOperation>::FrameResourceComponent(
	FrameResourceComponent&& other) noexcept : 
	ResourceComponent(std::move(other)), FrameBased<Frames>(std::move(other)), 
	resourceComponents(std::move(other.resourceComponents)), 
	storedLifetimeOperations(std::move(other.storedLifetimeOperations))
{
	// EMPTY
}

template<typename Component, FrameType Frames, typename CreationOperation>
inline FrameResourceComponent<Component, Frames, CreationOperation>& 
FrameResourceComponent<Component, Frames, CreationOperation>::operator=(
	FrameResourceComponent&& other) noexcept
{
	if (this != &other)
	{
		ResourceComponent::operator=(std::move(other));
		FrameBased<Frames>::operator=(std::move(other));
		resourceComponents = std::move(other.resourceComponents);
		storedLifetimeOperations = std::move(other.storedLifetimeOperations);
	}

	return *this;
}

template<typename Component, FrameType Frames, typename CreationOperation>
inline void 
FrameResourceComponent<Component, Frames, CreationOperation>::RemoveComponent(
	ResourceIndex indexToRemove)
{
	resourceComponents[this->activeFrame].RemoveComponent(indexToRemove);

	if constexpr (Frames != 1)
	{
		StoredLifetimeOperation toStore;
		toStore.type = LifetimeOperationType::REMOVAL;
		toStore.framesLeft = Frames - 1;
		toStore.removal.indexToRemove = indexToRemove;
		storedLifetimeOperations.push_back(toStore);
	}
}

template<typename Component, FrameType Frames, typename CreationOperation>
inline const D3D12_CPU_DESCRIPTOR_HANDLE 
FrameResourceComponent<Component, Frames, CreationOperation>::GetDescriptorHeapCBV(
	ResourceIndex indexOffset) const
{
	return resourceComponents[this->activeFrame].GetDescriptorHeapCBV(indexOffset);
}

template<typename Component, FrameType Frames, typename CreationOperation>
inline const D3D12_CPU_DESCRIPTOR_HANDLE 
FrameResourceComponent<Component, Frames, CreationOperation>::GetDescriptorHeapSRV(
	ResourceIndex indexOffset) const
{
	return resourceComponents[this->activeFrame].GetDescriptorHeapSRV(indexOffset);
}

template<typename Component, FrameType Frames, typename CreationOperation>
inline const D3D12_CPU_DESCRIPTOR_HANDLE 
FrameResourceComponent<Component, Frames, CreationOperation>::GetDescriptorHeapUAV(
	ResourceIndex indexOffset) const
{
	return resourceComponents[this->activeFrame].GetDescriptorHeapUAV(indexOffset);
}

template<typename Component, FrameType Frames, typename CreationOperation>
inline const D3D12_CPU_DESCRIPTOR_HANDLE 
FrameResourceComponent<Component, Frames, CreationOperation>::GetDescriptorHeapRTV(
	ResourceIndex indexOffset) const
{
	return resourceComponents[this->activeFrame].GetDescriptorHeapRTV(indexOffset);
}

template<typename Component, FrameType Frames, typename CreationOperation>
inline const D3D12_CPU_DESCRIPTOR_HANDLE 
FrameResourceComponent<Component, Frames, CreationOperation>::GetDescriptorHeapDSV(
	ResourceIndex indexOffset) const
{
	return resourceComponents[this->activeFrame].GetDescriptorHeapDSV(indexOffset);
}

template<typename Component, FrameType Frames, typename CreationOperation>
inline bool 
FrameResourceComponent<Component, Frames, CreationOperation>::HasDescriptorsOfType(
	ViewType type) const
{
	return resourceComponents[this->activeFrame].HasDescriptorsOfType(type);
}

template<typename Component, FrameType Frames, typename CreationOperation>
inline size_t FrameResourceComponent<Component, Frames, CreationOperation>::NrOfDescriptors() const
{
	return resourceComponents[this->activeFrame].NrOfDescriptors();
}

template<typename Component, FrameType Frames, typename CreationOperation>
inline void 
FrameResourceComponent<Component, Frames, CreationOperation>::SwapFrame()
{
	FrameBased<Frames>::SwapFrame();
	HandleStoredOperations();
}