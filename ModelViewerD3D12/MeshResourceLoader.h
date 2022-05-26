#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>

#include <DirectXMath.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "NSGG Scene\Headers\ManagedResourceComponents.h"
#include "NSGG Core\Headers\StableVector.h"

struct SubMesh
{
	unsigned int indexCount;

	ResourceIndex position = ResourceIndex(-1);
	ResourceIndex uv = ResourceIndex(-1);
	ResourceIndex normal = ResourceIndex(-1);
	ResourceIndex tangent = ResourceIndex(-1);
	ResourceIndex bitangent = ResourceIndex(-1);
	ResourceIndex indices = ResourceIndex(-1);

	ResourceIndex diffuseMap = ResourceIndex(-1);
	ResourceIndex specularMap = ResourceIndex(-1);
	ResourceIndex normalMap = ResourceIndex(-1);
};

struct Mesh
{
	std::vector<SubMesh> subMeshes;
};

enum class TexelFormat
{
	BYTE,
	FLOAT
};

struct DirectoryInformation
{
	std::string meshDirectory;

	std::string diffuseMapDirectory;
	std::string specularMapDirectory;
	std::string normalMapDirectory;
};

struct MemoryRequirements
{
	unsigned int nrOfVertices = static_cast<unsigned int>(-1);
	unsigned int nrOfMeshes = static_cast<unsigned int>(-1);

	unsigned int maxNrOfTextures = static_cast<unsigned int>(-1);
	unsigned int diffuseMapsTotalBytes = static_cast<unsigned int>(-1);
	unsigned int specularMapsTotalBytes = static_cast<unsigned int>(-1);
	unsigned int normalMapsTotalBytes = static_cast<unsigned int>(-1);
};

typedef size_t MeshIndex;

class MeshResourceLoader
{
private:
	StableVector<Mesh> meshes;
	std::unordered_map<std::string, size_t> loadedFiles;

	Assimp::Importer importer;
	unsigned int flags = 0;

	DirectoryInformation directories;

	ComponentIdentifier positionBufferComponent;
	ComponentIdentifier uvBufferComponent;
	ComponentIdentifier normalBufferComponent;
	ComponentIdentifier tangentBufferComponent;
	ComponentIdentifier bitangentBufferComponent;

	ComponentIdentifier indicesBufferComponent;

	ComponentIdentifier diffuseMapTextureComponent;
	ComponentIdentifier specularMapTextureComponent;
	ComponentIdentifier normalMapTextureComponent;

	std::vector<unsigned char> CreateMipData(unsigned char* originalData,
		std::uint8_t componentsPerTexel, TexelFormat texelFormat,
		ID3D12Resource* resource, ResourceIndex index,
		FrameTexture2DComponent<1>& textureComponent);

	bool ProcessMaterial(aiMaterial* material, aiTextureType textureType,
		const std::string& textureDirectory, ResourceIndex& toSet,
		TexelFormat format, std::uint8_t componentsPerTexel,
		FrameTexture2DComponent<1>& textureComponent);

	bool ProcessVertexComponent2(ResourceIndex& toSet, aiVector3D* data,
		unsigned int nrOfComponents, FrameBufferComponent<1>& bufferComponent);
	bool ProcessVertexComponent3(ResourceIndex& toSet, aiVector3D* data,
		unsigned int nrOfComponents, FrameBufferComponent<1>& bufferComponent);

	template<FrameType Frames>
	bool ProcessIndices(SubMesh& subMesh, aiMesh* currentMesh,
		ManagedResourceComponents<Frames>& resourceComponents);

	template<FrameType Frames>
	bool ProcessVertexComponents(SubMesh& subMesh, aiMesh* currentMesh,
		ManagedResourceComponents<Frames>& resourceComponents);

	template<FrameType Frames>
	bool ProcessMaterials(const aiScene* scene, unsigned int materialIndex,
		SubMesh& subMesh, ManagedResourceComponents<Frames>& resourceComponents);

	template<FrameType Frames>
	bool ProcessNode(aiNode* node, const aiScene* scene,
		Mesh& mesh, ManagedResourceComponents<Frames>& resourceComponents);


public:

	template<FrameType Frames>
	void Initialize(const DirectoryInformation& directoryInformation,
		ManagedResourceComponents<Frames>& resourceComponents,
		const MemoryRequirements& neededMemory);

	template<FrameType Frames>
	MeshIndex LoadMesh(const std::string& file,
		ManagedResourceComponents<Frames>& resourceComponents);

