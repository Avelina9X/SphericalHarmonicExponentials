#include "CommonShade.hlsli"
#include "../CommonSH.hlsli"
#include "AgX.hlsli"

struct SHCoeffsL2CBV16
{
    uint2 c_1_4_r;
    uint2 c_1_4_g;
    uint2 c_1_4_b;
    
    uint2 c_5_8_r;
    uint2 c_5_8_g;
    uint2 c_5_8_b;
    
    float3 c_0;
    float pad;
    
    uint4 _pad[12];
};

struct SHCoeffsL2L4CBV16
{
    uint2 p_1_4_r;
    uint2 p_1_4_g;
    uint2 p_1_4_b;
    
    uint2 p_5_8_r;
    uint2 p_5_8_g;
    uint2 p_5_8_b;
    
    uint2 p_9_12_r;
    uint2 p_9_12_g;
    uint2 p_9_12_b;
    
    uint2 p_13_16_r;
    uint2 p_13_16_g;
    uint2 p_13_16_b;
    
    uint2 p_17_20_r;
    uint2 p_17_20_g;
    uint2 p_17_20_b;
    
    uint2 p_21_24_r;
    uint2 p_21_24_g;
    uint2 p_21_24_b;
    
    uint2 q_1_4_r;
    uint2 q_1_4_g;
    uint2 q_1_4_b;
    
    uint2 q_5_8_r;
    uint2 q_5_8_g;
    uint2 q_5_8_b;
    
    float3 bias;
    float pad;
    
    uint4 _pad[3];
};

ConstantBuffer<SHCoeffsL2CBV16> gDiffuseSH : register( b1 );
ConstantBuffer<SHCoeffsL2L4CBV16> gSpecularSH : register( b2 );

float4 Unpack4( uint2 v )
{
    return float4(
        f16tof32( v.x ),
        f16tof32( v.x >> 16 ),
        f16tof32( v.y ),
        f16tof32( v.y >> 16 )
    );
}

float3 ReconstructSHL2FromCB( SHBasisL2 shBasis )
{
    float3 acc = float3(
        dot( shBasis.c_1_4, Unpack4( gDiffuseSH.c_1_4_r ) ),
        dot( shBasis.c_1_4, Unpack4( gDiffuseSH.c_1_4_g ) ),
        dot( shBasis.c_1_4, Unpack4( gDiffuseSH.c_1_4_b ) )
    );
    
    acc += float3(
        dot( shBasis.c_5_8, Unpack4( gDiffuseSH.c_5_8_r ) ),
        dot( shBasis.c_5_8, Unpack4( gDiffuseSH.c_5_8_g ) ),
        dot( shBasis.c_5_8, Unpack4( gDiffuseSH.c_5_8_b ) )
    );
    
    return shBasis.c_0 * gDiffuseSH.c_0 + acc;
}

float3 ReconstructSHL2L4FromCB( SHBasisL4 shBasisP, SHBasisL2 shBasisQ )
{    
    float3 acc = float3(
        dot( shBasisP.c_1_4, Unpack4( gSpecularSH.p_1_4_r ) ),
        dot( shBasisP.c_1_4, Unpack4( gSpecularSH.p_1_4_g ) ),
        dot( shBasisP.c_1_4, Unpack4( gSpecularSH.p_1_4_b ) )
    );
    
    acc += float3(
        dot( shBasisP.c_5_8, Unpack4( gSpecularSH.p_5_8_r ) ),
        dot( shBasisP.c_5_8, Unpack4( gSpecularSH.p_5_8_g ) ),
        dot( shBasisP.c_5_8, Unpack4( gSpecularSH.p_5_8_b ) )
    );
    
    acc += float3(
        dot( shBasisP.c_9_12, Unpack4( gSpecularSH.p_9_12_r ) ),
        dot( shBasisP.c_9_12, Unpack4( gSpecularSH.p_9_12_g ) ),
        dot( shBasisP.c_9_12, Unpack4( gSpecularSH.p_9_12_b ) )
    );
    
    acc += float3(
        dot( shBasisP.c_13_16, Unpack4( gSpecularSH.p_13_16_r ) ),
        dot( shBasisP.c_13_16, Unpack4( gSpecularSH.p_13_16_g ) ),
        dot( shBasisP.c_13_16, Unpack4( gSpecularSH.p_13_16_b ) )
    );
    
    acc += float3(
        dot( shBasisP.c_17_20, Unpack4( gSpecularSH.p_17_20_r ) ),
        dot( shBasisP.c_17_20, Unpack4( gSpecularSH.p_17_20_g ) ),
        dot( shBasisP.c_17_20, Unpack4( gSpecularSH.p_17_20_b ) )
    );
    
    acc += float3(
        dot( shBasisP.c_21_24, Unpack4( gSpecularSH.p_21_24_r ) ),
        dot( shBasisP.c_21_24, Unpack4( gSpecularSH.p_21_24_g ) ),
        dot( shBasisP.c_21_24, Unpack4( gSpecularSH.p_21_24_b ) )
    );
    
    
    acc += float3(
        dot( shBasisQ.c_1_4, Unpack4( gSpecularSH.q_1_4_r ) ),
        dot( shBasisQ.c_1_4, Unpack4( gSpecularSH.q_1_4_g ) ),
        dot( shBasisQ.c_1_4, Unpack4( gSpecularSH.q_1_4_b ) )
    );
    
    acc += float3(
        dot( shBasisQ.c_5_8, Unpack4( gSpecularSH.q_5_8_r ) ),
        dot( shBasisQ.c_5_8, Unpack4( gSpecularSH.q_5_8_g ) ),
        dot( shBasisQ.c_5_8, Unpack4( gSpecularSH.q_5_8_b ) )
    );
    
    return exp( shBasisP.c_0 * gSpecularSH.bias + acc );
}

Texture2D<float2> gIntegratedBRDF : register( t0 );
Texture2D<float4> gAbledoTexture : register( t1 );
Texture2D<float2> gNormalTexture : register( t2 );
Texture2D<float4> gORMTexture : register( t3 );

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
    float3 irradiance = ReconstructSHL2FromCB( shBasisD );
    
    SHBasisL4 shBasisP = ComputeSHBasisL4( R );
    SHBasisL2 shBasisQ = ComputeSHBasisL2( H );
    ApplyVonMisesFisher( shBasisP, shBasisQ, roughness * roughness );
    float3 prefilteredColor = ReconstructSHL2L4FromCB( shBasisP, shBasisQ );
    
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