struct TransformationMatrix
{
    float4x4 World;
    float4x4 WVP;
};

struct VSInput
{
    float4 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

cbuffer CameraCB : register(b0)
{
    float4x4 gViewProj;
};

StructuredBuffer<TransformationMatrix> gTransformationMatrices : register(t0);
