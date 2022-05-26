#pragma once

#include <stdexcept>

#include <d3d12.h>
#include <dxgi1_6.h>

#include "D3DPtr.h"
#include "FrameObject.h"

struct SwapChainFrame
{
	D3DPtr<ID3D12Resource> backbuffer;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
	D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_PRESENT;
};

template<FrameType Frames>
class ManagedSwapChain : public FrameObject<SwapChainFrame, Frames>
{
private:
	ID3D12Device* device;
	D3DPtr<IDXGISwapChain3> swapChain;
	D3DPtr<ID3D12DescriptorHeap> rtvHeap;
	HANDLE backbufferWaitHandle = nullptr;
	HWND windowHandle = nullptr;
	unsigned int rtvSize = 0;

	void CreateDescriptorHeap();
	void CreateRTVs();

public:
	ManagedSwapChain() = default;
	~ManagedSwapChain() = default;

	void Initialize(ID3D12Device* deviceToUse, ID3D12CommandQueue* queue,
		IDXGIFactory2* factory, HWND handleToWindow, bool fullscreen);

	HANDLE GetWaitHandle();
	void ResizeBackbuffers(unsigned int newWidth, unsigned int newHeight);
	unsigned int GetCurrentBackbufferIndex();

	D3D12_RESOURCE_BARRIER TransitionToPresent();
	D3D12_RESOURCE_BARRIER TransitionToRenderTarget();

	void ClearBackbuffer(ID3D12GraphicsCommandList* commandList);
	void Present();
};

template<FrameType Frames>
inline void ManagedSwapChain<Frames>::CreateDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	desc.NumDescriptors = Frames;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NodeMask = 0;
	HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtvHeap));
	if (FAILED(hr))
		throw std::runtime_error("Could not create descriptor heap for backbuffers");

	rtvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

template<FrameType Frames>
inline void ManagedSwapChain<Frames>::CreateRTVs()
{
	auto rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();

	for (std::uint8_t i = 0; i < Frames; ++i)
	{
		D3DPtr<ID3D12Resource> currentBuffer;
		HRESULT hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&currentBuffer));
		if (FAILED(hr))
			throw std::runtime_error("Could not fetch backbuffer");

		device->CreateRenderTargetView(currentBuffer, nullptr, rtvHandle);
		this->frameObjects[i].backbuffer = std::move(currentBuffer);
		this->frameObjects[i].rtvHandle = rtvHandle;
		rtvHandle.ptr += rtvSize;
	}
}

template<FrameType Frames>
inline void ManagedSwapChain<Frames>::Initialize(ID3D12Device* deviceToUse,
	ID3D12CommandQueue* queue, IDXGIFactory2* factory, HWND handleToWindow,
	bool fullscreen)
{
	device = deviceToUse;
	windowHandle = handleToWindow;

	DXGI_SWAP_CHAIN_DESC1 desc;
	desc.Width = 0;
	desc.Height = 0;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.Stereo = false;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = Frames;
	desc.Scaling = DXGI_SCALING_STRETCH;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreenDesc;
	fullscreenDesc.RefreshRate = { 60, 1 };
	fullscreenDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	fullscreenDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	fullscreenDesc.Windowed = !fullscreen;

	D3DPtr<IDXGISwapChain1> temp;
	HRESULT hr = factory->CreateSwapChainForHwnd(queue, windowHandle, &desc,
		nullptr, nullptr, &temp);
	if (FAILED(hr))
		throw std::runtime_error("Could not create swap chain");

	hr = temp->QueryInterface(__uuidof(IDXGISwapChain3),
		reinterpret_cast<void**>(&swapChain));
	if (FAILED(hr))
		throw std::runtime_error("Could not query swap chain 3");

	hr = swapChain->SetMaximumFrameLatency(Frames);
	if (FAILED(hr))
		throw std::runtime_error("Could not set max frame latency");

	backbufferWaitHandle = swapChain->GetFrameLatencyWaitableObject();

	CreateDescriptorHeap();
	CreateRTVs();
}

template<FrameType Frames>
inline HANDLE ManagedSwapChain<Frames>::GetWaitHandle()
{
	return backbufferWaitHandle;
}

template<FrameType Frames>
inline void ManagedSwapChain<Frames>::ResizeBackbuffers(unsigned int newWidth,
	unsigned int newHeight)
{
	for (auto& swapChainFrame : this->frameObjects)
		swapChainFrame.backbuffer = D3DPtr<ID3D12Resource>();

	swapChain->ResizeBuffers(Frames, newWidth, newHeight, 
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);

	CreateRTVs();
}

template<FrameType Frames>
inline unsigned int ManagedSwapChain<Frames>::GetCurrentBackbufferIndex()
{
	return swapChain->GetCurrentBackBufferIndex();
}

template<FrameType Frames>
inline D3D12_RESOURCE_BARRIER ManagedSwapChain<Frames>::TransitionToPresent()
{
	D3D12_RESOURCE_BARRIER toReturn;
	toReturn.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toReturn.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	toReturn.Transition.pResource = this->Active().backbuffer;
	toReturn.Transition.StateBefore = this->Active().currentState;
	toReturn.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	toReturn.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	this->Active().currentState = D3D12_RESOURCE_STATE_PRESENT;

	return toReturn;
}

template<FrameType Frames>
inline D3D12_RESOURCE_BARRIER ManagedSwapChain<Frames>::TransitionToRenderTarget()
{
	D3D12_RESOURCE_BARRIER toReturn;
	toReturn.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toReturn.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	toReturn.Transition.pResource = this->Active().backbuffer;
	toReturn.Transition.StateBefore = this->Active().currentState;
	toReturn.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	toReturn.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	this->Active().currentState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	return toReturn;
}

template<FrameType Frames>
inline void ManagedSwapChain<Frames>::ClearBackbuffer(
	ID3D12GraphicsCommandList* commandList)
{
	float clearColour[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	commandList->ClearRenderTargetView(this->Active().rtvHandle, clearColour,
		0, nullptr);
}

template<FrameType Frames>
inline void ManagedSwapChain<Frames>::Present()
{
	HRESULT hr = swapChain->Present(0, 0);

	if (FAILED(hr))
		throw std::runtime_error("Could not present backbuffer");
}