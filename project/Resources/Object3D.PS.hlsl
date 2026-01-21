
//#include"Object3d.hlsli"



//struct Material
//{
//    float32_t4 color;
//    int32_t enableLighting;
//    float32_t4x4 uvTransform;
//    float shininess; // 追加
//    float3 pad_; // 16byte alignment 用
//};

//cbuffer Camera : register(b2)
//{
//    float3 cameraWorldPosition;
//    float padCam;
//}


//ConstantBuffer<Material> gMaterial  : register(b0);
//Texture2D<float4> gTexture : register(t0);
//SamplerState gSampler : register(s0);
//Texture2D<float4> gReceivedTex : register(t4); // 受信画像テクスチャ
//Texture2D<float4> motionMaskTex : register(t2);

//struct PixelShaderOutput
//{
//    float4 color : SV_TARGET0;
//};

//struct DirectionalLight
//{
//    float4 color; // ライトの色
//    float3 direction; // ライトの向き（単位ベクトル）
//    float intensity; // 輝度
//};

//ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);




///*PixelShaderOutput main(VertexShaderOutput input)
//{
//    PixelShaderOutput output;
//    output.color = gMaterial.color;
//    // テクスチャをサンプリングして色を取得
//   // float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
   
//    float4 transformedUV = mul(float32_t4(input.texcoord,0.0f, 1.0f), gMaterial.uvTransform);
//    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
//   // float4 textureColor = gReceivedTex.Sample(gSampler, transformedUV.xy);

//    output.color = gMaterial.color * textureColor;
    
//    if (gMaterial.enableLighting != 0) { // Lightingする場合
//    float NdotL = saturate(dot(normalize(input.normal), -gDirectionalLight.direction));
//    float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
//        output.color = gMaterial.color * gDirectionalLight.color * cos * gDirectionalLight.intensity * textureColor;
//    } else { // Lightingしない場合（前回までと同じ）
//    output.color = gMaterial.color * textureColor;
//}
//    return output;
//}*/

//PixelShaderOutput main(VertexShaderOutput input)
//{
//    PixelShaderOutput output;

//    float4 texColor = gTexture.Sample(gSampler, input.texcoord);
//    float4 maskColor = motionMaskTex.Sample(gSampler, input.texcoord);

//    float3 baseRgb = texColor.rgb * gMaterial.color.rgb;
//    float baseA = texColor.a * gMaterial.color.a;

//    float3 outRgb = baseRgb;

//    if (gMaterial.enableLighting != 0)
//    {
//        float3 N = normalize(input.normal);

//        // direction が「光が進む方向」なら、ピクセルへ向かう光は -direction
//        float3 L = normalize(gDirectionalLight.direction);

//        // Diffuse（あなたが使ってる HalfLambert の形）
//        float NdotL = saturate(dot(N, L));
//        float halfLambert = pow(NdotL * 0.5f + 0.5f, 2.0f);

//        float3 diffuse =
//            baseRgb *
//            gDirectionalLight.color.rgb *
//            halfLambert *
//            gDirectionalLight.intensity;

//        // Specular（Phong）ra
//        float3 V = normalize(input.worldPosition - cameraWorldPosition);

//        // reflect(I,N) の I は入射ベクトル → 入射は -L
//        float3 R = reflect(-L, N);

//        float specPow = pow(saturate(dot(R, V)), gMaterial.shininess);

//        // 鏡面色は白固定（資料と同じ）。後で material化してもOK
//        float3 specular =
//            gDirectionalLight.color.rgb *
//            gDirectionalLight.intensity *
//            specPow;

//        outRgb = diffuse + specular;
//    }

//    // motionMask を赤くブレンドしたいなら（必要ならON）
//    // float3 highlight = float3(1.0f, 0.0f, 0.0f);
//    // outRgb = lerp(outRgb, highlight, maskColor.r);

//    output.color = float4(outRgb, baseA);
//    return output;
//}

#include "Object3d.hlsli"

// ------------------------------------------------------------
// Material / Light
// ------------------------------------------------------------
struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
    float shininess;
    float3 pad_; // 16byte alignment
};

struct DirectionalLight
{
    float4 color; // light color
    float3 direction; // light direction (unit vector)
    float intensity; // brightness
};

cbuffer Camera : register(b2)
{
    float3 cameraWorldPosition;
    float padCam;
}

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// kept for your project (not used here)
Texture2D<float4> gReceivedTex : register(t4);

// motion mask (optional)
Texture2D<float4> motionMaskTex : register(t2);

// ------------------------------------------------------------
// Output
// ------------------------------------------------------------
struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// ------------------------------------------------------------
// Switch (optional)
// 1 = Blinn-Phong (HalfVector)
// 0 = Phong (Reflect)
// ------------------------------------------------------------
#define USE_BLINN_PHONG 1

// ------------------------------------------------------------
// Safe normalize (prevents NaN when vector length is near zero)
// This fixes "weird streak/line" artifacts in highlights.
// ------------------------------------------------------------
static float3 SafeNormalize(float3 v)
{
    float len2 = dot(v, v);
    if (len2 < 1e-8f)
    {
        // return any stable direction; purpose is only to avoid NaN
        return float3(0.0f, 0.0f, 1.0f);
    }
    return v * rsqrt(len2);
}

// ------------------------------------------------------------
// PS Main
// ------------------------------------------------------------
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // UV transform (enable if needed)
    // float4 uv4 = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    // float2 uv = uv4.xy;

    float2 uv = input.texcoord;

    float4 texColor = gTexture.Sample(gSampler, uv);
    float4 maskColor = motionMaskTex.Sample(gSampler, uv);

    float3 baseRgb = texColor.rgb * gMaterial.color.rgb;
    float baseA = texColor.a * gMaterial.color.a;

    float3 outRgb = baseRgb;

    if (gMaterial.enableLighting != 0)
    {
        // Normal (must be in the same space as L and V)
        float3 N = SafeNormalize(input.normal);

        // Light vector L:
        // Here we assume gDirectionalLight.direction is "light -> travels direction" (light -> pixel),
        // so pixel -> light becomes -direction.
        // If lighting looks reversed, flip the sign here.
        float3 L = SafeNormalize(-gDirectionalLight.direction);

        // View vector V (pixel -> camera)
        float3 V = SafeNormalize(cameraWorldPosition - input.worldPosition);

        // Diffuse (Half-Lambert)
        float NdotL = saturate(dot(N, L));
        float halfLambert = pow(NdotL * 0.5f + 0.5f, 2.0f);

        float3 diffuse =
            baseRgb *
            gDirectionalLight.color.rgb *
            halfLambert *
            gDirectionalLight.intensity;

        // Specular
        float specPow = 0.0f;

#if USE_BLINN_PHONG
        // Blinn-Phong: HalfVector
        float3 H = SafeNormalize(L + V); // fixes NaN streaks
        float NdotH = saturate(dot(N, H));
        specPow = pow(NdotH, gMaterial.shininess);
#else
        // Phong: Reflection vector
        float3 R = reflect(-L, N);
        specPow = pow(saturate(dot(R, V)), gMaterial.shininess);
#endif

        // Specular color (white like the slide)
        float3 specular =
            gDirectionalLight.color.rgb *
            gDirectionalLight.intensity *
            specPow;

        outRgb = diffuse + specular;
    }

    // Optional: blend motion mask as red highlight
    // float3 highlight = float3(1.0f, 0.0f, 0.0f);
    // outRgb = lerp(outRgb, highlight, maskColor.r);

    output.color = float4(outRgb, baseA);
    return output;
}
