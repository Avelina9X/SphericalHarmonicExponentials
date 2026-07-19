#define NUM_SAMPLES 1024

cbuffer Constants : register( b0 )
{
    uint2 gGroupCount;
}

RWTexture2D<float2> gOutput : register( u0 );

static const float PI = 3.141592654f;
static const float TAU = 6.283185307f;
static const float PI_DIV2 = 1.570796327f;

float RadicalInverse_VdC( uint bits ) 
{
    bits = ( bits << 16u ) | ( bits >> 16u );
    bits = ( ( bits & 0x55555555u ) << 1u ) | ( ( bits & 0xAAAAAAAAu ) >> 1u );
    bits = ( ( bits & 0x33333333u ) << 2u ) | ( ( bits & 0xCCCCCCCCu ) >> 2u );
    bits = ( ( bits & 0x0F0F0F0Fu ) << 4u ) | ( ( bits & 0xF0F0F0F0u ) >> 4u );
    bits = ( ( bits & 0x00FF00FFu ) << 8u ) | ( ( bits & 0xFF00FF00u ) >> 8u );
    return float( bits ) * 2.3283064365386963e-10;
}

float2 Hammersley( uint i, uint N )
{
    return float2( float( i ) / float( N ), RadicalInverse_VdC( i ) );
}

float3 ImportanceSampleGGX( float2 Xi, float3 N, float roughness )
{
    float a = roughness * roughness;
	
    float phi = TAU * Xi.x;
    float cosTheta = min( 1, sqrt( ( 1.0f - Xi.y ) / ( 1.0f + ( a * a - 1.0f ) * Xi.y ) ) ); // Note: the compiler will try to optimzie away the costTheta*cosTheta on the following line. min(1,...) prevents this, and ensures we never get a NaN
    float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );
	
    float3 H;
    H.x = cos( phi ) * sinTheta;
    H.y = sin( phi ) * sinTheta;
    H.z = cosTheta;
	
    float3 up = abs( N.z ) < 0.999 ? float3( 0.0, 0.0, 1.0 ) : float3( 1.0, 0.0, 0.0 );
    float3 tangent = normalize( cross( up, N ) );
    float3 bitangent = ( cross( N, tangent ) );
	
    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize( sampleVec );
}

float GeometrySchlickGGX( float NdotV, float roughness )
{
    float a = roughness;
    float k = ( a * a ) / 2.0f;

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

groupshared float2 gsAccumulator[NUM_SAMPLES];

[numthreads( NUM_SAMPLES, 1, 1 )]
void main(
    uint3 dispatchThreadId : SV_DispatchThreadID,
    uint3 groupThreadId    : SV_GroupThreadID,
    uint3 groupId          : SV_GroupID,
    uint groupIndex        : SV_GroupIndex
)
{
    float NdotV = ( float( groupId.x + 1.0f ) / gGroupCount.x );
    float roughness = ( float( groupId.y + 1.0f ) / gGroupCount.y );
    
    gsAccumulator[groupIndex] = 0.0f;
    
    float3 V;
    V.x = sqrt( 1.0f - NdotV * NdotV );
    V.y = 0.0f;
    V.z = NdotV;

    float3 N = float3( 0.0f, 0.0f, 1.0f );

    float2 Xi = Hammersley( groupIndex, NUM_SAMPLES );
    float3 H = ImportanceSampleGGX( Xi, N, roughness );
    float3 L = normalize( 2.0f * dot( V, H ) * H - V );

    float NdotL = max( L.z, 0.0f );
    float NdotH = max( H.z, 0.0f );
    float VdotH = max( dot( V, H ), 0.0f );

    if ( NdotL > 0.0 ) {
        float G = GeometrySmith( N, V, L, roughness );
        float G_Vis = ( G * VdotH ) / ( NdotH * NdotV );
        float Fc = pow( 1.0f - VdotH, 5.0f );
        
        gsAccumulator[groupIndex].x = ( 1.0f - Fc ) * G_Vis;
        gsAccumulator[groupIndex].y = Fc * G_Vis;
    }
    
    GroupMemoryBarrierWithGroupSync();
    
    // Prefix sum accumulate irradiance
    for ( uint stride = NUM_SAMPLES >> 1; stride > 0; stride >>= 1 ) {
        if ( groupIndex < stride ) {
            gsAccumulator[groupIndex] += gsAccumulator[groupIndex + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }
    
    if ( groupIndex == 0 ) {
        gOutput[groupId.xy] = gsAccumulator[0].xy / NUM_SAMPLES;
    }
}