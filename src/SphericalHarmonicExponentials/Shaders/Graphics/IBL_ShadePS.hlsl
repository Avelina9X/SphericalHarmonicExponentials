#include "CommonShade.hlsli"

float4 main( PSInput input ) : SV_TARGET
{
    return float4( input.NormalWS * 0.5f + 0.5f, 1 );
}