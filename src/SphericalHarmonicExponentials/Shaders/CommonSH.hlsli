struct SHBasisL2
{
    float4 c_1_4; // band 1 (m=1,0,-1) + band 2 (m=2)
    float4 c_5_8; // band 2 (m=1,0,-1,-2)
    
    float c_0; // band 0
};

struct SHBasisL4
{
    float4 c_1_4; // band 1 (m=1,0,-1) + band 2 (m=2)
    float4 c_5_8; // band 2 (m=1,0,-1,-2)
    float4 c_9_12; // band 3 (m=3,2,1,0)
    float4 c_13_16; // band 3 (m=-1,-2,-3) + band 4 (m=4)
    float4 c_17_20; // band 4 (m=3,2,1,0)
    float4 c_21_24; // band 4 (m=-1,-2,-3,-4)
    
    float c_0; // band 0
};

SHBasisL2 ComputeSHBasisL2( float3 normal )
{
    SHBasisL2 coeffs;

    float x = normal.x, y = normal.y, z = normal.z;
    float x2 = x * x, y2 = y * y, z2 = z * z;

    coeffs.c_0 = 0.282095f;

    coeffs.c_1_4 = float4(
        0.488603f * x, // l=1, m= 1
        0.488603f * z, // l=1, m= 0
        0.488603f * y, // l=1, m=-1
        0.546274f * ( x2 - y2 ) // l=2, m= 2
    );

    coeffs.c_5_8 = float4(
        1.092548f * x * z, // l=2, m= 1
        0.946176f * z2 - 0.315392f, // l=2, m= 0
        1.092548f * y * z, // l=2, m=-1
        1.092548f * x * y // l=2, m=-2
    );

    return coeffs;
}

SHBasisL4 ComputeSHBasisL4( float3 normal )
{
    SHBasisL4 coeffs;

    float x = normal.x, y = normal.y, z = normal.z;
    float x2 = x * x, y2 = y * y, z2 = z * z;

    coeffs.c_0 = 0.282095f;

    coeffs.c_1_4 = float4(
        0.488603f * x, // l=1, m= 1
        0.488603f * z, // l=1, m= 0
        0.488603f * y, // l=1, m=-1
        0.546274f * ( x2 - y2 ) // l=2, m= 2
    );

    coeffs.c_5_8 = float4(
        1.092548f * x * z, // l=2, m= 1
        0.946176f * z2 - 0.315392f, // l=2, m= 0
        1.092548f * y * z, // l=2, m=-1
        1.092548f * x * y // l=2, m=-2
    );

    coeffs.c_9_12 = float4(
        0.590044f * x * ( x2 - 3.0f * y2 ), // l=3, m= 3
        1.445306f * z * ( x2 - y2 ), // l=3, m= 2
        0.457046f * x * ( 5.0f * z2 - 1.0f ), // l=3, m= 1
        0.373176f * z * ( 5.0f * z2 - 3.0f ) // l=3, m= 0
    );

    coeffs.c_13_16 = float4(
        0.457046f * y * ( 5.0f * z2 - 1.0f ), // l=3, m=-1
        2.890611f * x * y * z, // l=3, m=-2
        0.590044f * y * ( 3.0f * x2 - y2 ), // l=3, m=-3
        0.625836f * ( x2 * ( x2 - 3.0f * y2 ) - y2 * ( 3.0f * x2 - y2 ) ) // l=4, m= 4
    );

    coeffs.c_17_20 = float4(
        1.770131f * x * z * ( x2 - 3.0f * y2 ), // l=4, m= 3
        0.473087f * ( x2 - y2 ) * ( 7.0f * z2 - 1.0f ), // l=4, m= 2
        0.669047f * x * z * ( 7.0f * z2 - 3.0f ), // l=4, m= 1
        0.105786f * ( 35.0f * z2 * z2 - 30.0f * z2 + 3.0f ) // l=4, m= 0
    );

    coeffs.c_21_24 = float4(
        0.669047f * y * z * ( 7.0f * z2 - 3.0f ), // l=4, m=-1
        0.946174f * x * y * ( 7.0f * z2 - 1.0f ), // l=4, m=-2
        1.770131f * y * z * ( 3.0f * x2 - y2 ), // l=4, m=-3
        2.503343f * x * y * ( x2 - y2 ) // l=4, m=-4
    );

    return coeffs;
}

struct SHCoeffsL2
{
    row_major float4x3 c_1_4;
    row_major float4x3 c_5_8;
    float3 c_0;
    float pad;
};

