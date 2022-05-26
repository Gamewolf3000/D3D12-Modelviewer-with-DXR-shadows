#include "ModelViewerScene.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

using namespace DirectX;

void ModelViewerScene::UpdatePerObjectBuffers()
{
	const Mesh& mesh = meshLoader.GetMeshInfo(0); // We only have one mesh
	for (auto& object : objects)
	{
		const SubMesh& submesh = mesh.subMeshes[object.subMeshIndex];
		VertexShaderPerObjectIndices vsUpload;
		vsUpload.positionIndex = static_cast<unsigned int>(submesh.position +
				resourceComponents.GetComponentDescriptorStart(
				meshLoader.GetPositionComponentIdentifier(), ViewType::SRV));
		vsUpload.uvIndex = static_cast<unsigned int>(submesh.uv +
				resourceComponents.GetComponentDescriptorStart(
					meshLoader.GetUVComponentIdentifier(), ViewType::SRV));
		vsUpload.normalIndex = static_cast<unsigned int>(submesh.normal +
				resourceComponents.GetComponentDescriptorStart(
					meshLoader.GetNormalComponentIdentifier(), ViewType::SRV));

		vsUpload.tangentIndex = static_cast<unsigned int>(submesh.tangent +
			resourceComponents.GetComponentDescriptorStart(
				meshLoader.GetTangentComponentIdentifier(), ViewType::SRV));
		vsUpload.bitangentIndex = static_cast<unsigned int>(submesh.bitangent +
			resourceComponents.GetComponentDescriptorStart(
				meshLoader.GetBitangentComponentIdentifier(), ViewType::SRV));

		vsUpload.indicesIndex = static_cast<unsigned int>(submesh.indices +
			resourceComponents.GetComponentDescriptorStart(
				meshLoader.GetIndicesComponentIdentifier(), ViewType::SRV));

		vsUpload.worldMatrix = static_cast<unsigned int>(worldMatrix +
			resourceComponents.GetComponentDescriptorStart(
				worldMatrixComponent, ViewType::SRV));
		vsUpload.vpMatrix = static_cast<unsigned int>(frontCameraMatrix +
			resourceComponents.GetComponentDescriptorStart(
				cameraMatrixComponent, ViewType::SRV));

		resourceComponents.GetDynamicBufferComponent(
			vertexShaderPerObjectComponent).SetUpdateData(
				object.vertexShaderBuffer, &vsUpload);

		PixelShaderPerObjectIndices psUpload;
		if (submesh.diffuseMap != ResourceIndex(-1))
		{
			psUpload.diffuseMapIndex = static_cast<unsigned int>(
				submesh.diffuseMap +
				resourceComponents.GetComponentDescriptorStart(
					meshLoader.GetDiffuseMapComponentIdentifier(), ViewType::SRV));
		}

		if (submesh.specularMap != ResourceIndex(-1))
		{
			psUpload.specularMapIndex = static_cast<unsigned int>(
				submesh.specularMap +
				resourceComponents.GetComponentDescriptorStart(
					meshLoader.GetSpecularMapComponentIdentifier(), ViewType::SRV));
		}

		if (submesh.normalMap != ResourceIndex(-1))
		{
			psUpload.normalMapIndex = static_cast<unsigned int>(
				submesh.normalMap +
				resourceComponents.GetComponentDescriptorStart(
					meshLoader.GetNormalMapComponentIdentifier(), ViewType::SRV));
		}

		resourceComponents.GetDynamicBufferComponent(
			vertexShaderPerObjectComponent).SetUpdateData(
				object.vertexShaderBuffer, &vsUpload);

		resourceComponents.GetDynamicBufferComponent(
			pixelShaderPerObjectComponent).SetUpdateData(
				object.pixelShaderBuffer, &psUpload);
	}
}

void ModelViewerScene::UpdatePerFrameBuffers()
{
	PixelShaderPerFrameIndices psUpload;

	psUpload.pointlightIndex = static_cast<unsigned int>(
		light + resourceComponents.GetComponentDescriptorStart(
			pointlightComponent, ViewType::SRV));
	psUpload.cameraInfoIndex = static_cast<unsigned int>(
		frontCameraInfo + resourceComponents.GetComponentDescriptorStart(
			cameraInfoComponent, ViewType::SRV));

	resourceComponents.GetDynamicBufferComponent(
		pixelShaderPerFrameComponent).SetUpdateData(
			pixelShaderPerFrame, &psUpload);
}

