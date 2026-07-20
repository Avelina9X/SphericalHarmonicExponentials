#include "CommonShade.hlsli"

PSInput main( float3 position : POSITION, float3 normal : NORMAL )
{
    PSInput output;
    
    float4 PositionWS = float4( position, 1.0f );
    float4 PositionPS = mul( PositionWS, gViewProj );
    
    output.PositionPS = PositionPS;
    output.PositionWS = PositionWS.xyz;
    output.NormalWS = normal;
    
    return output;
}