struct VertexShaderInput
{
	float32_t4 position : POSITION0;
    float32_t4 color : COLOR0;
};

struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t4 color : COLOR0;
};

struct Camera
{
	float4x4 viewProjection;
};

ConstantBuffer<Camera> gCamera : register(b0);

VertexShaderOutput main(VertexShaderInput input)
{
	VertexShaderOutput output;

	output.position = mul(input.position, gCamera.viewProjection);

	output.color = input.color;

	return output;
}