void ModelViewerScene::UpdateWorldMatrix()
{
	XMFLOAT4X4 toUpload;
	XMMATRIX transformMatrix = XMMatrixRotationY(rotation);
	transformMatrix *= XMMatrixScaling(scaling, scaling, scaling);
	XMStoreFloat4x4(&toUpload, XMMatrixTranspose(transformMatrix));

	resourceComponents.GetDynamicBufferComponent(
		worldMatrixComponent).SetUpdateData(worldMatrix, &toUpload);
}

void ModelViewerScene::UpdateAccelerationStructure(
	ID3D12GraphicsCommandList* list)
{
	XMFLOAT3X4 toUpload;
	XMMATRIX transformMatrix = XMMatrixRotationY(rotation);
	transformMatrix *= XMMatrixScaling(scaling, scaling, scaling);
	XMStoreFloat3x4(&toUpload, transformMatrix);

	SetTopLevelTransform(meshAccelerationStructures.Active(),
		list, toUpload);
}

void ModelViewerScene::CreateBufferComponents()
{
	worldMatrixComponent =
		this->resourceComponents.CreateBufferComponent<XMFLOAT4X4>(true,
			1, 1, UpdateType::MAP_UPDATE, false, true, false);

	cameraMatrixComponent =
		this->resourceComponents.CreateBufferComponent<XMFLOAT4X4>(true,
			1, 1, UpdateType::MAP_UPDATE, false, true, false);
	cameraInfoComponent =
		this->resourceComponents.CreateBufferComponent<CameraInfo>(true,
			1, 1, UpdateType::MAP_UPDATE, false, true, false);

	pointlightComponent =
		this->resourceComponents.CreateBufferComponent<Pointlight>(true,
			1, 1, UpdateType::MAP_UPDATE, false, true, false);

	vertexShaderPerObjectComponent = this->resourceComponents.CreateBufferComponent<
		VertexShaderPerObjectIndices>(true, 100, 100, UpdateType::MAP_UPDATE, false,
			false, false);

	pixelShaderPerObjectComponent = this->resourceComponents.CreateBufferComponent<
		PixelShaderPerObjectIndices>(true, 100, 100, UpdateType::MAP_UPDATE, false,
			false, false);

	pixelShaderPerFrameComponent = this->resourceComponents.CreateBufferComponent<
		PixelShaderPerFrameIndices>(true, 1, 1, UpdateType::MAP_UPDATE, false,
			false, false);
}

void ModelViewerScene::CreateTexture2DComponents()
{
	depthMapComponent =
		this->resourceComponents.CreateTexture2DComponent(true, 99999999, 1, 4,
			DXGI_FORMAT_D32_FLOAT, UpdateType::NONE, false, false, false, true);
}

void ModelViewerScene::CreateWorldMatrix()
{
	worldMatrix = resourceComponents.GetDynamicBufferComponent(
		worldMatrixComponent).CreateBuffer(1);

	if (worldMatrix == ResourceIndex(-1))
		throw std::runtime_error("Could not create world matrix component");

	XMFLOAT4X4 toUpload;
	XMStoreFloat4x4(&toUpload, XMMatrixIdentity());

	resourceComponents.GetDynamicBufferComponent(
		worldMatrixComponent).SetUpdateData(worldMatrix, &toUpload);
}

void ModelViewerScene::CreateCameras()
{
	frontCameraMatrix = resourceComponents.GetDynamicBufferComponent(
		cameraMatrixComponent).CreateBuffer(1);
	frontCameraInfo = resourceComponents.GetDynamicBufferComponent(
		cameraInfoComponent).CreateBuffer(1);

	if (frontCameraMatrix == ResourceIndex(-1))
		throw std::runtime_error("Could not create camera matrix component");
	if (frontCameraInfo == ResourceIndex(-1))
		throw std::runtime_error("Could not create camera info component");

	float position[3] = { 0.0f, 10.0f, -10.0f };
	XMVECTOR mainPos = { position[0], position[1], position[2]};
	XMVECTOR lookAt = mainPos + XMVECTOR({ 0.0f, -1.0f, 1.0f });
	XMMATRIX matrix = XMMatrixLookAtLH(mainPos, lookAt, { 0.0f, 1.0f, 0.0f });
	float aspectRatio = static_cast<float>(screenWidth) / screenHeight;
	matrix *= XMMatrixPerspectiveFovLH(XM_PIDIV2, aspectRatio, 0.1f, 50.0f);

	XMFLOAT4X4 matrixToUpload;
	XMStoreFloat4x4(&matrixToUpload, XMMatrixTranspose(matrix));
	resourceComponents.GetDynamicBufferComponent(
		cameraMatrixComponent).SetUpdateData(frontCameraMatrix, &matrixToUpload);

	CameraInfo infoToUpload = { {position[0], position[1], position[2]} };
	resourceComponents.GetDynamicBufferComponent(
		cameraInfoComponent).SetUpdateData(frontCameraInfo, &infoToUpload);
}

