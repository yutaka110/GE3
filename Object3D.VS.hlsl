//float4 main(float4 pos : POSITION) : SV_POSITION
//{
//    return pos;
//}
#include "object3d.hlsli"



struct TransformationMatrix
{
   float4x4  WVP;
    float4x4 World;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

//struct VertexShaderOutput
//{
//    float4 position : SV_POSITION;
//    float2 texcoord : TEXCOORD0;
//};

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};



VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float32_t3x3) gTransformationMatrix.World));

    return output;
}

