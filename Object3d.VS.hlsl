struct TransformmationMatrix{
    float32_t4x4 WVP;
};
ConstantBuffer<TransformmationMatrix> gTransformationMatrix : register(b0);
struct VertexShaderOutput
{
    float4 position : SV_POSITION;
};

struct VertexShaderInput
{
    float4 position : POSITION;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(input.position, gTransformationMatrix.WVP);
    return output;
}