void ModelViewerScene::CreatePointlights()
{
	light = resourceComponents.GetDynamicBufferComponent(
		pointlightComponent).CreateBuffer(1);

	if (light == ResourceIndex(-1))
		throw std::runtime_error("Could not create pointlight component");

	Pointlight toUpload = { {0.0f, 10.0f, 0.0f}, 100.0f, {1.0f, 1.0f, 1.0f} };
	resourceComponents.GetDynamicBufferComponent(
		pointlightComponent).SetUpdateData(light, &toUpload);
}

void ModelViewerScene::CreatePerObjectBuffers(unsigned int nrOfSubmeshes)
{
	for (unsigned int i = 0; i < nrOfSubmeshes; ++i)
	{
		DrawableObject toStore;

		toStore.subMeshIndex = i;
		toStore.vertexShaderBuffer = resourceComponents.GetDynamicBufferComponent(
			vertexShaderPerObjectComponent).CreateBuffer(1);
		toStore.pixelShaderBuffer = resourceComponents.GetDynamicBufferComponent(
			pixelShaderPerObjectComponent).CreateBuffer(1);

		if (toStore.vertexShaderBuffer == ResourceIndex(-1) ||
			toStore.pixelShaderBuffer == ResourceIndex(-1))
		{
			throw std::runtime_error("Could not create shader buffers");
		}

		objects.push_back(toStore);
	}
}

void ModelViewerScene::CreatePerFrameBuffers()
{
	pixelShaderPerFrame = resourceComponents.GetDynamicBufferComponent(
		pixelShaderPerFrameComponent).CreateBuffer(1);
}

void ModelViewerScene::CreateDepthBuffer()
{
	D3D12_CLEAR_VALUE depthClear;
	depthClear.Format = DXGI_FORMAT_D32_FLOAT;
	depthClear.DepthStencil.Depth = 1.0f;
	TextureAllocationInfo allocationInfo(screenWidth, screenHeight, 1, 1, 1, 0,
		&depthClear);
	depthMap = resourceComponents.GetDynamicTexture2DComponent(
		depthMapComponent).CreateTexture(allocationInfo);

	if (depthMap == ResourceIndex(-1))
		throw std::runtime_error("Could not create depth map");
}

void ModelViewerScene::CreateRaytracingStructures()
{
	directAllocators.Active().Reset();
	auto directList = directAllocators.Active().ActiveList();

	auto lambda = [&](AccelerationStructure& structure)
	{
		structure = std::move(CreateAccelerationStructure(device, directList,
			loadedMeshIndex, meshLoader,
			resourceComponents.GetStaticBufferComponent(
				meshLoader.GetPositionComponentIdentifier()),
			resourceComponents.GetStaticBufferComponent(
				meshLoader.GetIndicesComponentIdentifier())));
	};

	meshAccelerationStructures.Initialize(lambda);

	directAllocators.Active().FinishActiveList();
	directAllocators.Active().ExecuteCommands(directQueue);
	endOfFrameFences.Active().Signal(directQueue);
	endOfFrameFences.Active().WaitGPU(directQueue);
}

D3D12_STATIC_SAMPLER_DESC ModelViewerScene::CreateStaticSampler()
{
	D3D12_STATIC_SAMPLER_DESC toReturn;
	toReturn.Filter = D3D12_FILTER_ANISOTROPIC;
	toReturn.AddressU = toReturn.AddressV = toReturn.AddressW =
		D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	toReturn.MipLODBias = 0.0f;
	toReturn.MaxAnisotropy = 16;
	toReturn.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	toReturn.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
	toReturn.MinLOD = 0;
	toReturn.MaxLOD = D3D12_FLOAT32_MAX;
	toReturn.ShaderRegister = 0;
	toReturn.RegisterSpace = 0;
	toReturn.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	return toReturn;
}

void ModelViewerScene::SetupImgui(HWND windowHandle)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	//ImGuiIO& io = ImGui::GetIO();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(windowHandle);

	D3D12_DESCRIPTOR_HEAP_DESC imguiHeapDesc;
	imguiHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	imguiHeapDesc.NumDescriptors = FRAMES;
	imguiHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	imguiHeapDesc.NodeMask = 0;
	HRESULT hr = device->CreateDescriptorHeap(&imguiHeapDesc, 
		IID_PPV_ARGS(&imguiHeap));

	if (FAILED(hr))
		throw std::runtime_error("Could not create ImGui descriptor heap");

	ImGui_ImplDX12_Init(device, FRAMES, DXGI_FORMAT_R8G8B8A8_UNORM,
		imguiHeap, imguiHeap->GetCPUDescriptorHandleForHeapStart(),
		imguiHeap->GetGPUDescriptorHandleForHeapStart());
}

