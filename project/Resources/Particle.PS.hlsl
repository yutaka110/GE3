Texture2D gParticleTex : register(t0);
SamplerState gSampler : register(s0);

struct PSIn
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

float4 main(PSIn input) : SV_TARGET
{
    float4 color = gParticleTex.Sample(gSampler, input.texcoord);

    if (color.a <= 0.01f)
        discard;

    return color;
}
