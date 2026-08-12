#include "CommonShade.hlsli"
#include "../CommonSH.hlsli"
#include "AgX.hlsli"

struct SHCoeffsL2HalfCBV16
{
    half4 c_1_4_r;
    half4 c_1_4_g;
    half4 c_1_4_b;
    
    half4 c_5_8_r;
    half4 c_5_8_g;
    half4 c_5_8_b;
    
    float3 c_0;
    float pad;
    
    uint4 _pad[12];
};

struct SHCoeffsL2L4HalfCBV16
{
    half4 p_1_4_r;
    half4 p_1_4_g;
    half4 p_1_4_b;
    
    half4 p_5_8_r;
    half4 p_5_8_g;
    half4 p_5_8_b;
    
    half4 p_9_12_r;
    half4 p_9_12_g;
    half4 p_9_12_b;
    
    half4 p_13_16_r;
    half4 p_13_16_g;
    half4 p_13_16_b;
    
    half4 p_17_20_r;
    half4 p_17_20_g;
    half4 p_17_20_b;
    
    half4 p_21_24_r;
    half4 p_21_24_g;
    half4 p_21_24_b;
    
    half4 q_1_4_r;
    half4 q_1_4_g;
    half4 q_1_4_b;
    
    half4 q_5_8_r;
    half4 q_5_8_g;
    half4 q_5_8_b;
    
    float3 bias;
    float pad;
    
    uint4 _pad[3];
};

ConstantBuffer<SHCoeffsL2HalfCBV16> gDiffuseSH : register( b1 );
ConstantBuffer<SHCoeffsL2L4HalfCBV16> gSpecularSH : register( b2 );

struct SHBasisL2Half16
{
    half4 c_1_4; // band 1 (m=1,0,-1) + band 2 (m=2)
    half4 c_5_8; // band 2 (m=1,0,-1,-2)
    
    float c_0; // band 0
};

struct SHBasisL4Half16
{
    half4 c_1_4; // band 1 (m=1,0,-1) + band 2 (m=2)
    half4 c_5_8; // band 2 (m=1,0,-1,-2)
    half4 c_9_12; // band 3 (m=3,2,1,0)
    half4 c_13_16; // band 3 (m=-1,-2,-3) + band 4 (m=4)
    half4 c_17_20; // band 4 (m=3,2,1,0)
    half4 c_21_24; // band 4 (m=-1,-2,-3,-4)
    
    float c_0; // band 0
};

SHBasisL2Half16 ComputeSHBasisL2Half16( float3 normal )
{
    SHBasisL2Half16 coeffs;

    half x = normal.x, y = normal.y, z = normal.z;
    half x2 = x * x, y2 = y * y, z2 = z * z;

    coeffs.c_0 = 0.282095f;

    coeffs.c_1_4 = half4(
        0.488603 * x, // l=1, m= 1
        0.488603 * z, // l=1, m= 0
        0.488603 * y, // l=1, m=-1
        0.546274 * ( x2 - y2 ) // l=2, m= 2
    );

    coeffs.c_5_8 = half4(
        1.092548 * x * z, // l=2, m= 1
        0.946176 * z2 - 0.315392, // l=2, m= 0
        1.092548 * y * z, // l=2, m=-1
        1.092548 * x * y // l=2, m=-2
    );

    return coeffs;
}

SHBasisL4Half16 ComputeSHBasisL4Half16( float3 normal )
{
    SHBasisL4Half16 coeffs;

    half x = normal.x, y = normal.y, z = normal.z;
    half x2 = x * x, y2 = y * y, z2 = z * z;

    coeffs.c_0 = 0.282095f;

    coeffs.c_1_4 = half4(
        0.488603 * x, // l=1, m= 1
        0.488603 * z, // l=1, m= 0
        0.488603 * y, // l=1, m=-1
        0.546274 * ( x2 - y2 ) // l=2, m= 2
    );

    coeffs.c_5_8 = half4(
        1.092548 * x * z, // l=2, m= 1
        0.946176 * z2 - 0.315392, // l=2, m= 0
        1.092548 * y * z, // l=2, m=-1
        1.092548 * x * y // l=2, m=-2
    );

    coeffs.c_9_12 = half4(
        0.590044 * x * ( x2 - 3.0 * y2 ), // l=3, m= 3
        1.445306 * z * ( x2 - y2 ), // l=3, m= 2
        0.457046 * x * ( 5.0 * z2 - 1.0 ), // l=3, m= 1
        0.373176 * z * ( 5.0 * z2 - 3.0 ) // l=3, m= 0
    );

    coeffs.c_13_16 = half4(
        0.457046 * y * ( 5.0 * z2 - 1.0 ), // l=3, m=-1
        2.890611 * x * y * z, // l=3, m=-2
        0.590044 * y * ( 3.0 * x2 - y2 ), // l=3, m=-3
        0.625836 * ( x2 * ( x2 - 3.0 * y2 ) - y2 * ( 3.0 * x2 - y2 ) ) // l=4, m= 4
    );

    coeffs.c_17_20 = half4(
        1.770131 * x * z * ( x2 - 3.0 * y2 ), // l=4, m= 3
        0.473087 * ( x2 - y2 ) * ( 7.0 * z2 - 1.0 ), // l=4, m= 2
        0.669047 * x * z * ( 7.0 * z2 - 3.0 ), // l=4, m= 1
        0.105786 * ( 35.0 * z2 * z2 - 30.0 * z2 + 3.0 ) // l=4, m= 0
    );

    coeffs.c_21_24 = half4(
        0.669047 * y * z * ( 7.0 * z2 - 3.0 ), // l=4, m=-1
        0.946174 * x * y * ( 7.0 * z2 - 1.0 ), // l=4, m=-2
        1.770131 * y * z * ( 3.0 * x2 - y2 ), // l=4, m=-3
        2.503343 * x * y * ( x2 - y2 ) // l=4, m=-4
    );

    return coeffs;
}

