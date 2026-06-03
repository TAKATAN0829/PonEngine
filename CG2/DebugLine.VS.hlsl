struct VertexShaderInput
{
	float4 position : POSITION0;
	float4 color : COLOR0;
};

struct VertexShaderOutput
{
	float4 position : SV_POSITION;
	float4 color : COLOR0;
};

struct Camera
{
	float4x4 viewProjection;
};

ConstantBuffer<Camera> gCamera : register(b0);

VertexShaderOutput main(VertexShaderInput input)
{
	VertexShaderOutput output;

	output.position =
        mul(input.position, gCamera.viewProjection);

	output.color = input.color;

	return output;
}