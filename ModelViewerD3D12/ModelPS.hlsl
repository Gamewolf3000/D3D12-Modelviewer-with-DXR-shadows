cbuffer PerObjectComponentIndexBuffer : register(b0, space0)
{
	uint diffuseMapIndex;
	uint specularMapIndex;
	uint normalMapIndex;
};

cbuffer PerFrameComponentIndexBuffer : register(b1, space0)
{
	uint pointlightIndex;
	uint cameraInfoIndex;
};

struct Pointlight
{
	float3 position;
	float range;
	float3 colour;
	float padding;
};

struct CameraInfo
{
	float3 position;
	float padding;
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

SamplerState standardSampler : register(s0);

RaytracingAccelerationStructure scene : register(t0);


static const float PI = 3.14159265359f;

float3 RotateAxis(float3 toRotate, float angle, float3 rotationAxis)
{
	float cosAngle = cos(angle);
	float sinAngle = sin(angle);

	float xy = rotationAxis.x * rotationAxis.y;
	float xz = rotationAxis.x * rotationAxis.z;
	float yz = rotationAxis.y * rotationAxis.z;
	float cosInverse = 1 - cosAngle;

	float3x3 rotationMatrix;
	rotationMatrix[0][0] = cosAngle + pow(rotationAxis.x, 2) * cosInverse;
	rotationMatrix[1][0] = xy * cosInverse - (rotationAxis.z * sinAngle);
	rotationMatrix[2][0] = xz * cosInverse + rotationAxis.y * sinAngle;

	rotationMatrix[0][1] = xy * cosInverse + rotationAxis.z * sinAngle;
	rotationMatrix[1][1] = cosAngle + pow(rotationAxis.y, 2) * cosInverse;
	rotationMatrix[2][1] = yz * cosInverse - rotationAxis.x * sinAngle;

	rotationMatrix[0][2] = xz * cosInverse - rotationAxis.y * sinAngle;
	rotationMatrix[1][2] = yz * cosInverse + rotationAxis.x * sinAngle;
	rotationMatrix[2][2] = cosAngle + pow(rotationAxis.z, 2) * cosInverse;

	return mul(toRotate, rotationMatrix);
}

bool CastShadowRay(float3 origin, float3 direction, float maxLength)
{
	RayQuery<RAY_FLAG_CULL_NON_OPAQUE |
		RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
		RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> query;

	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = 0.0f;
	ray.TMax = maxLength;

	query.TraceRayInline(scene, 0, 0xFF, ray);

	query.Proceed();

	return query.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}

float CalculateShadowFactor(float3 origin, float3 destination, float angleDifference,
	uint raySamples, float lengthFactor)
{
	float3 centerDirection = destination - origin;
	float rayLength = length(centerDirection) * lengthFactor;
	centerDirection = normalize(centerDirection);
	float3 up = float3(0.0f, 1.0f, 0.0f);
	float3 right = cross(centerDirection, up);
	float3 defaultDirection = RotateAxis(centerDirection, angleDifference, right);
	float rotationPerSample = (PI * 2.0f) / raySamples;

	float toReturn = 1.0f;
	float shadowFactor = 1.0f / raySamples;

	for (uint raySample = 0; raySample < raySamples; ++raySample)
	{
		float3 currentRay = RotateAxis(defaultDirection, rotationPerSample * raySample, centerDirection);
		
		if (CastShadowRay(origin, currentRay, rayLength))
			toReturn -= shadowFactor;
	}

	return toReturn;
}

float4 main(VS_OUT input) : SV_TARGET
{
	float4 toReturn;

	StructuredBuffer<Pointlight> lightBuffers = ResourceDescriptorHeap[pointlightIndex];

	float3 ambient = float3(lightBuffers[0].colour * 0.1f);
	float3 diffuse = float3(0.0f, 0.0f, 0.0f);
	float3 specular = float3(0.0f, 0.0f, 0.0f);
	float shininessFactor = 5.0f;

	float3 position = input.worldPosition;
	float3 normal = normalize(input.normal);
	StructuredBuffer<CameraInfo> cameraInfo = ResourceDescriptorHeap[cameraInfoIndex];
	float3 viewVector = normalize(cameraInfo[0].position - position);

	float4 diffuseMaterial = float4(0.0f, 0.0f, 0.0f, 0.0f);
	float3 specularMaterial = float3(0.0f, 0.0f, 0.0f);

	if (diffuseMapIndex != -1)
	{
		Texture2D<float4> diffuseMap = ResourceDescriptorHeap[diffuseMapIndex];
		diffuseMaterial = diffuseMap.Sample(standardSampler, input.uv);

		clip(diffuseMaterial.w < 0.1f ? -1 : 1);

		diffuseMaterial = pow(diffuseMaterial, 2.2f.xxxx); // Gamma correction
	}

	if (specularMapIndex != -1)
	{
		Texture2D<float4> specularMap = ResourceDescriptorHeap[specularMapIndex];
		specularMaterial = specularMap.Sample(standardSampler, input.uv);
		specularMaterial = pow(specularMaterial, 2.2f.xxx); // Gamma correction
	}

	if (normalMapIndex != -1)
	{
		float3 tangent = normalize(input.tangent);
		float3 bitangent = normalize(input.bitangent);
		float3x3 tbn = float3x3(tangent, bitangent, normal);
		Texture2D<float4> normalMap = ResourceDescriptorHeap[normalMapIndex];
		float3 tsNormal = normalMap.Sample(standardSampler, input.uv).xyz;
		tsNormal = tsNormal * 2.0f - 1.0f;
		normal = normalize(mul(tsNormal, tbn));
	}

	float shadowFactor = CalculateShadowFactor(lightBuffers[0].position,
		input.worldPosition, 0.1f * PI / 180.0f, 8, 0.95f);

	float3 lightVector = normalize(lightBuffers[0].position - position);
	float cosA = max(0.0f, dot(lightVector, normal));
	diffuse += lightBuffers[0].colour * cosA;

	float3 reflection = reflect(-lightVector, normal);
	specular += lightBuffers[0].colour * max(0.0f, pow(dot(reflection, viewVector), shininessFactor));

	float3 illumination = (ambient + diffuse * shadowFactor) * diffuseMaterial.xyz +
		specularMaterial * specular * shadowFactor;

	illumination = pow(illumination, (1.0f / 2.0f).xxx); // Gamma correction
	return float4(illumination, 1.0f);
}