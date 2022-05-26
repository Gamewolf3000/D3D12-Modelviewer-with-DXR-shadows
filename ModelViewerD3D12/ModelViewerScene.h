#pragma once

#include <DirectXMath.h>

#include "BaseScene.h"
#include "ManagedGraphicsPipelineState.h"

#include "MeshResourceLoader.h"
#include "RaytracingHelper.h"

static const short FRAMES = 2;

class ModelViewerScene : public BaseScene<FRAMES>
{
private:

	struct VertexShaderPerObjectIndices
	{
		unsigned int positionIndex = static_cast<unsigned int>(-1);
		unsigned int uvIndex = static_cast<unsigned int>(-1);
		unsigned int normalIndex = static_cast<unsigned int>(-1);
		unsigned int tangentIndex = static_cast<unsigned int>(-1);
		unsigned int bitangentIndex = static_cast<unsigned int>(-1);

		unsigned int indicesIndex = static_cast<unsigned int>(-1);

		unsigned int worldMatrix = static_cast<unsigned int>(-1);
		unsigned int vpMatrix = static_cast<unsigned int>(-1);

		char padding[256 - 32];
	};

	struct PixelShaderPerObjectIndices
	{
		unsigned int diffuseMapIndex = static_cast<unsigned int>(-1);
		unsigned int specularMapIndex = static_cast<unsigned int>(-1);
		unsigned int normalMapIndex = static_cast<unsigned int>(-1);

		char padding[256 - 12];
	};

	struct PixelShaderPerFrameIndices
	{
		unsigned int pointlightIndex = static_cast<unsigned int>(-1);
		unsigned int cameraInfoIndex = static_cast<unsigned int>(-1);
	};

	struct Pointlight
	{
		float position[3];
		float range;
		float colour[3];
		float padding = 0.0f;
	};

	struct CameraInfo
	{
		float position[3];
		float padding = 0.0f;
	};

	struct DrawableObject
	{
		size_t subMeshIndex;
		ResourceIndex vertexShaderBuffer = ResourceIndex(-1);
		ResourceIndex pixelShaderBuffer = ResourceIndex(-1);
	};

	std::vector<DrawableObject> objects;

	FrameObject<ManagedFence, FRAMES> updateCopyFence;
	ManagedGraphicsPipelineState pipelineState;

	FrameObject<ManagedCommandAllocator, FRAMES> copyAllocators;
	FrameObject<ManagedCommandAllocator, FRAMES> directAllocators;

	ComponentIdentifier worldMatrixComponent;
	ResourceIndex worldMatrix;

	ComponentIdentifier cameraMatrixComponent;
	ResourceIndex frontCameraMatrix;
	ComponentIdentifier cameraInfoComponent;
	ResourceIndex frontCameraInfo;

	ComponentIdentifier pointlightComponent;
	ResourceIndex light;

	ComponentIdentifier depthMapComponent;
	ResourceIndex depthMap;
	
	ComponentIdentifier vertexShaderPerObjectComponent;
	ComponentIdentifier pixelShaderPerObjectComponent;

	ComponentIdentifier pixelShaderPerFrameComponent;
	ResourceIndex pixelShaderPerFrame;

	MeshResourceLoader meshLoader;
	MeshIndex loadedMeshIndex = MeshIndex(-1);
	FrameObject<AccelerationStructure, FRAMES> meshAccelerationStructures;

	D3DPtr<ID3D12DescriptorHeap> imguiHeap;
	float rotation = 0.0f;
	float scaling = 1.01f;
	int subMeshToRender = -1;

	void UpdatePerObjectBuffers();
	void UpdatePerFrameBuffers();
	void UpdateWorldMatrix();
	void UpdateAccelerationStructure(ID3D12GraphicsCommandList* list);

	void CreateBufferComponents();
	void CreateTexture2DComponents();
	void CreateWorldMatrix();
	void CreateCameras();
	void CreatePointlights();
	void CreatePerObjectBuffers(unsigned int nrOfSubmeshes);
	void CreatePerFrameBuffers();
	void CreateDepthBuffer();
	void CreateRaytracingStructures();

	D3D12_STATIC_SAMPLER_DESC CreateStaticSampler();

	void SetupImgui(HWND windowHandle);
	void DrawImgui(ID3D12GraphicsCommandList* list);

	void SwapFrame() override;

public:
	ModelViewerScene() = default;
	~ModelViewerScene();

	void Initialize(HWND windowHandle, bool fullscreen,
		unsigned int backbufferWidth, unsigned int backbufferHeight,
		IDXGIAdapter* adapter = nullptr);

	void ChangeScreenSize(unsigned int backbufferWidth,
		unsigned int backbufferHeight) override;

	void Update() override;
	void Render() override;
};