	const Mesh& GetMeshInfo(MeshIndex index);

	const ComponentIdentifier& GetPositionComponentIdentifier();
	const ComponentIdentifier& GetUVComponentIdentifier();
	const ComponentIdentifier& GetNormalComponentIdentifier();
	const ComponentIdentifier& GetTangentComponentIdentifier();
	const ComponentIdentifier& GetBitangentComponentIdentifier();
	const ComponentIdentifier& GetIndicesComponentIdentifier();
	const ComponentIdentifier& GetDiffuseMapComponentIdentifier();
	const ComponentIdentifier& GetSpecularMapComponentIdentifier();
	const ComponentIdentifier& GetNormalMapComponentIdentifier();
};

template<FrameType Frames>
inline bool MeshResourceLoader::ProcessIndices(SubMesh& subMesh, 
	aiMesh* currentMesh, ManagedResourceComponents<Frames>& resourceComponents)
{
	std::vector<unsigned int> indices;
	indices.reserve(static_cast<size_t>(currentMesh->mNumFaces) * 3);
	for (unsigned int i = 0; i < currentMesh->mNumFaces; i++)
	{
		aiFace face = currentMesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
			indices.push_back(face.mIndices[j]);
	}

	subMesh.indices = resourceComponents.GetStaticBufferComponent(
		indicesBufferComponent).CreateBuffer(indices.size());

	if (subMesh.indices == ResourceIndex(-1))
		return false;

	subMesh.indexCount = static_cast<unsigned int>(indices.size());
	resourceComponents.GetStaticBufferComponent(
		indicesBufferComponent).SetUpdateData(subMesh.indices, indices.data());

	return true;
}

template<FrameType Frames>
inline bool MeshResourceLoader::ProcessVertexComponents(SubMesh& subMesh,
	aiMesh* currentMesh, ManagedResourceComponents<Frames>& resourceComponents)
{
	if (!ProcessVertexComponent3(subMesh.position, currentMesh->mVertices,
		currentMesh->mNumVertices, resourceComponents.GetStaticBufferComponent(
			positionBufferComponent)))
	{
		return false;
	}
	if (!ProcessVertexComponent2(subMesh.uv, currentMesh->mTextureCoords[0],
		currentMesh->mNumVertices, resourceComponents.GetStaticBufferComponent(
			uvBufferComponent)))
	{
		return false;
	}
	if (!ProcessVertexComponent3(subMesh.normal, currentMesh->mNormals,
		currentMesh->mNumVertices, resourceComponents.GetStaticBufferComponent(
			normalBufferComponent)))
	{
		return false;
	}
	if (!ProcessVertexComponent3(subMesh.tangent, currentMesh->mTangents,
		currentMesh->mNumVertices, resourceComponents.GetStaticBufferComponent(
			tangentBufferComponent)))
	{
		return false;
	}
	if (!ProcessVertexComponent3(subMesh.bitangent, currentMesh->mBitangents,
		currentMesh->mNumVertices, resourceComponents.GetStaticBufferComponent(
			bitangentBufferComponent)))
	{
		return false;
	}

	return true;
}

template<FrameType Frames>
inline bool MeshResourceLoader::ProcessMaterials(const aiScene* scene,
	unsigned int materialIndex, SubMesh& subMesh,
	ManagedResourceComponents<Frames>& resourceComponents)
{
	if (!ProcessMaterial(scene->mMaterials[materialIndex], aiTextureType_DIFFUSE,
		directories.diffuseMapDirectory, subMesh.diffuseMap, TexelFormat::BYTE,
		4, resourceComponents.GetStaticTexture2DComponent(
			diffuseMapTextureComponent)))
	{
		return false;
	}

	if (!ProcessMaterial(scene->mMaterials[materialIndex], aiTextureType_SPECULAR,
		directories.specularMapDirectory, subMesh.specularMap, TexelFormat::BYTE,
		4, resourceComponents.GetStaticTexture2DComponent(
			specularMapTextureComponent)))
	{
		return false;
	}

	if (!ProcessMaterial(scene->mMaterials[materialIndex], aiTextureType_NORMALS,
		directories.normalMapDirectory, subMesh.normalMap, TexelFormat::BYTE,
		4, resourceComponents.GetStaticTexture2DComponent(
			normalMapTextureComponent)))
	{
		return false;
	}

	return true;
}

