#include "Particle.hlsli"

VSOutput main(VSInput input, uint instanceId : SV_InstanceID)
{
    VSOutput output;

    float4x4 wvp = gTransformationMatrices[instanceId].WVP;

    output.position = mul(input.position, wvp);
    output.texcoord = input.texcoord;

    return output;
}
