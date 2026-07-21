#define NUM_THREADS 256

#include "../../CommonSH.hlsli"

static const float PI = 3.14159265f;
static const float TAU = PI * 2;
static const float PI_DIV2 = PI / 2;

float3 FibonacciSphere( uint i, uint k )
{
    precise float phi = PI * ( 3.0f - sqrt( 5.0f ) );
    precise float y = 1.0f - 2.0f * i / ( k - 1.0f );
    precise float r = sqrt( 1.0 - y * y );
    precise float theta = phi * i;
    
    return float3( r * cos( theta ), y, r * sin( theta ) );
}

Texture2DArray<float4> gInput : register( t0 );
RWByteAddressBuffer gMatrixAtA_Atb : register( u0 ); // Store 33x33 + 33x3 as 36x33

cbuffer Constants : register( b0 )
{
    uint2 gResolution;
    uint gRoughnessLevels;
    float gMinRoughness;
    float gMaxRoughness;

    uint gRoughnessIndex;
    uint gYIndex;
    
    uint gClear;
}

static const float gAlpha = lerp( gMinRoughness, gMaxRoughness, gRoughnessIndex / ( gRoughnessLevels - 1.0f ) );

[numthreads( NUM_THREADS, 1, 1 )]
void main(
    uint3 dispatchThreadId : SV_DispatchThreadID,
    uint3 groupThreadId    : SV_GroupThreadID,
    uint3 groupId          : SV_GroupID,
    uint groupIndex        : SV_GroupIndex
)
{
    const uint xIndex = dispatchThreadId.x;
    const uint xIndexByte = ( xIndex + gResolution.x * groupId.y ) * 33 * 36 * 4;
    
    float4 linearColor = gInput.Load( uint4( xIndex, gYIndex + groupId.y, gRoughnessIndex, 0 ) );
    
    [branch]
    if ( gClear > 0 ) {
        for ( uint i = 0; i < ( 33 * 36 ) / 4; ++i ) {
            gMatrixAtA_Atb.Store4( xIndexByte + i * 16, asuint( float4( 0.0f, 0.0f, 0.0f, 0.0f ) ) );
        }
    }

    [branch]
    if ( linearColor.a == 0.0f ) {
        // Write 4 floats at a time (16 bytes)
        //for ( uint i = 0; i < ( 33 * 36 ) / 4; ++i ) {
        //    gMatrixAtA_Atb.Store4( xIndexByte + i * 16, asuint( float4( 0.0f, 0.0f, 0.0f, 0.0f ) ) );
        //}
        return;
    }
    
    float3 logColor = log( linearColor.rgb );
    
    // Compute N and V
    float3 N = FibonacciSphere( xIndex, gResolution.x );
    float3 V = FibonacciSphere( gYIndex + groupId.y, gResolution.y );
    
    [branch]
    if ( dot( N, V ) <= 0.0f )
        return;
    
    float A[33];
    ConstructSHVectorA( N, V, gAlpha, A );
    
    // Iterate over rows of A
    for ( uint i = 0; i < 33; ++i ) {
        const uint rowByteIndex = xIndexByte + 36 * 4 * i;
        
        // Write 4 floats at a time for columns 0-31
        for ( uint j = 0; j < 8; ++j ) {
            float4 src = float4( A[j * 4], A[j * 4 + 1], A[j * 4 + 2], A[j * 4 + 3] ) * A[i];
            src += asfloat( gMatrixAtA_Atb.Load4( rowByteIndex + j * 16 ) );
            gMatrixAtA_Atb.Store4( rowByteIndex + j * 16, asuint( src ) );
        }

        // Write column 32 and A[i] * color
        float4 src = float4( A[32], logColor ) * A[i];
        src += asfloat( gMatrixAtA_Atb.Load4( rowByteIndex + 8 * 16 ) );
        gMatrixAtA_Atb.Store4( rowByteIndex + 8 * 16, asuint( src ) );
    }
}