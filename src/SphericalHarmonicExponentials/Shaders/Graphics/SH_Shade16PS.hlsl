#include "CommonShade.hlsli"
#include "../CommonSH.hlsli"
#include "AgX.hlsli"

Texture2D<float2> gIntegratedBRDF : register( t0 );
StructuredBuffer<SHCoeffsL2Half> gDiffuseSH : register( t1 );
StructuredBuffer<SHCoeffsL2L4Half> gSpecularSH : register( t2 );

Texture2D<float4> gAbledoTexture : register( t3 );
Texture2D<float2> gNormalTexture : register( t4 );
Texture2D<float4> gORMTexture : register( t5 );

SamplerState gClampSampler : register( s0 );
SamplerState gSampler : register( s1 );

static const float PI = 3.141592654f;
static const float TAU = 6.283185307f;
static const float PI_DIV2 = 1.570796327f;

float3 fresnelSchlick( float cosTheta, float3 F0 )
{
    return F0 + ( 1.0f - F0 ) * pow( saturate( 1.0f - cosTheta ), 5.0f );
}
float3 fresnelSchlickRoughness( float cosTheta, float3 F0, float roughness )
{
    return F0 + ( max( 1.0f - roughness, F0 ) - F0 ) * pow( saturate( 1.0f - cosTheta ), 5.0f );
}

float4 main( PSInput input ) : SV_TARGET
{
    float3x3 TBN = CalculateTBN( input.PositionWS, normalize( input.NormalWS ), input.UV );
    float3 localNormal = TwoChannelNormalX2( gNormalTexture.Sample( gSampler, input.UV ).xy ) * gNormalStrength;
    float3 N = normalize( mul( localNormal, TBN ) );
    
    float3 V = normalize( gEyePosition - input.PositionWS );
    float3 R = reflect( -V, N );
    float3 H = normalize( N + R );
    
    float3 albedo = gAlbedo * gAbledoTexture.Sample( gSampler, input.UV ).rgb;
    float3 ORM = gORMTexture.Sample( gSampler, input.UV ).rgb;
    
    float ao = ORM.x * gAO;
    float roughness = ORM.y * gRoughness;
    float metallic = ORM.z * gMetallic;
    
    SHBasisL2 shBasisD = ComputeSHBasisL2( N );
    float3 irradiance = ReconstructSHL2HalfFromSB( shBasisD, gDiffuseSH, 0 );
    
    SHBasisL4 shBasisP = ComputeSHBasisL4( R );
    SHBasisL2 shBasisQ = ComputeSHBasisL2( H );
    ApplyVonMisesFisher( shBasisP, shBasisQ, roughness * roughness );
    float3 prefilteredColor = ReconstructSHL2L4HalfFromSB( shBasisP, shBasisQ, gSpecularSH, 0 );
    
    float3 F0 = 0.04f;
    F0 = lerp( F0, albedo, metallic );
    
    float NdotV = saturate( dot( N, V ) );
    
    // IBL
    float3 F = fresnelSchlickRoughness( NdotV, F0, roughness );
    float3 kS = F;
    float3 kD = 1.0f - kS;
    kD *= 1.0f - metallic;
    
    float3 diffuse = irradiance * albedo;
    
    float2 brdf = gIntegratedBRDF.SampleLevel( gClampSampler, float2( NdotV, saturate( roughness ) ), 0.0f );
    
    float3 specular = prefilteredColor * ( F * brdf.x + brdf.y );
    
    float3 ambient = ( kD * diffuse + specular ) * ao;
    
    float3 color = ambient;
    
    // HDR tonemap
    color = agx( ldexp( color, gExposure ) );
    
    // Gamma correct
    color = pow( color, 1.0f / 2.2f );
    
    return float4( color, 1.0f );
}