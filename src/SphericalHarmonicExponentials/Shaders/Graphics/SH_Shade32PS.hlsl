#include "CommonShade.hlsli"
#include "../CommonSH.hlsli"
#include "AgX.hlsli"

Texture2D<float2> gIntegratedBRDF : register( t0 );
StructuredBuffer<SHCoeffsL2> gDiffuseSH : register( t1 );
StructuredBuffer<SHCoeffsL2L4> gSpecularSH : register( t2 );

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
    float3 N = normalize( input.NormalWS );
    float3 V = normalize( gEyePosition - input.PositionWS );
    float3 R = reflect( -V, N );
    float3 H = normalize( N + R );
    
    SHBasisL2 shBasisD = ComputeSHBasisL2( N );
    float3 irradiance = ReconstructSHL2FromSB( shBasisD, gDiffuseSH, 0 );
    
    SHBasisL4 shBasisP = ComputeSHBasisL4( R );
    SHBasisL2 shBasisQ = ComputeSHBasisL2( H );
    ApplyVonMisesFisher( shBasisP, shBasisQ, gRoughness * gRoughness );
    float3 prefilteredColor = ReconstructSHL2L4FromSB( shBasisP, shBasisQ, gSpecularSH, 0 );
    
    float gAO = 1.0f;
    
    float3 F0 = 0.04f;
    F0 = lerp( F0, gAlbedo, gMetallic );
    
    float NdotV = saturate( dot( N, V ) );
    
    // IBL
    float3 F = fresnelSchlickRoughness( NdotV, F0, gRoughness );
    //float3 F = fresnelSchlick( NdotV, F0 );
    float3 kS = F;
    float3 kD = 1.0f - kS;
    kD *= 1.0f - gMetallic;
    
    float3 diffuse = irradiance * gAlbedo;
    
    float2 brdf = gIntegratedBRDF.SampleLevel( gClampSampler, float2( NdotV, saturate( gRoughness ) ), 0.0f );
    
    //float3 specular = prefilteredColor * ( F * brdf.x + brdf.y );
    float3 specular = prefilteredColor * ( F0 * brdf.x + brdf.y );
    //float3 specular = prefilteredColor * F;
    
    float3 ambient = ( kD * diffuse + specular ) * gAO;
    
    float3 color = ambient;
    
    // HDR tonemap
    color = agx( ldexp( color, gExposure ) );
    
    // Gamma correct
    color = pow( color, 1.0f / 2.2f );
    
    return float4( color, 1.0f );
}