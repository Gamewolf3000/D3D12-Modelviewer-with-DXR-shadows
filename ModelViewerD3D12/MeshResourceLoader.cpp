#include "MeshResourceLoader.h"

#include <stdexcept>

#include <DirectXMath.h>
using namespace DirectX;

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#pragma warning(disable:4996)
#include "stb_image_write.h"
#include "stb_image.h"
#include "NSGG Core\Headers\FrameTexture2DComponent.h"
#include "NSGG Core\Headers\FrameBufferComponent.h"

template<typename ComponentType, typename CombinationType>
void GenerateMipData(std::vector<unsigned char>& data, std::vector<UINT> rows,
	std::vector<UINT64> rowSizes, UINT16 currentMipLevel, size_t parentDataStart, 
	std::uint8_t componentsPerTexel, ResourceIndex index, 
	FrameTexture2DComponent<1>& textureComponent)
{
	std::uint8_t texelSize = componentsPerTexel * sizeof(ComponentType);
	size_t texelsPerRow = rowSizes[currentMipLevel] / texelSize;
	size_t parentComponentsPerRow = componentsPerTexel * (rowSizes[currentMipLevel - 1] / texelSize);
	ComponentType* parentData = reinterpret_cast<ComponentType*>(data.data() + parentDataStart);
	ComponentType* currentData = parentData;
	currentData += rows[currentMipLevel - 1] * (rowSizes[currentMipLevel - 1] / sizeof(ComponentType));

	for (UINT row = 0; row < rows[currentMipLevel]; ++row)
	{
		for (UINT64 rowTexel = 0; rowTexel < texelsPerRow; ++rowTexel)
		{
			for (std::uint8_t component = 0; component < componentsPerTexel; ++component)
			{
				ComponentType topLeft = *parentData;
				ComponentType bottomLeft = ComponentType(0);
				ComponentType topRight = ComponentType(0);
				ComponentType bottomRight = ComponentType(0);
				std::uint8_t divider = 1;

				if (row + 1 < rows[currentMipLevel])
				{
					bottomLeft = *(parentData + parentComponentsPerRow);
					divider += 1;
				}

				if (rowTexel + 1 < texelsPerRow)
				{
					topRight = *(parentData + componentsPerTexel);
					divider += 1;
				}
				
				if (row + 1 < rows[currentMipLevel] && rowTexel + 1 < texelsPerRow)
				{
					bottomRight = *(parentData + parentComponentsPerRow + componentsPerTexel);
					divider += 1;
				}

				CombinationType combinedComponents = topLeft + topRight +
					bottomLeft + bottomRight;
				combinedComponents /= divider;

				*currentData = static_cast<ComponentType>(combinedComponents);
				currentData += 1;
				parentData += 1;
			}

			parentData += componentsPerTexel;
		}

		if (rows[currentMipLevel] % 2 == 1)
			parentData += componentsPerTexel;

		parentData += parentComponentsPerRow;
	}

	size_t dataStart = parentDataStart +
		rows[currentMipLevel - 1] * rowSizes[currentMipLevel - 1];
	textureComponent.SetUpdateData(index, data.data() + dataStart,
		static_cast<std::uint8_t>(currentMipLevel));

	if (currentMipLevel + 1 < rows.size())
	{
		size_t nextLevelDataStart = parentDataStart + 
			rows[currentMipLevel - 1] * rowSizes[currentMipLevel - 1];
		GenerateMipData<ComponentType, CombinationType>(data, rows, rowSizes,
			currentMipLevel + 1, nextLevelDataStart, componentsPerTexel,
			index, textureComponent);
	}
}

std::vector<unsigned char> MeshResourceLoader::CreateMipData(unsigned char* originalData,
	std::uint8_t componentsPerTexel, TexelFormat texelFormat, ID3D12Resource* resource,
	ResourceIndex index, FrameTexture2DComponent<1>& textureComponent)
{
	auto desc = resource->GetDesc();
	ID3D12Device* device = nullptr;
	HRESULT hr = resource->GetDevice(IID_PPV_ARGS(&device));

	if (FAILED(hr) || device == nullptr)
		throw std::runtime_error("Could not generate mip maps due to failure to get device");

	std::vector<UINT> rows(desc.MipLevels);
	std::vector<UINT64> rowSizes(desc.MipLevels);
	device->GetCopyableFootprints(&desc, 0, desc.MipLevels, 0,
		nullptr, rows.data(), rowSizes.data(), nullptr);
	size_t totalSize = 0;

	for (UINT16 mipLevel = 0; mipLevel < desc.MipLevels; ++mipLevel)
		totalSize += rows[mipLevel] * rowSizes[mipLevel];

	std::vector<unsigned char> toReturn(totalSize);
	memcpy(toReturn.data(), originalData, rows[0] * rowSizes[0]);
	textureComponent.SetUpdateData(index, toReturn.data(), 0);

	if (texelFormat == TexelFormat::BYTE)
	{
		GenerateMipData<std::uint8_t, std::uint16_t>(toReturn, rows,
			rowSizes, 1, 0, componentsPerTexel, index, textureComponent);
	}
	else if (texelFormat == TexelFormat::FLOAT)
	{
		GenerateMipData<float, double>(toReturn, rows, rowSizes,
			1, 0, componentsPerTexel, index, textureComponent);
	}
	else
	{
		throw std::runtime_error("Unknown texel format when generating mip maps!");
	}
	
	device->Release();
	return toReturn;
}

