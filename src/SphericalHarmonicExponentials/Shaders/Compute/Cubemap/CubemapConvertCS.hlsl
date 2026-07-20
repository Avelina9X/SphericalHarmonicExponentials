#define TILE_SIZE 16

cbuffer Constants : register( b0 )
{
    uint2 gResolution;
    float gClampValue;
}

Texture2D<float4> gEnvironment : register( t0 );
RWTexture2DArray<float4> gOutput : register( u0 );
SamplerState gLinearClamp : register( s0 );

static const float PI = 3.141592654f;
static const float TAU = 6.283185307f;
static const float PI_DIV2 = 1.570796327f;

float2 DirToUV( float3 dir )
{
    return float2(
        1.0f - ( atan2( dir.x, dir.z ) / TAU + 0.5f ), // RHS to LHS conversion for cubemaps
        acos( dir.y ) / PI
    );
}

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

[numthreads( TILE_SIZE, TILE_SIZE, 1 )]
void main(
    uint3 dispatchThreadId : SV_DispatchThreadID,
    uint3 groupThreadId    : SV_GroupThreadID,
    uint3 groupId          : SV_GroupID,
    uint groupIndex        : SV_GroupIndex
)
{
    uint2 pixelCoord = dispatchThreadId.xy;
    
    // Get UV (-1..1) coordinates of face
    float2 uv = float2( pixelCoord.xy + 0.5 ) / gResolution;
    uv = uv * 2 - 1;
    uv.y = -uv.y;
    
    // Calculate normal direction for the face
    float3 normal = normalize( CalculateFaceNormal( uv, groupId.z ) );
    
    // Sample from equirectangular texture
    float4 color = gEnvironment.SampleLevel( gLinearClamp, DirToUV( normal ), 0 );
    
    // Apply optional clamping
    if ( gClampValue > 0.0f ) {
        color = min( color, gClampValue );
    }
    
    gOutput[uint3( pixelCoord, groupId.z )] = color;

}