float3 ReconstructSHL2( SHBasisL2 shBasis, SHCoeffsL2 shCoeffs )
{
    return mul( shBasis.c_1_4, shCoeffs.c_1_4 ) + mul( shBasis.c_5_8, shCoeffs.c_5_8 ) + ( shBasis.c_0 * shCoeffs.c_0 );
}

float3 ReconstructSHL2FromSB( SHBasisL2 shBasis, StructuredBuffer<SHCoeffsL2> buffer, uint index )
{
    float3 acc = mul( shBasis.c_1_4, buffer[index].c_1_4 );
    acc += mul( shBasis.c_5_8, buffer[index].c_5_8 );
    acc += shBasis.c_0 * buffer[index].c_0;
    
    return acc;
}

SHCoeffsL2 ComputeSHCoeffsL2( SHBasisL2 shBasis, float1x3 L )
{
    SHCoeffsL2 coeffs;

    coeffs.c_1_4 = mul( float4x1( shBasis.c_1_4 ), L );
    coeffs.c_5_8 = mul( float4x1( shBasis.c_5_8 ), L );
    coeffs.c_0 = shBasis.c_0 * L;
    
    return coeffs;
}

struct SHCoeffsL4
{
    row_major float4x3 c_1_4;
    row_major float4x3 c_5_8;
    row_major float4x3 c_9_12;
    row_major float4x3 c_13_16;
    row_major float4x3 c_17_20;
    row_major float4x3 c_21_24;
    float3 c_0;
    float pad;
};

struct SHCoeffsL2L4
{
    row_major float4x3 p_1_4;
    row_major float4x3 p_5_8;
    row_major float4x3 p_9_12;
    row_major float4x3 p_13_16;
    row_major float4x3 p_17_20;
    row_major float4x3 p_21_24;
    
    row_major float4x3 q_1_4;
    row_major float4x3 q_5_8;
    
    float3 c_0;
    float pad;
};


void ApplyVonMisesFisher( inout SHBasisL4 shBasisP, inout SHBasisL2 shBasisQ, float alpha )
{
    float4 l = exp( float4( -1.0f, -3.0f, -6.0f, -10.0f ) * alpha );
    
    shBasisP.c_1_4 *= l.xxxy;
    shBasisQ.c_1_4 *= l.xxxy;
    
    shBasisP.c_5_8 *= l.y;
    shBasisQ.c_5_8 *= l.y;
    
    shBasisP.c_9_12 *= l.z;
    shBasisP.c_13_16 *= l.zzzw;
    shBasisP.c_17_20 *= l.w;
    shBasisP.c_21_24 *= l.w;
}

float3 ReconstructSHL2L4( SHBasisL4 shBasisP, SHBasisL2 shBasisQ, SHCoeffsL2L4 shCoeffs )
{
    return exp(
        mul( shBasisP.c_1_4, shCoeffs.p_1_4 ) + mul( shBasisP.c_5_8, shCoeffs.p_5_8 )
        + mul( shBasisP.c_9_12, shCoeffs.p_9_12 ) + mul( shBasisP.c_13_16, shCoeffs.p_13_16 )
        + mul( shBasisP.c_17_20, shCoeffs.p_17_20 ) + mul( shBasisP.c_21_24, shCoeffs.p_21_24 )
        + mul( shBasisQ.c_1_4, shCoeffs.q_1_4 ) + mul( shBasisQ.c_5_8, shCoeffs.q_5_8 )
        + ( shBasisP.c_0 * shCoeffs.c_0 )
    );
}

float3 ReconstructSHL2L4FromSB( SHBasisL4 shBasisP, SHBasisL2 shBasisQ, StructuredBuffer<SHCoeffsL2L4> buffer, uint index )
{    
    float3 acc = mul( shBasisP.c_1_4, buffer[index].p_1_4 );
    acc += mul( shBasisP.c_5_8, buffer[index].p_5_8 );
    acc += mul( shBasisP.c_9_12, buffer[index].p_9_12 );
    acc += mul( shBasisP.c_13_16, buffer[index].p_13_16 );
    acc += mul( shBasisP.c_17_20, buffer[index].p_17_20 );
    acc += mul( shBasisP.c_21_24, buffer[index].p_21_24 );
    acc += mul( shBasisQ.c_1_4, buffer[index].q_1_4 );
    acc += mul( shBasisQ.c_5_8, buffer[index].q_5_8 );
    acc += shBasisP.c_0 * buffer[index].c_0;

    return exp( acc );
}

