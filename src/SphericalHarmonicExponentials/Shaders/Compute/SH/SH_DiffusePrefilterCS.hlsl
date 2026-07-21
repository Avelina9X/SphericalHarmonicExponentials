#define TILE_SIZE 16

#include "../../CommonSH.hlsli"

cbuffer Constants : register( b0 )
{
    uint2 gGroupCount;
}

TextureCube<float4> gCubemap : register( t0 );
RWStructuredBuffer<SHCoeffsL2> gOutputCoeffs : register( u0 );

SamplerState gLinearWrap : register( s0 );

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

groupshared SHCoeffsL2 gsWeights[TILE_SIZE * TILE_SIZE];

[numthreads( TILE_SIZE, TILE_SIZE, 1 )]
void main(
    uint3 dispatchThreadId : SV_DispatchThreadID,
    uint3 groupThreadId    : SV_GroupThreadID,
    uint3 groupId          : SV_GroupID,
    uint groupIndex        : SV_GroupIndex
)
{
    // Get UV (-1..1) coordinates of face
    float2 uv = float2( dispatchThreadId.xy + 0.5 ) / ( gGroupCount * TILE_SIZE );
    uv = uv * 2 - 1;
    uv.y = -uv.y;
    
    // Tile ID to write out
    uint tileIdx = ( groupId.y * gGroupCount.x + groupId.x ) * 6 + groupId.z;
    
    // Calculate normal direction for the face
    float3 normal = normalize( CalculateFaceNormal( uv, groupId.z ) );
    
    // Sample irradiance
    float3 L = gCubemap.SampleLevel( gLinearWrap, normal, 0 ).rgb;
    
    precise float dOmega = ( 4.0f / ( TILE_SIZE * TILE_SIZE * gGroupCount.x * gGroupCount.y ) ) / pow( 1.0f + uv.x * uv.x + uv.y * uv.y, 1.5f );
    L *= dOmega;
    
    SHBasisL2 shBasis = ComputeSHBasisL2( normal );
    
    gsWeights[groupIndex] = ComputeSHCoeffsL2( shBasis, L );
    
    GroupMemoryBarrierWithGroupSync();
    
    for ( uint stride = ( TILE_SIZE * TILE_SIZE ) >> 1; stride > 0; stride >>= 1 ) {
        if ( groupIndex < stride ) {            
            gsWeights[groupIndex].c_1_4 += gsWeights[groupIndex + stride].c_1_4;
            gsWeights[groupIndex].c_5_8 += gsWeights[groupIndex + stride].c_5_8;
            gsWeights[groupIndex].c_0 += gsWeights[groupIndex + stride].c_0;
        }
        GroupMemoryBarrierWithGroupSync();
    }
    
    if ( groupIndex == 0 ) {
        gOutputCoeffs[tileIdx] = gsWeights[0];
    }
}