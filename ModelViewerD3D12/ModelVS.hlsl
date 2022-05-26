cbuffer PerObjectComponentIndexBuffer : register(b0, space0)
{
	uint positionIndex;
	uint uvIndex;
	uint normalIndex;

	uint tangentIndex;
	uint bitangentIndex;

	uint indicesIndex;

	unsigned int worldMatrixIndex;
	unsigned int vpMatrixIndex;
};

struct VS_OUT
{
	float4 svPosition : SV_POSITION;
	float3 worldPosition : WORLD_POSITION;
	float2 uv : UV;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 bitangent : BITANGENT;
};

VS_OUT main(uint vertexID : SV_VertexID)
{
	VS_OUT toReturn;

	Buffer<float3> positionBuffer = ResourceDescriptorHeap[positionIndex];
	Buffer<float2> uvBuffer = ResourceDescriptorHeap[uvIndex];
	Buffer<float3> normalBuffer = ResourceDescriptorHeap[normalIndex];
	Buffer<float3> tangentBuffer = ResourceDescriptorHeap[tangentIndex];
	Buffer<float3> bitangentBuffer = ResourceDescriptorHeap[bitangentIndex];
	Buffer<uint> indicesBuffer = ResourceDescriptorHeap[indicesIndex];
	StructuredBuffer<float4x4> worldMatrices = 
		ResourceDescriptorHeap[worldMatrixIndex];
	StructuredBuffer<float4x4> cameraMatrices = 
		ResourceDescriptorHeap[vpMatrixIndex];

	unsigned int index = indicesBuffer[vertexID];
	float4 localPos = float4(positionBuffer[index], 1.0f);
	float2 uv = uvBuffer[index];
	float3 normal = normalBuffer[index];
	float3 tangent = tangentBuffer[index];
	float3 bitangent = bitangentBuffer[index];
	float4x4 worldMatrix = worldMatrices[0];
	float4x4 vpMatrix = cameraMatrices[0];
	toReturn.worldPosition = mul(localPos, worldMatrix);
	toReturn.svPosition = mul(float4(toReturn.worldPosition, 1.0f), vpMatrix);
	toReturn.uv = uv;
	toReturn.normal = mul(normal, worldMatrix);
	toReturn.tangent = mul(tangent, worldMatrix);
	toReturn.bitangent = mul(bitangent, worldMatrix);

	return toReturn;
}