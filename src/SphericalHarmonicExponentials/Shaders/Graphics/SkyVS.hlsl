#include "CommonShade.hlsli"

PSInputSky main( float2 position : POSITION )
{
    PSInputSky output;
    
    output.PositionPS = float4( position, 1.0f, 1.0f );
    
    float3 viewPos = mul( gInvProj, float4( position, 0.0f, 1.0f ) ).xyz;
    output.DirectionWS = mul( gInvView, float4( viewPos, 0.0f ) ).xyz;
    
    return output;
}