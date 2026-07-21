#include "../../CommonSH.hlsli"

cbuffer Constants : register( b0 )
{
    uint2 gGroupCount;
}

StructuredBuffer<SHCoeffsL2> gInputCoeffs : register( t0 );
RWStructuredBuffer<SHCoeffsL2> gAccuCoeffs : register( u0 );
RWStructuredBuffer<SHCoeffsL2Half> gAccuCoeffsHalf : register( u1 );

[numthreads( 1, 1, 1 )]
void main(
    uint3 dispatchThreadId : SV_DispatchThreadID,
    uint3 groupThreadId    : SV_GroupThreadID,
    uint3 groupId          : SV_GroupID,
    uint groupIndex        : SV_GroupIndex
)
{
    const uint totalGroups = gGroupCount.x * gGroupCount.y * 6;
    
    SHCoeffsL2 accumulator = (SHCoeffsL2) 0;
    
    // Sum all coefficients
    for ( uint i = 0; i < totalGroups; ++i ) {
        accumulator.c_1_4 += gInputCoeffs[i].c_1_4;
        accumulator.c_5_8 += gInputCoeffs[i].c_5_8;
        accumulator.c_0 += gInputCoeffs[i].c_0;
    }
    
    // Write FP32 coefficients
    gAccuCoeffs[0] = accumulator;
    
    // Write FP16 coefficients
    PackPair( accumulator.c_1_4[0], accumulator.c_1_4[1], gAccuCoeffsHalf[0].c_1_8[0] );
    PackPair( accumulator.c_1_4[2], accumulator.c_1_4[3], gAccuCoeffsHalf[0].c_1_8[1] );
    PackPair( accumulator.c_5_8[0], accumulator.c_5_8[1], gAccuCoeffsHalf[0].c_1_8[2] );
    PackPair( accumulator.c_5_8[2], accumulator.c_5_8[3], gAccuCoeffsHalf[0].c_1_8[3] );
    gAccuCoeffsHalf[0].c_0 = accumulator.c_0;
}