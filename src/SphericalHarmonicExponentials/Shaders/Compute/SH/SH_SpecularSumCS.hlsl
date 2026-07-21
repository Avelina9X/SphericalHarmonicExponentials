// 9x33 float4
#define nrThreads 297

RWByteAddressBuffer gMatrixAtA_Atb : register( u0 );

cbuffer Constants : register( b0 )
{
    uint gBlocks;
}

[numthreads( nrThreads, 1, 1 )]
void main(
    uint3 dispatchThreadId : SV_DispatchThreadID,
    uint3 groupThreadId    : SV_GroupThreadID,
    uint3 groupId          : SV_GroupID,
    uint groupIndex        : SV_GroupIndex
)
{
    const uint elementOffset = groupIndex * 16;
    
    float4 accumulator = 0.0f;
    
    for ( uint i = 0; i < gBlocks; ++i ) {
        const uint matrixOffset = i * 297 * 16;
        
        accumulator += asfloat( gMatrixAtA_Atb.Load4( elementOffset + matrixOffset ) );
    }

    gMatrixAtA_Atb.Store4( elementOffset, asuint( accumulator ) );
}