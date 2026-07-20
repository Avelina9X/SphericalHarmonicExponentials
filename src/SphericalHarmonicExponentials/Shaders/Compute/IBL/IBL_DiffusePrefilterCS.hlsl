#define NUM_SAMPLES 256

cbuffer Constants : register( b0 )
{
    uint2 gGroupCount;
    uint gFaceIndex;
}

TextureCube<float4> gEnvironment : register( t0 );
RWTexture2DArray<float4> gOutput : register( u0 );
SamplerState gLinearWrap : register( s0 );

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

static const uint totalSamples = ( NUM_SAMPLES * NUM_SAMPLES * 4 );
static const float deltaAngle = PI_DIV2 / NUM_SAMPLES;

groupshared float3 gsIrradiance[NUM_SAMPLES];

[numthreads( NUM_SAMPLES, 1, 1 )]
void main(
    uint3 dispatchThreadId : SV_DispatchThreadID,
    uint3 groupThreadId    : SV_GroupThreadID,
    uint3 groupId          : SV_GroupID,
    uint groupIndex        : SV_GroupIndex
)
{
    // Zero our irradiance accumulator
    gsIrradiance[groupIndex] = float3( 0.0f, 0.0f, 0.0f );
    
    // Get UV (-1..1) coordinates of face
    float2 uv = float2( groupId.xy + 0.5 ) / gGroupCount;
    uv = uv * 2 - 1;
    uv.y = -uv.y;
    
    // Calculate normal direction for the face
    float3 normal = normalize( CalculateFaceNormal( uv, gFaceIndex ) );
    
    
    // Calculate orthogonal basis components
    float3 up = float3( 0.0f, 1.0f, 0.0f );
    
    if ( abs( dot( normal, up ) ) > 0.999f )
        up = float3( 1.0f, 0.0f, 0.0f );
    
    float3 right = normalize( cross( up, normal ) );
    up = normalize( cross( normal, right ) );
    
    
    // Precompute texel densitiy for LOD selection
    uint2 sourceResolution;
    gEnvironment.GetDimensions( sourceResolution.x, sourceResolution.y );
    
    float saTexel = 4.0f * PI / ( 6.0f * sourceResolution.x * sourceResolution.y );
    float maxLod = log2( float( sourceResolution.x ) );
    
    // Integrate over hemisphere
    for ( int i = 0; i < NUM_SAMPLES * 4; ++i ) {
        
        float phi = i * deltaAngle; // [0..2Pi]
        float theta = groupIndex * deltaAngle; // [0..Pi/2]
        
        // Calculate sample densitity and LOD
        float saSample = sin( theta ) * deltaAngle * deltaAngle;
        float lod = 0.5f * log2( saSample / saTexel );
        lod = clamp( lod, 0.0f, maxLod );
        
        // Compute Riemann sum sample direction
        float3 tangentSample = float3( sin( theta ) * cos( phi ), sin( theta ) * sin( phi ), cos( theta ) );
        float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;
        
        // Sample cubemap
        float3 sampleCol = gEnvironment.SampleLevel( gLinearWrap, sampleVec, lod ).rgb;
        
        // Accumulate weighted irradiance samples
        gsIrradiance[groupIndex] += sampleCol * cos( theta ) * sin( theta );
    }

    GroupMemoryBarrierWithGroupSync();
    
    // Prefix sum accumulate irradiance
    for ( uint stride = NUM_SAMPLES >> 1; stride > 0; stride >>= 1 ) {
        if ( groupIndex < stride ) {
            gsIrradiance[groupIndex] += gsIrradiance[groupIndex + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }
    
    // Write out weighted irradiance
    if ( groupIndex == 0 ) {
        gOutput[uint3( groupId.xy, gFaceIndex )] = float4( gsIrradiance[groupIndex] * PI * ( 1.0f / totalSamples ), 1.0f );
    }
}