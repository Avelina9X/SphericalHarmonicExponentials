#define NUM_THREADS 1024
#define NUM_SAMPLES_PER_THREAD 4

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

float3 FibonacciSphere( uint i, uint k )
{
    precise float phi = PI * ( 3.0f - sqrt( 5.0f ) );
    precise float y = 1.0f - 2.0f * i / ( k - 1.0f );
    precise float r = min( 1, sqrt( 1.0f - y * y ) );
    precise float theta = phi * i;
    
    return float3( r * cos( theta ), y, r * sin( theta ) );
}

float3 ImportanceSampleGGXAlpha( float2 Xi, float3 N, float a )
{	
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

float VcavityG( float NdotV, float NdotL, float NdotH, float VdotH )
{
    return min( min( 1.0f, 2 * NdotH * NdotV / VdotH ), 2 * NdotH * NdotL / VdotH );
}

float DistributionGGXAlpha( float NdotH, float a )
{
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = ( NdotH2 * ( a2 - 1.0f ) + 1.0f );
    denom = PI * denom * denom;

    return nom / denom;
}

TextureCube<float4> gInput : register( t0 );
RWTexture2DArray<float4> gOutput : register( u0 );
SamplerState gSampler : register( s0 );

cbuffer Constants : register( b0 )
{
    uint2 gResolution;
    uint gRoughnessLevels;
    float gMinRoughness;
    float gMaxRoughness;

    uint gRoughnessIndex;
}

static const uint kTotalSamples = NUM_THREADS * NUM_SAMPLES_PER_THREAD;
static const float gAlpha = lerp( gMinRoughness, gMaxRoughness, gRoughnessIndex / ( gRoughnessLevels - 1.0f ) );

groupshared float3 gsColor[NUM_THREADS];

[numthreads( NUM_THREADS, 1, 1 )]
void main(
    uint3 dispatchThreadId : SV_DispatchThreadID,
    uint3 groupThreadId    : SV_GroupThreadID,
    uint3 groupId          : SV_GroupID,
    uint groupIndex        : SV_GroupIndex
)
{   
    // Zero our radiance accumulator
    gsColor[groupIndex] = 0.0f;
    
    // Compute N and V
    float3 N = FibonacciSphere( groupId.x, gResolution.x );
    float3 V = FibonacciSphere( groupId.y, gResolution.y );
    
    float NdotV = dot( N, V );
    
    [branch]
    if ( NdotV <= 0.0f ) {
        if ( groupIndex == 0 ) {
            gOutput[uint3( groupId.xy, gRoughnessIndex )] = 0.0f;
        }
        return;
    }
    
    uint2 sourceResolution;
    gInput.GetDimensions( sourceResolution.x, sourceResolution.y );
    
    float3 colorAccumulator = 0.0f;
    
    for ( uint i = 0; i < NUM_SAMPLES_PER_THREAD; ++i ) {
        float2 Xi = Hammersley( i + groupIndex * NUM_SAMPLES_PER_THREAD, kTotalSamples );
        float3 H = ImportanceSampleGGXAlpha( Xi, N, gAlpha );
        
        float VdotH = dot( V, H );
        if ( VdotH <= 0.0f )
            continue;
        
        float3 L = 2.0f * VdotH * H - V;
        float NdotL = dot( N, L );
        if ( NdotL <= 0.0f )
            continue;
        
        float NdotH = dot( N, H );
        
        float G = VcavityG( NdotV, NdotL, NdotH, VdotH );
        float weight = G * VdotH / ( NdotV * NdotH );
        
        float D = DistributionGGXAlpha( NdotH, gAlpha );
        float pdf = ( D * NdotH / ( 4.0f * VdotH ) ) + 0.0001f;
        float saTexel = 4.0f * PI / ( 6.0f * sourceResolution.x * sourceResolution.y );
        float saSample = 1.0 / ( kTotalSamples * pdf + 0.0001f );
        float mipLevel = gAlpha == 0.0f ? 0.0f : 0.5 * log2( saSample / saTexel );
        
        float3 radiance = gInput.SampleLevel( gSampler, L, mipLevel ).rgb;
        
        colorAccumulator += radiance * weight;
    }
    
    gsColor[groupIndex] = colorAccumulator;
    
    GroupMemoryBarrierWithGroupSync();
    
    for ( uint stride = NUM_THREADS >> 1; stride > 0; stride >>= 1 ) {
        if ( groupIndex < stride ) {
            gsColor[groupIndex] += gsColor[groupIndex + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }
    
    if ( groupIndex == 0 ) {
        gOutput[uint3( groupId.xy, gRoughnessIndex )] = float4( gsColor[groupIndex] / kTotalSamples, 1.0f );
    }
}