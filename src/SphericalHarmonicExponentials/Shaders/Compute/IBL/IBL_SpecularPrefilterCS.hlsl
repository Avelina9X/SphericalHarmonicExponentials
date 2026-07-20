#define NUM_SAMPLES 1024

cbuffer Constants : register( b0 )
{
    uint2 gResolution;
    uint gFaceIndex;
    float gRoughness;
    float gLODBias;
}

TextureCube<float4> gInput : register( t0 );
RWTexture2DArray<float4> gOutput : register( u0 );
SamplerState gSampler : register( s0 );

static const float PI = 3.141592654f;
static const float TAU = 6.283185307f;
static const float PI_DIV2 = 1.570796327f;

float3 CalculateFaceNormal( float2 uv, uint face )
{
    switch ( face ) {
        case 0: // +X
            return float3( 1, uv.y, -uv.x );

        case 1: // -X
            return float3( -1, uv.y, uv.x );

        case 2: // +Y
            return float3( uv.x, 1, -uv.y );

        case 3: // -Y
            return float3( uv.x, -1, uv.y );

        case 4: // +Z
            return float3( uv.x, uv.y, 1 );

        case 5: // -Z
            return float3( -uv.x, uv.y, -1 );
    }
    return 0.0f;
}

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

float DistributionGGX( float NdotH, float roughness )
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = ( NdotH2 * ( a2 - 1.0f ) + 1.0f );
    denom = PI * denom * denom;

    return nom / denom;
}

groupshared float4 gsColorWeight[NUM_SAMPLES];

[numthreads( NUM_SAMPLES, 1, 1 )]
void main(
    uint3 dispatchThreadId : SV_DispatchThreadID,
    uint3 groupThreadId    : SV_GroupThreadID,
    uint3 groupId          : SV_GroupID,
    uint groupIndex        : SV_GroupIndex
)
{   
    // Zero our irradiance accumulator
    gsColorWeight[groupIndex] = 0.0f;
    
    // Get UV (-1..1) coordinates of face
    float2 uv = float2( groupId.xy + 0.5 ) / gResolution;
    uv = uv * 2 - 1;
    uv.y = -uv.y;
    
    // Calculate normal direction for the face
    float3 N = normalize( CalculateFaceNormal( uv, gFaceIndex ) );
    
    float3 R = N;
    float3 V = N;
    
    float4 colorWeight = 0.0f;
    
    uint2 sourceResolution;
    gInput.GetDimensions( sourceResolution.x, sourceResolution.y );
    
    float saTexel = 4.0f * PI / ( 6.0f * sourceResolution.x * sourceResolution.y );
    
    const uint nrSamplesMultiplier = ( sourceResolution.x / gResolution.x ) * ( sourceResolution.y / gResolution.y );
    const uint totalSamples = NUM_SAMPLES * nrSamplesMultiplier;
    
    for ( uint i = 0; i < nrSamplesMultiplier; ++i ) {
        float2 Xi = Hammersley( i + groupIndex * nrSamplesMultiplier, totalSamples );
        float3 H = ImportanceSampleGGX( Xi, N, gRoughness );
        float3 L = normalize( 2.0f * dot( V, H ) * H - V );
    
        float NdotL = max( dot( N, L ), 0.0f );
        if ( NdotL > 0.0f ) {
            float NdotH = max( dot( N, H ), 0.0f );
            float HdotV = max( dot( H, V ), 0.0f );
        
            float D = DistributionGGX( NdotH, gRoughness );
            float pdf = ( D * NdotH / ( 4.0f * HdotV ) ) + 0.0001f;
            float saSample = 1.0 / ( totalSamples * pdf + 0.0001f );
            float mipLevel = gRoughness == 0.0f ? 0.0f : 0.5 * log2( saSample / saTexel );
            
            colorWeight.rgb += gInput.SampleLevel( gSampler, L, mipLevel + gLODBias ).rgb * NdotL;
            colorWeight.a += NdotL;
        }
    }
    gsColorWeight[groupIndex] = colorWeight;
    
    GroupMemoryBarrierWithGroupSync();
    
    // Prefix sum accumulate color and weight
    for ( uint stride = NUM_SAMPLES >> 1; stride > 0; stride >>= 1 ) {
        if ( groupIndex < stride ) {
            gsColorWeight[groupIndex] += gsColorWeight[groupIndex + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }
    
    if ( groupIndex == 0 ) {
        gOutput[uint3( groupId.xy, gFaceIndex )] = gsColorWeight[groupIndex] / gsColorWeight[groupIndex].a;
    }
}