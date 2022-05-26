#pragma once

#include <d3d12.h>
#include <vector>

#include "D3DPtr.h"

class ManagedCommandAllocator
{
private:
	D3DPtr<ID3D12CommandAllocator> allocator;
	std::vector<ID3D12GraphicsCommandList*> commandLists;
	unsigned int currentList = static_cast<unsigned int>(-1);
	unsigned int firstUnexecuted = static_cast<unsigned int>(-1);
	D3D12_COMMAND_LIST_TYPE type;

	ID3D12Device* device;

public:
	ManagedCommandAllocator() = default;
	~ManagedCommandAllocator();

	void Initialize(ID3D12Device* deviceToUse, 
		D3D12_COMMAND_LIST_TYPE typeOfList);

	ID3D12GraphicsCommandList* ActiveList();
	void FinishActiveList(bool prepareNewList = false);
	void ExecuteCommands(ID3D12CommandQueue* queue);
	void Reset();

	size_t GetNrOfStoredLists();
	ID3D12GraphicsCommandList* GetStoredList(size_t index);
};