#include "CommonShade.hlsli"

#include "AgX.hlsli"

TextureCube<float3> gEnvironment : register( t0 );

SamplerState gSampler : register( s0 );

float4 main( PSInputSky input ) : SV_TARGET
{
    float3 col = gEnvironment.Sample( gSampler, normalize( input.DirectionWS ) );
    
    // HDR tonemap
    col = agx( ldexp( col, gExposure ) );
    
    // Gamma correct
    col = pow( col, 1.0f / 2.2f );
    
    return float4( col, 1.0f );
}