#pragma once

#include <stdexcept>

#include <d3d12.h>
#include <dxgi1_6.h>

#include "FrameBased.h"
#include "FrameObject.h"
#include "D3DPtr.h"
#include "FrameBufferComponent.h"
#include "DirectAccessComponentBinder.h"
#include "GraphicalComponentRegistry.h"
#include "ManagedSwapChain.h"
#include "ManagedResourceComponents.h"
#include "ManagedFence.h"
#include "ManagedCommandAllocator.h"

template<FrameType Frames>
class BaseScene : public FrameBased<Frames>
{
protected:
	typedef std::uint32_t ComponentIndex;
	GraphicalComponentRegistry<ComponentIndex> registry;
	ManagedResourceComponents<Frames> resourceComponents;

	D3DPtr<ID3D12CommandQueue> directQueue;
	D3DPtr<ID3D12CommandQueue> computeQueue;
	D3DPtr<ID3D12CommandQueue> copyQueue;

	unsigned int rtvSize = 0;
	unsigned int dsvSize = 0;
	unsigned int shaderViewSize = 0;
	D3DPtr<IDXGIFactory2> factory;
	D3DPtr<ID3D12Device> device;
	ManagedSwapChain<Frames> swapChain;
	FrameObject<ManagedFence, Frames> endOfFrameFences;

	HWND window;
	unsigned int screenWidth = 0;
	unsigned int screenHeight = 0;

	void ThrowIfFailed(HRESULT hr, const std::exception& exceptionToThrow);
	void CreateFactory();
	void CreateDevice(IDXGIAdapter* adapter);
	void SetDescriptorSizes();
	void CreateCommandQueues();

	void FlushAllQueues();

	bool PossibleToSwapFrame();
	void SwapFrame() override;

public: 
	BaseScene() = default;
	virtual ~BaseScene() = default;

	virtual void Initialize(HWND windowHandle, bool fullscreen,
		unsigned int backbufferWidth,unsigned int backbufferHeight,
		size_t minSizePerUploader, AllocationStrategy allocationStrategy,
		IDXGIAdapter* adapter = nullptr);
	//virtual void Shutdown() = 0;
	virtual void ChangeScreenSize(unsigned int backbufferWidth,
		unsigned int backbufferHeight);

	virtual void Update() = 0;
	virtual void Render() = 0;
};

template<FrameType Frames>
inline void BaseScene<Frames>::ThrowIfFailed(HRESULT hr, 
	const std::exception& exceptionToThrow)
{
	if (FAILED(hr))
		throw exceptionToThrow;
}

template<FrameType Frames>
inline void BaseScene<Frames>::CreateFactory()
{
	HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
	ThrowIfFailed(hr, std::runtime_error("Could not create dxgi factory"));
}

template<FrameType Frames>
inline void BaseScene<Frames>::CreateDevice(IDXGIAdapter* adapter)
{
	HRESULT hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, 
		IID_PPV_ARGS(&device));
	ThrowIfFailed(hr, std::runtime_error("Could not create device"));
}

template<FrameType Frames>
inline void BaseScene<Frames>::SetDescriptorSizes()
{
	rtvSize = 
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	dsvSize = 
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	shaderViewSize = 
		device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

template<FrameType Frames>
inline void BaseScene<Frames>::CreateCommandQueues()
{
	D3D12_COMMAND_QUEUE_DESC desc;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	HRESULT hr = device->CreateCommandQueue(&desc, IID_PPV_ARGS(&copyQueue));
	ThrowIfFailed(hr, std::runtime_error("Could not create copy command queue"));

	desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	hr = device->CreateCommandQueue(&desc, IID_PPV_ARGS(&computeQueue));
	ThrowIfFailed(hr, std::runtime_error("Could not create compute command queue"));

	desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	hr = device->CreateCommandQueue(&desc, IID_PPV_ARGS(&directQueue));
	ThrowIfFailed(hr, std::runtime_error("Could not create direct command queue"));
}

template<FrameType Frames>
inline void BaseScene<Frames>::FlushAllQueues()
{
	ID3D12Fence* fences[3];
	HRESULT hr = S_OK;
	hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fences[0]));
	ThrowIfFailed(hr, std::runtime_error("Could not create flush fence 0"));
	hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fences[1]));
	ThrowIfFailed(hr, std::runtime_error("Could not create flush fence 1"));
	hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fences[2]));
	ThrowIfFailed(hr, std::runtime_error("Could not create flush fence 2"));

	hr = directQueue->Signal(fences[0], 1);
	ThrowIfFailed(hr, std::runtime_error("Could not set direct flush fence"));
	hr = computeQueue->Signal(fences[1], 1);
	ThrowIfFailed(hr, std::runtime_error("Could not set compute flush fence"));
	hr = copyQueue->Signal(fences[2], 1);
	ThrowIfFailed(hr, std::runtime_error("Could not set copy flush fence"));

	for (ID3D12Fence* fence : fences)
	{
		if (fence->GetCompletedValue() < 1)
		{
			HANDLE eventHandle = CreateEventEx(nullptr, 0, 0, EVENT_ALL_ACCESS);
			hr = fence->SetEventOnCompletion(1, eventHandle);
			ThrowIfFailed(hr, std::runtime_error("Error setting wait for fence event"));
			WaitForSingleObject(eventHandle, INFINITE);
			CloseHandle(eventHandle);
		}

		fence->Release();
	}
}

template<FrameType Frames>
inline bool BaseScene<Frames>::PossibleToSwapFrame()
{
	if (!endOfFrameFences.Next().Completed())
		return false; // Still working on next frame, cannot swap

	DWORD backbufferWait = WaitForSingleObjectEx(swapChain.GetWaitHandle(),
		0, true);

	if (backbufferWait != WAIT_OBJECT_0)
		return false; // Backbuffer not done presenting yet... I think?

	return true;
}

template<FrameType Frames>
inline void BaseScene<Frames>::SwapFrame()
{
	FrameBased<Frames>::SwapFrame();
	resourceComponents.SwapFrame();
	swapChain.SwapFrame();
	endOfFrameFences.SwapFrame();
}

template<FrameType Frames>
inline void BaseScene<Frames>::Initialize(HWND windowHandle, bool fullscreen,
	unsigned int backbufferWidth, unsigned int backbufferHeight, 
	size_t minSizePerUploader, AllocationStrategy allocationStrategy,
	IDXGIAdapter* adapter)
{
	window = windowHandle;
	screenWidth = backbufferWidth;
	screenHeight = backbufferHeight;
	CreateFactory();
	CreateDevice(adapter);
	SetDescriptorSizes();
	CreateCommandQueues();
	swapChain.Initialize(device, directQueue, factory, windowHandle, fullscreen);
	endOfFrameFences.Initialize(&ManagedFence::Initialize, device.Get(), size_t(0));
	resourceComponents.Initialize(device, minSizePerUploader, allocationStrategy);
}

template<FrameType Frames>
inline void BaseScene<Frames>::ChangeScreenSize(unsigned int backbufferWidth,
	unsigned int backbufferHeight)
{
	screenWidth = backbufferWidth;
	screenHeight = backbufferHeight;
	FlushAllQueues();
	swapChain.ResizeBackbuffers(backbufferWidth, backbufferHeight);

	while (this->activeFrame != Frames - 1)
		this->SwapFrame();
}