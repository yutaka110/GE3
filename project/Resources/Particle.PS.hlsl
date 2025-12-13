Texture2D gParticleTex : register(t0);
SamplerState gSampler : register(s0);

struct PSIn
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
    float4 color : COLOR0;
};

float4 main(PSIn input) : SV_TARGET
{
    float4 tex = gParticleTex.Sample(gSampler, input.texcoord);

    float4 outc = tex * input.color;

    // ★これが重要：加算合成でもフェードするようにRGBをαで弱める
    outc.rgb *= outc.a; // (= tex.a * input.color.a)
    // もしくは outc.rgb *= input.color.a; でもOK

    return outc;
}
