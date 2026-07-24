#include "CommonShade.hlsli"
#include "AgX.hlsli"

Texture2D<float2> gIntegratedBRDF : register( t0 );
TextureCube<float4> gDiffuseTex : register( t1 );
TextureCube<float4> gSpecularTex : register( t2 );

Texture2D<float4> gAbledoTexture : register( t3 );
Texture2D<float2> gNormalTexture : register( t4 );
Texture2D<float4> gORMTexture : register( t5 );

SamplerState gClampSampler : register( s0 );
SamplerState gSampler : register( s1 );

static const float PI = 3.141592654f;
static const float TAU = 6.283185307f;
static const float PI_DIV2 = 1.570796327f;


float DistributionGGX( float3 N, float3 H, float roughness )
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max( dot( N, H ), 0.0f );
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = ( NdotH2 * ( a2 - 1.0f ) + 1.0f );
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX( float NdotV, float roughness )
{
    float r = ( roughness + 1.0f );
    float k = ( r * r ) / 8.0f;

    float nom = NdotV;
    float denom = NdotV * ( 1.0f - k ) + k;

    return nom / denom;
}

float GeometrySmith( float3 N, float3 V, float3 L, float roughness )
{
    float NdotV = max( dot( N, V ), 0.0f );
    float NdotL = max( dot( N, L ), 0.0f );
    float ggx2 = GeometrySchlickGGX( NdotV, roughness );
    float ggx1 = GeometrySchlickGGX( NdotL, roughness );

    return ggx1 * ggx2;
}

float3 fresnelSchlick( float cosTheta, float3 F0 )
{
    return F0 + ( 1.0f - F0 ) * pow( clamp( 1.0f - cosTheta, 0.0f, 1.0f ), 5.0f );
}
float3 fresnelSchlickRoughness( float cosTheta, float3 F0, float roughness )
{
    return F0 + ( max( 1.0f - roughness, F0 ) - F0 ) * pow( clamp( 1.0f - cosTheta, 0.0f, 1.0f ), 5.0 );
}


float4 main( PSInput input ) : SV_TARGET
{
    float3x3 TBN = CalculateTBN( input.PositionWS, normalize( input.NormalWS ), input.UV );
    float3 localNormal = TwoChannelNormalX2( gNormalTexture.Sample( gSampler, input.UV ).xy ) * gNormalStrength;
    float3 N = normalize( mul( localNormal, TBN ) );
    
    float3 V = normalize( gEyePosition - input.PositionWS );
    float3 R = reflect( -V, N );
    
    float3 albedo = gAlbedo * gAbledoTexture.Sample( gSampler, input.UV ).rgb;
    float3 ORM = gORMTexture.Sample( gSampler, input.UV ).rgb;
    
    float ao = ORM.x * gAO;
    float roughness = ORM.y * gRoughness;
    float metallic = ORM.z * gMetallic;
    
    float3 F0 = 0.04f;
    F0 = lerp( F0, albedo, metallic );
    
    float NdotV = saturate( dot( N, V ) );
    
    // IBL
    float3 F = fresnelSchlickRoughness( NdotV, F0, roughness );
    float3 kS = F;
    float3 kD = 1.0f - kS;
    kD *= 1.0f - metallic;
    
    float3 irradiance = gDiffuseTex.Sample( gSampler, N ).rgb;
    float3 diffuse = irradiance * albedo;
    
    const float MAX_REFLECTION_LOD = 5.0;
    float3 prefilteredColor = gSpecularTex.SampleLevel( gSampler, R, roughness * MAX_REFLECTION_LOD ).rgb;
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