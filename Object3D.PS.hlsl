
#include"Object3d.hlsli"



struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
};

ConstantBuffer<Material> gMaterial  : register(b0);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);
Texture2D<float4> gReceivedTex : register(t4); // 受信画像テクスチャ
Texture2D<float4> motionMaskTex : register(t2);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

struct DirectionalLight
{
    float4 color; // ライトの色
    float3 direction; // ライトの向き（単位ベクトル）
    float intensity; // 輝度
};

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);




/*PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    output.color = gMaterial.color;
    // テクスチャをサンプリングして色を取得
   // float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
   
    float4 transformedUV = mul(float32_t4(input.texcoord,0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
   // float4 textureColor = gReceivedTex.Sample(gSampler, transformedUV.xy);

    output.color = gMaterial.color * textureColor;
    
    if (gMaterial.enableLighting != 0) { // Lightingする場合
    float NdotL = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
    float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        output.color = gMaterial.color * gDirectionalLight.color * cos * gDirectionalLight.intensity * textureColor;
    } else { // Lightingしない場合（前回までと同じ）
    output.color = gMaterial.color * textureColor;
}
    return output;
}*/

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // UV座標に対してテクスチャとマスクをサンプリング
    float4 texColor = gTexture.Sample(gSampler, input.texcoord);
    float4 maskColor = motionMaskTex.Sample(gSampler, input.texcoord);

    float4 litColor = texColor;

    // ライティングを適用する場合
    if (gMaterial.enableLighting != 0)
    {
        float NdotL = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
        float lighting = pow(NdotL * 0.5f + 0.5f, 2.0f);
        litColor = texColor * gMaterial.color * gDirectionalLight.color * lighting * gDirectionalLight.intensity;
    }
    else
    {
        litColor = texColor * gMaterial.color;
    }

    // マスクがあるところは赤くブレンド表示（Rのみ強調）
    //float4 highlight = float4(1.0f, 0.0f, 0.0f, maskColor.r); // 赤
    //output.color = lerp(litColor, highlight, maskColor.r);
    
    output.color=texColor * gMaterial.color;
    return output;
}