void ModelViewerScene::DrawImgui(ID3D12GraphicsCommandList* list)
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Settings");
	ImGui::SliderFloat("Rotation", &rotation, -XM_PI, XM_PI);
	ImGui::InputFloat("Scaling", &scaling, 0.001f);
	ImGui::InputInt("Sub mesh to render (-1 == all)", &subMeshToRender);
	ImGui::End();

	ImGui::Render();
	list->SetDescriptorHeaps(1, &imguiHeap);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), list);
}

void ModelViewerScene::SwapFrame()
{
	BaseScene::SwapFrame();
	copyAllocators.SwapFrame();
	directAllocators.SwapFrame();
	updateCopyFence.SwapFrame();
	meshAccelerationStructures.SwapFrame();
}

ModelViewerScene::~ModelViewerScene()
{
	FlushAllQueues();

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void ModelViewerScene::Initialize(HWND windowHandle, bool fullscreen,
	unsigned int backbufferWidth, unsigned int backbufferHeight,
	IDXGIAdapter* adapter)
{
	BaseScene::Initialize(windowHandle, fullscreen, backbufferWidth,
		backbufferHeight, 1024 * 1024 * 500, AllocationStrategy::FIRST_FIT,
		adapter);

	DirectoryInformation directoryInformation;
	directoryInformation.meshDirectory = ".\\Resource files\\Sponza-gltf\\glTF\\";
	directoryInformation.diffuseMapDirectory = ".\\Resource files\\Sponza-gltf\\glTF\\";
	directoryInformation.specularMapDirectory = ".\\Resource files\\Sponza-gltf\\glTF\\";
	directoryInformation.normalMapDirectory = ".\\Resource files\\Sponza-gltf\\glTF\\";
	MemoryRequirements neededMemory;
	neededMemory.nrOfVertices = 10000000;
	neededMemory.nrOfMeshes = 200;
	neededMemory.maxNrOfTextures = 1000;
	neededMemory.diffuseMapsTotalBytes = 1000000000;
	neededMemory.specularMapsTotalBytes = 1000000000;
	neededMemory.normalMapsTotalBytes = 1000000000;
	meshLoader.Initialize(directoryInformation, resourceComponents,
		neededMemory);

	GraphicsPipelineData pipelineData;
	pipelineData.shaderPaths[0] = "../x64/Debug/ModelVS.cso";
	pipelineData.shaderPaths[4] = "../x64/Debug/ModelPS.cso";
	pipelineData.rendertargetWidth = backbufferWidth;
	pipelineData.rendertargetHeight = backbufferHeight;
	pipelineData.rootBufferBindings.push_back(
		{ D3D12_SHADER_VISIBILITY_VERTEX, 0 });
	pipelineData.rootBufferBindings.push_back(
		{ D3D12_SHADER_VISIBILITY_PIXEL, 0 });
	pipelineData.rootBufferBindings.push_back(
		{ D3D12_SHADER_VISIBILITY_PIXEL, 1 });
	pipelineData.rootBufferBindings.push_back(
		{ D3D12_SHADER_VISIBILITY_PIXEL, 0, D3D12_ROOT_PARAMETER_TYPE_SRV });
	pipelineData.staticSamplers.push_back(CreateStaticSampler());
	pipelineState.Initialize(device, pipelineData);

	loadedMeshIndex = meshLoader.LoadMesh("Sponza.gltf", resourceComponents);
	if (loadedMeshIndex == MeshIndex(-1))
		throw std::runtime_error("Could not load mesh");

	copyAllocators.Initialize(&ManagedCommandAllocator::Initialize,
		device.Get(), D3D12_COMMAND_LIST_TYPE_COPY);
	directAllocators.Initialize(&ManagedCommandAllocator::Initialize,
		device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
	updateCopyFence.Initialize(&ManagedFence::Initialize, device.Get(), size_t(0));

	CreateBufferComponents();
	CreateTexture2DComponents();
	CreateWorldMatrix();
	CreateCameras();
	CreatePointlights();
	auto& mesh = meshLoader.GetMeshInfo(loadedMeshIndex);
	CreatePerObjectBuffers(static_cast<unsigned int>(mesh.subMeshes.size()));
	CreatePerFrameBuffers();
	CreateDepthBuffer();
	resourceComponents.FinalizeComponents();

	copyAllocators.Active().Reset();
	resourceComponents.UpdateComponents(copyAllocators.Active().ActiveList());
	copyAllocators.Active().FinishActiveList();
	copyAllocators.Active().ExecuteCommands(copyQueue);
	updateCopyFence.Active().Signal(copyQueue);
	updateCopyFence.Active().WaitCPU();
	CreateRaytracingStructures();

	SetupImgui(windowHandle);
}

void ModelViewerScene::ChangeScreenSize(unsigned int backbufferWidth,
	unsigned int backbufferHeight)
{
	BaseScene::ChangeScreenSize(backbufferWidth, backbufferHeight);
	resourceComponents.GetDynamicTexture2DComponent(
		depthMapComponent).RemoveComponent(depthMap);
	CreateDepthBuffer();
	pipelineState.ChangeBackbufferDependent(backbufferWidth,
		backbufferHeight);
}

void ModelViewerScene::Update()
{
	UpdateWorldMatrix();
}

void ModelViewerScene::Render()
{
	if (!PossibleToSwapFrame())
		return; // We could not swap frames because no frame available, no need to render yet

	SwapFrame();

	directAllocators.Active().Reset();
	auto directList = directAllocators.Active().ActiveList();
	resourceComponents.BindComponents(directList);
	UpdatePerObjectBuffers();
	UpdatePerFrameBuffers();
	UpdateAccelerationStructure(directList);

	copyAllocators.Active().Reset();
	resourceComponents.UpdateComponents(copyAllocators.Active().ActiveList());
	copyAllocators.Active().FinishActiveList();
	copyAllocators.Active().ExecuteCommands(copyQueue);
	updateCopyFence.Active().Signal(copyQueue);

	updateCopyFence.Active().WaitGPU(directQueue);
	static std::vector<D3D12_RESOURCE_BARRIER> barriers;
	barriers.clear();
	barriers.push_back(swapChain.TransitionToRenderTarget());
	resourceComponents.GetDynamicTexture2DComponent(
		depthMapComponent).ChangeToState(depthMap, barriers,
			D3D12_RESOURCE_STATE_DEPTH_WRITE);
	directList->ResourceBarrier(static_cast<UINT>(barriers.size()), &barriers[0]);
	swapChain.ClearBackbuffer(directList);
	auto dsvHandle = resourceComponents.GetDynamicTexture2DComponent(
		depthMapComponent).GetDescriptorHeapDSV(depthMap);
	directList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f,
		0, 0, nullptr);
	directAllocators.Active().FinishActiveList(true);
	directAllocators.Active().ExecuteCommands(directQueue);
	directList = directAllocators.Active().ActiveList();

	pipelineState.SetPipelineState(directList); // Do AFTER component binding since root signature MUST be set after descriptor heap is set for direct access
	directList->OMSetRenderTargets(1, &swapChain.Active().rtvHandle, true,
		&dsvHandle);

	directList->SetGraphicsRootConstantBufferView(2,
		resourceComponents.GetDynamicBufferComponent(
			pixelShaderPerFrameComponent).GetVirtualAdress(
				pixelShaderPerFrame));
	directList->SetGraphicsRootShaderResourceView(3,
		meshAccelerationStructures.Active().topLevel.resultBuffer->GetGPUVirtualAddress());

	const Mesh& mesh = meshLoader.GetMeshInfo(0); // We only have one mesh
	for (auto& object : objects)
	{
		if (subMeshToRender != -1 && subMeshToRender != object.subMeshIndex)
			continue;

		directList->SetGraphicsRootConstantBufferView(0,
			resourceComponents.GetDynamicBufferComponent(
				vertexShaderPerObjectComponent).GetVirtualAdress(
					object.vertexShaderBuffer));
		directList->SetGraphicsRootConstantBufferView(1,
			resourceComponents.GetDynamicBufferComponent(
				pixelShaderPerObjectComponent).GetVirtualAdress(
					object.pixelShaderBuffer));
		
		directList->DrawInstanced(mesh.subMeshes[object.subMeshIndex].indexCount,
			1, 0, 0);
	}

	DrawImgui(directAllocators.Active().ActiveList());

	barriers.clear();
	barriers.push_back(swapChain.TransitionToPresent());
	directList->ResourceBarrier(static_cast<UINT>(barriers.size()), &barriers[0]);
	directAllocators.Active().FinishActiveList();
	directAllocators.Active().ExecuteCommands(directQueue);

	swapChain.Present();
	endOfFrameFences.Active().Signal(directQueue);
}