void ConstructSHVectorA( float3 N, float3 V, float alpha, out float A[33] )
{
    float3 R = reflect( -V, N );
    float3 H = normalize( N + R );
    
    SHBasisL4 shBasisP = ComputeSHBasisL4( R );
    SHBasisL2 shBasisQ = ComputeSHBasisL2( H );
    
    ApplyVonMisesFisher( shBasisP, shBasisQ, alpha );
    
    [unroll]
    for ( uint i = 0; i < 4; ++i ) {
        A[0 + i] = shBasisP.c_1_4[i];
        A[4 + i] = shBasisP.c_5_8[i];
        A[8 + i] = shBasisP.c_9_12[i];
        A[12 + i] = shBasisP.c_13_16[i];
        A[16 + i] = shBasisP.c_17_20[i];
        A[20 + i] = shBasisP.c_21_24[i];
    }
    
    [unroll]
    for ( uint i = 0; i < 4; ++i ) {
        A[24 + i] = shBasisQ.c_1_4[i];
        A[28 + i] = shBasisQ.c_5_8[i];
    }
    
    A[32] = shBasisP.c_0;
}


struct SHCoeffsL2L4Half
{
    uint3 p[12];
    uint3 q[4];
    float3 bias;
    float pad;
};

struct SHCoeffsL2Half
{
    uint3 c_1_8[4];
    float3 c_0;
    float pad;
};

uint packHalf2( float a, float b )
{
    return f32tof16( a ) | ( f32tof16( b ) << 16 );
}
void PackPair( float3 a, float3 b, out uint3 out3 )
{
    out3[0] = packHalf2( a.x, a.y );
    out3[1] = packHalf2( a.z, b.x );
    out3[2] = packHalf2( b.y, b.z );
}

float4x3 Unpack4Float3( uint3 first, uint3 second )
{
    float4x3 ret;

    ret[0] = float3(
        f16tof32( first.x ),
        f16tof32( first.x >> 16 ),
        f16tof32( first.y )
    );
    
    ret[1] = float3(
        f16tof32( first.y >> 16 ),
        f16tof32( first.z ),
        f16tof32( first.z >> 16 )
    );

    ret[2] = float3(
        f16tof32( second.x ),
        f16tof32( second.x >> 16 ),
        f16tof32( second.y )
    );
    
    ret[3] = float3(
        f16tof32( second.y >> 16 ),
        f16tof32( second.z ),
        f16tof32( second.z >> 16 )
    );
    
    return ret;
}

float3 ReconstructSHL2HalfFromSB( SHBasisL2 shBasis, StructuredBuffer<SHCoeffsL2Half> buffer, uint index )
{
    float3 acc = mul( shBasis.c_1_4, Unpack4Float3( buffer[index].c_1_8[0], buffer[index].c_1_8[1] ) );
    acc += mul( shBasis.c_5_8, Unpack4Float3( buffer[index].c_1_8[2], buffer[index].c_1_8[3] ) );
    acc += shBasis.c_0 * buffer[index].c_0;

    return acc;
}

float3 ReconstructSHL2L4HalfFromSB( SHBasisL4 shBasisP, SHBasisL2 shBasisQ, StructuredBuffer<SHCoeffsL2L4Half> buffer, uint index )
{    
    float3 acc = mul( shBasisP.c_1_4, Unpack4Float3( buffer[index].p[0], buffer[index].p[1] ) );
    acc += mul( shBasisP.c_5_8, Unpack4Float3( buffer[index].p[2], buffer[index].p[3] ) );
    acc += mul( shBasisP.c_9_12, Unpack4Float3( buffer[index].p[4], buffer[index].p[5] ) );
    acc += mul( shBasisP.c_13_16, Unpack4Float3( buffer[index].p[6], buffer[index].p[7] ) );
    acc += mul( shBasisP.c_17_20, Unpack4Float3( buffer[index].p[8], buffer[index].p[9] ) );
    acc += mul( shBasisP.c_21_24, Unpack4Float3( buffer[index].p[10], buffer[index].p[11] ) );
    
    acc += mul( shBasisQ.c_1_4, Unpack4Float3( buffer[index].q[0], buffer[index].q[1] ) );
    acc += mul( shBasisQ.c_5_8, Unpack4Float3( buffer[index].q[2], buffer[index].q[3] ) );
    
    acc += shBasisP.c_0 * buffer[index].bias;

    return exp( acc );
}