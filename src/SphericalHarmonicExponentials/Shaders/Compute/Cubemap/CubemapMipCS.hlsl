#define TILE_SIZE 16

cbuffer Constants : register( b0 )
{
    uint2 gResolution;
}

RWTexture2DArray<float4> gInput : register( u0 );
RWTexture2DArray<float4> gOutput : register( u1 );

[numthreads( TILE_SIZE, TILE_SIZE, 1 )]
void main(
    uint3 dispatchThreadId : SV_DispatchThreadID,
    uint3 groupThreadId    : SV_GroupThreadID,
    uint3 groupId          : SV_GroupID,
    uint groupIndex        : SV_GroupIndex
)
{
    uint2 pixelCoord = dispatchThreadId.xy;
    
    if ( pixelCoord.x * 2 < gResolution.x || pixelCoord.y * 2 < gResolution.y ) {    
        float3 accu = float3( 0.0f, 0.0f, 0.0f );
    
        accu += gInput[uint3( pixelCoord.x * 2, pixelCoord.y * 2, groupId.z )].rgb;
        accu += gInput[uint3( pixelCoord.x * 2 + 1, pixelCoord.y * 2, groupId.z )].rgb;
        accu += gInput[uint3( pixelCoord.x * 2, pixelCoord.y * 2 + 1, groupId.z )].rgb;
        accu += gInput[uint3( pixelCoord.x * 2 + 1, pixelCoord.y * 2 + 1, groupId.z )].rgb;
    
        // Sample from equirectangular texture    
        gOutput[uint3( pixelCoord, groupId.z )] = float4( accu / 4, 1.0f );
    }
}