bool MeshResourceLoader::ProcessMaterial(aiMaterial* material,
	aiTextureType textureType, const std::string& textureDirectory,
	ResourceIndex& toSet, TexelFormat format, std::uint8_t componentsPerTexel,
	FrameTexture2DComponent<1>& textureComponent)
{
	unsigned int textureCount = material->GetTextureCount(textureType);
	if (textureCount == 0) // No texture of this type, this is fine
		return true;

	aiString name;
	material->GetTexture(textureType, 0, &name);

	std::string filepath = textureDirectory + name.C_Str();
	int width = 0, height = 0;
	void* data = nullptr;
	std::uint8_t componentSize = 0;

	switch (format)
	{
	case TexelFormat::BYTE:
		data = stbi_load(filepath.c_str(), &width, &height,
			nullptr, componentsPerTexel);
		componentSize = 1;
		break;
	case TexelFormat::FLOAT:
		data = stbi_loadf(filepath.c_str(), &width, &height,
			nullptr, componentsPerTexel);
		componentSize = 4;
		break;
	default:
		break;
	}

	if (data == nullptr)
		return false;

	TextureAllocationInfo allocationInfo(width, height, 1, 0);
	toSet = textureComponent.CreateTexture(allocationInfo);
	if (toSet == ResourceIndex(-1))
		return false;

	auto handle = textureComponent.GetTextureHandle(toSet);
	auto mipMappedData = CreateMipData(static_cast<unsigned char*>(data), 
		componentsPerTexel, format, handle.resource, toSet, textureComponent);

	STBI_FREE(data);

	return true;
}

bool MeshResourceLoader::ProcessVertexComponent2(ResourceIndex& toSet,
	aiVector3D* data, unsigned int nrOfComponents,
	FrameBufferComponent<1>& bufferComponent)
{
	if (data == nullptr)
		return true;

	static std::vector<XMFLOAT2> temp;
	temp.clear();
	temp.resize(nrOfComponents);

	for (unsigned int i = 0; i < nrOfComponents; ++i)
		temp[i] = XMFLOAT2(data[i].x, data[i].y);

	toSet = bufferComponent.CreateBuffer(nrOfComponents);

	if (toSet == ResourceIndex(-1))
		return false;

	bufferComponent.SetUpdateData(toSet, temp.data());

	return true;
}

bool MeshResourceLoader::ProcessVertexComponent3(ResourceIndex& toSet, 
	aiVector3D* data, unsigned int nrOfComponents,
	FrameBufferComponent<1>& bufferComponent)
{
	if (data == nullptr)
		return true;

	static std::vector<XMFLOAT3> temp;
	temp.clear();
	temp.resize(nrOfComponents);

	for (unsigned int i = 0; i < nrOfComponents; ++i)
		temp[i] = XMFLOAT3(data[i].x, data[i].y, data[i].z);

	toSet = bufferComponent.CreateBuffer(nrOfComponents);

	if (toSet == ResourceIndex(-1))
		return false;

	bufferComponent.SetUpdateData(toSet, temp.data());

	return true;
}

const Mesh& MeshResourceLoader::GetMeshInfo(MeshIndex index)
{
	return meshes[index];
}

const ComponentIdentifier& MeshResourceLoader::GetPositionComponentIdentifier()
{
	return positionBufferComponent;
}

const ComponentIdentifier& MeshResourceLoader::GetUVComponentIdentifier()
{
	return uvBufferComponent;
}

const ComponentIdentifier& MeshResourceLoader::GetNormalComponentIdentifier()
{
	return normalBufferComponent;
}

const ComponentIdentifier& MeshResourceLoader::GetTangentComponentIdentifier()
{
	return tangentBufferComponent;
}

const ComponentIdentifier& MeshResourceLoader::GetBitangentComponentIdentifier()
{
	return bitangentBufferComponent;
}

const ComponentIdentifier& MeshResourceLoader::GetIndicesComponentIdentifier()
{
	return indicesBufferComponent;
}

const ComponentIdentifier& MeshResourceLoader::GetDiffuseMapComponentIdentifier()
{
	return diffuseMapTextureComponent;
}

const ComponentIdentifier& MeshResourceLoader::GetSpecularMapComponentIdentifier()
{
	return specularMapTextureComponent;
}

const ComponentIdentifier& MeshResourceLoader::GetNormalMapComponentIdentifier()
{
	return normalMapTextureComponent;
}