template<FrameType Frames>
inline bool MeshResourceLoader::ProcessNode(aiNode* node, const aiScene* scene,
	Mesh& mesh, ManagedResourceComponents<Frames>& resourceComponents)
{
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		SubMesh subMesh;
		aiMesh* currentMesh = scene->mMeshes[node->mMeshes[i]];

		if (!ProcessVertexComponents(subMesh, currentMesh, resourceComponents))
			return false;

		if (!ProcessMaterials(scene, currentMesh->mMaterialIndex, subMesh,
			resourceComponents))
		{
			return false;
		}

		if (!ProcessIndices(subMesh, currentMesh, resourceComponents))
			return false;

		mesh.subMeshes.push_back(subMesh);
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		if (!ProcessNode(node->mChildren[i], scene, mesh, resourceComponents))
			return false;
	}

	return true;
}

template<FrameType Frames>
inline void MeshResourceLoader::Initialize(
	const DirectoryInformation& directoryInformation,
	ManagedResourceComponents<Frames>& resourceComponents,
	const MemoryRequirements& neededMemory)
{
	directories = directoryInformation;
	flags = aiProcess_JoinIdenticalVertices |
		aiProcess_Triangulate | aiProcess_ConvertToLeftHanded |
		aiProcess_GenNormals | aiProcess_ValidateDataStructure |
		aiProcess_ImproveCacheLocality | aiProcess_FindInvalidData |
		aiProcess_GenUVCoords | aiProcess_TransformUVCoords |
		aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph |
		aiProcess_CalcTangentSpace | 0;

	unsigned int maximumVertices = neededMemory.nrOfVertices;
	unsigned int maximumBuffers = neededMemory.nrOfMeshes;
	positionBufferComponent =
		resourceComponents.CreateBufferComponent<DirectX::XMFLOAT3>(false,
			maximumVertices, maximumBuffers, UpdateType::INITIALISE_ONLY, false,
			true, false);
	uvBufferComponent =
		resourceComponents.CreateBufferComponent<DirectX::XMFLOAT2>(false,
			maximumVertices, maximumBuffers, UpdateType::INITIALISE_ONLY, false,
			true, false);
	normalBufferComponent =
		resourceComponents.CreateBufferComponent<DirectX::XMFLOAT3>(false,
			maximumVertices, maximumBuffers, UpdateType::INITIALISE_ONLY, false,
			true, false);
	tangentBufferComponent =
		resourceComponents.CreateBufferComponent<DirectX::XMFLOAT3>(false,
			maximumVertices, maximumBuffers, UpdateType::INITIALISE_ONLY, false,
			true, false);
	bitangentBufferComponent =
		resourceComponents.CreateBufferComponent<DirectX::XMFLOAT3>(false,
			maximumVertices, maximumBuffers, UpdateType::INITIALISE_ONLY, false,
			true, false);
	indicesBufferComponent =
		resourceComponents.CreateBufferComponent<unsigned int>(false,
			maximumVertices, maximumBuffers, UpdateType::INITIALISE_ONLY, false,
			true, false);

	diffuseMapTextureComponent =
		resourceComponents.CreateTexture2DComponent(false,
			neededMemory.diffuseMapsTotalBytes, neededMemory.maxNrOfTextures,
			4, DXGI_FORMAT_R8G8B8A8_UNORM, UpdateType::INITIALISE_ONLY, true,
			false, false, false);
	specularMapTextureComponent =
		resourceComponents.CreateTexture2DComponent(false,
			neededMemory.specularMapsTotalBytes, neededMemory.maxNrOfTextures,
			4, DXGI_FORMAT_R8G8B8A8_UNORM, UpdateType::INITIALISE_ONLY, true,
			false, false, false);
	normalMapTextureComponent =
		resourceComponents.CreateTexture2DComponent(false,
			neededMemory.normalMapsTotalBytes, neededMemory.maxNrOfTextures,
			4, DXGI_FORMAT_R8G8B8A8_UNORM, UpdateType::INITIALISE_ONLY, true,
			false, false, false);
}

template<FrameType Frames>
inline MeshIndex MeshResourceLoader::LoadMesh(const std::string& file,
	ManagedResourceComponents<Frames>& resourceComponents)
{
	std::string filepath = directories.meshDirectory + file;
	const aiScene* scene = importer.ReadFile(filepath, flags);
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		return MeshIndex(-1);

	Mesh toStore;
	if (!ProcessNode(scene->mRootNode, scene, toStore, resourceComponents))
		return MeshIndex(-1);

	return meshes.Add(std::move(toStore));
}