float3 ReconstructSHL2FromCB( SHBasisL2Half16 shBasis )
{
    half3 acc = half3(
        dot( shBasis.c_1_4, gDiffuseSH.c_1_4_r ),
        dot( shBasis.c_1_4, gDiffuseSH.c_1_4_g ),
        dot( shBasis.c_1_4, gDiffuseSH.c_1_4_b )
    );
    
    acc += half3(
        dot( shBasis.c_5_8, gDiffuseSH.c_5_8_r ),
        dot( shBasis.c_5_8, gDiffuseSH.c_5_8_g ),
        dot( shBasis.c_5_8, gDiffuseSH.c_5_8_b )
    );
    
    return shBasis.c_0 * gDiffuseSH.c_0 + acc;
}

void ApplyVonMisesFisherHalf16( inout SHBasisL4Half16 shBasisP, inout SHBasisL2Half16 shBasisQ, half alpha )
{
    half4 l = exp( half4( -1.0, -3.0, -6.0, -10.0 ) * alpha );
    
    shBasisP.c_1_4 *= l.xxxy;
    shBasisQ.c_1_4 *= l.xxxy;
    
    shBasisP.c_5_8 *= l.y;
    shBasisQ.c_5_8 *= l.y;
    
    shBasisP.c_9_12 *= l.z;
    shBasisP.c_13_16 *= l.zzzw;
    shBasisP.c_17_20 *= l.w;
    shBasisP.c_21_24 *= l.w;
}

float3 ReconstructSHL2L4FromCB( SHBasisL4Half16 shBasisP, SHBasisL2Half16 shBasisQ )
{    
    half3 acc = half3(
        dot( shBasisP.c_1_4, gSpecularSH.p_1_4_r ),
        dot( shBasisP.c_1_4, gSpecularSH.p_1_4_g ),
        dot( shBasisP.c_1_4, gSpecularSH.p_1_4_b )
    );
    
    acc += half3(
        dot( shBasisP.c_5_8, gSpecularSH.p_5_8_r ),
        dot( shBasisP.c_5_8, gSpecularSH.p_5_8_g ),
        dot( shBasisP.c_5_8, gSpecularSH.p_5_8_b )
    );
    
    acc += half3(
        dot( shBasisP.c_9_12, gSpecularSH.p_9_12_r ),
        dot( shBasisP.c_9_12, gSpecularSH.p_9_12_g ),
        dot( shBasisP.c_9_12, gSpecularSH.p_9_12_b )
    );
    
    acc += half3(
        dot( shBasisP.c_13_16, gSpecularSH.p_13_16_r ),
        dot( shBasisP.c_13_16, gSpecularSH.p_13_16_g ),
        dot( shBasisP.c_13_16, gSpecularSH.p_13_16_b )
    );
    
    acc += half3(
        dot( shBasisP.c_17_20, gSpecularSH.p_17_20_r ),
        dot( shBasisP.c_17_20, gSpecularSH.p_17_20_g ),
        dot( shBasisP.c_17_20, gSpecularSH.p_17_20_b )
    );
    
    acc += half3(
        dot( shBasisP.c_21_24, gSpecularSH.p_21_24_r ),
        dot( shBasisP.c_21_24, gSpecularSH.p_21_24_g ),
        dot( shBasisP.c_21_24, gSpecularSH.p_21_24_b )
    );
    
    
    acc += half3(
        dot( shBasisQ.c_1_4, gSpecularSH.q_1_4_r ),
        dot( shBasisQ.c_1_4, gSpecularSH.q_1_4_g ),
        dot( shBasisQ.c_1_4, gSpecularSH.q_1_4_b )
    );
    
    acc += half3(
        dot( shBasisQ.c_5_8, gSpecularSH.q_5_8_r ),
        dot( shBasisQ.c_5_8, gSpecularSH.q_5_8_g ),
        dot( shBasisQ.c_5_8, gSpecularSH.q_5_8_b )
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
    
    SHBasisL2Half16 shBasisD = ComputeSHBasisL2Half16( N );
    float3 irradiance = ReconstructSHL2FromCB( shBasisD );
    
    SHBasisL4Half16 shBasisP = ComputeSHBasisL4Half16( R );
    SHBasisL2Half16 shBasisQ = ComputeSHBasisL2Half16( H );
    ApplyVonMisesFisherHalf16( shBasisP, shBasisQ, roughness * roughness );
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