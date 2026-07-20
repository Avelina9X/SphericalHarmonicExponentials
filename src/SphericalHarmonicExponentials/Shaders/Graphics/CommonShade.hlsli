struct PSInput
{
    float4 PositionPS : SV_Position;
    float3 PositionWS : TEXCOORD0;
    float3 NormalWS : TEXCOORD1;
};

cbuffer Constants : register( b0 )
{
    float3 gEyePosition;
    float4x4 gViewProj;
    float3 gAlbedo;
    float gRoughness;
    float gMetallic;
    float gExposure;
};