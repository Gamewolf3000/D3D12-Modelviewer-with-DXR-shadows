#pragma once

#include <vector>

#include "ResourceComponent.h"

typedef size_t GraphicalEntityIndex;

template<typename ComponentIndex>
class GraphicalComponentRegistry
{
private:
	ComponentIndex componentsPerEntity = 0;
	size_t firstFreeEntityIndex = size_t(-1);
	std::vector<ResourceIndex> componentIndices;

public:
	GraphicalComponentRegistry() = default;
	~GraphicalComponentRegistry() = default;

	void Initialize(const ComponentIndex& maxComponentIndex,
		size_t startingAllocatedNrOfEntities = 0);

	GraphicalEntityIndex CreateEntity();
	void RemoveEntity(const GraphicalEntityIndex& index);

	void SetResourceIndex(const GraphicalEntityIndex& entityIndex,
		const ComponentIndex& componentIndex, const ResourceIndex& resourceIndex);
	const ResourceIndex& GetResourceIndex(const GraphicalEntityIndex& entityIndex,
		const ComponentIndex& componentIndex) const;
	void ClearResourceIndex(const GraphicalEntityIndex& entityIndex,
		const ComponentIndex& componentIndex);
};

template<typename ComponentIndex>
inline void GraphicalComponentRegistry<ComponentIndex>::Initialize(const ComponentIndex& maxComponentIndex, size_t startingAllocatedNrOfEntities)
{
	componentsPerEntity = maxComponentIndex;
	componentIndices.reserve(componentsPerEntity * startingAllocatedNrOfEntities);
}

template<typename ComponentIndex>
inline GraphicalEntityIndex GraphicalComponentRegistry<ComponentIndex>::CreateEntity()
{
	GraphicalEntityIndex toReturn;

	if (firstFreeEntityIndex != size_t(-1))
	{
		toReturn = firstFreeEntityIndex;
		firstFreeEntityIndex = componentIndices[firstFreeEntityIndex];
	}
	else
	{
		toReturn = componentIndices.size();
		componentIndices.resize(componentIndices.size() + componentsPerEntity);
	}

	for (size_t i = 0; i < componentsPerEntity; ++i)
		componentIndices[toReturn + i] = ResourceIndex(-1);

	return toReturn;
}

template<typename ComponentIndex>
inline void GraphicalComponentRegistry<ComponentIndex>::RemoveEntity(
	const GraphicalEntityIndex& index)
{
	componentIndices[index] = firstFreeEntityIndex;
	firstFreeEntityIndex = index;
}

template<typename ComponentIndex>
inline void GraphicalComponentRegistry<ComponentIndex>::SetResourceIndex(
	const GraphicalEntityIndex& entityIndex, const ComponentIndex& componentIndex,
	const ResourceIndex& resourceIndex)
{
	componentIndices[entityIndex + componentIndex] = resourceIndex;
}

template<typename ComponentIndex>
inline const ResourceIndex& GraphicalComponentRegistry<ComponentIndex>::GetResourceIndex(
	const GraphicalEntityIndex& entityIndex, const ComponentIndex& componentIndex) const
{
	return componentIndices[entityIndex + componentIndex];
}

template<typename ComponentIndex>
inline void GraphicalComponentRegistry<ComponentIndex>::ClearResourceIndex(
	const GraphicalEntityIndex& entityIndex, const ComponentIndex& componentIndex)
{
	componentIndices[entityIndex + componentIndex] = ResourceIndex(-1);
}
