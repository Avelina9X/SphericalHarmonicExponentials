struct PSInput
{
    float4 PositionPS : SV_Position;
    float3 PositionWS : TEXCOORD0;
    float3 NormalWS : TEXCOORD1;
    float2 UV : TEXCOORD2;
};

cbuffer Constants : register( b0 )
{
    float3 gEyePosition;
    float4x4 gViewProj;
    float3 gAlbedo;
    float gNormalStrength;
    float gAO;
    float gRoughness;
    float gMetallic;
    float gExposure;
};

// Christian Schuler, "Normal Mapping without Precomputed Tangents", ShaderX 5, Chapter 2.6, pp. 131-140
// See also follow-up blog post: http://www.thetenthplanet.de/archives/1180
float3x3 CalculateTBN( float3 p, float3 n, float2 tex )
{
    float3 dp1 = ddx( p );
    float3 dp2 = ddy( p );
    float2 duv1 = ddx( tex );
    float2 duv2 = ddy( tex );

    float3x3 M = float3x3( dp1, dp2, cross( dp1, dp2 ) );
    float2x3 inverseM = float2x3( cross( M[1], M[2] ), cross( M[2], M[0] ) );
    float3 t = normalize( mul( float2( duv1.x, duv2.x ), inverseM ) );
    float3 b = normalize( mul( float2( duv1.y, duv2.y ), inverseM ) );
    return float3x3( t, b, n );
}

float3 PeturbNormal( float3 localNormal, float3 position, float3 normal, float2 texCoord )
{
    const float3x3 TBN = CalculateTBN( position, normal, texCoord );
    return normalize( mul( localNormal, TBN ) );
}

float3 TwoChannelNormalX2( float2 normal )
{
    float2 xy = 2.0f * normal - 1.0f;
    float z = sqrt( 1 - dot( xy, xy ) );
    return float3( xy.x, xy.y, z );
}