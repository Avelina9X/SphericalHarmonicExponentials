// 33 threads matching our matrix dimensions
#define MATRIX_SIZE 33

#include "../../CommonSH.hlsli"

// Store 33x33 + 33x3 as 36x33
ByteAddressBuffer gMatrixAtA_Atb : register( t0 );

RWStructuredBuffer<SHCoeffsL2L4> gHarmonics32 : register( u0 );
RWStructuredBuffer<SHCoeffsL2L4Half> gHarmonics16 : register( u1 );

groupshared float A_shared[MATRIX_SIZE][MATRIX_SIZE];
groupshared float3 b_shared[MATRIX_SIZE];
groupshared float L_shared[MATRIX_SIZE][MATRIX_SIZE];

[numthreads( MATRIX_SIZE, 1, 1 )]
void main( uint3 gtid : SV_GroupThreadID )
{
    int row = gtid.x;
    
    const uint rowByte = 36 * 4 * row;

    // STEP 1: Cooperatively load data from global GPU buffers into shared memory
    for ( int col = 0; col < MATRIX_SIZE; ++col )
    {
        const uint colByte = col * 4;
        A_shared[row][col] = asfloat( gMatrixAtA_Atb.Load( rowByte + colByte ) );
    }
    b_shared[row] = asfloat( gMatrixAtA_Atb.Load3( rowByte + 33 * 4 ) );
    
    // Initialize L_shared to 0
    for ( int c = 0; c < MATRIX_SIZE; ++c ) {
        L_shared[row][c] = 0.0f;
    }
    GroupMemoryBarrierWithGroupSync();

    // STEP 2: Parallel Cholesky Decomposition (L * L^T)
    for ( int k = 0; k < MATRIX_SIZE; ++k )
    {
        // Compute diagonal element
        if ( row == k )
        {
            float sum = 0.0f;
            for ( int j = 0; j < k; ++j )
            {
                sum += L_shared[k][j] * L_shared[k][j];
            }
            L_shared[k][k] = sqrt( max( 0.0f, A_shared[k][k] - sum ) );
        }
        GroupMemoryBarrierWithGroupSync(); // Sync after diagonal calculation

        // Compute lower triangular elements for column k
        if ( row > k )
        {
            float sum = 0.0f;
            for ( int j = 0; j < k; ++j )
            {
                sum += L_shared[row][j] * L_shared[k][j];
            }
            L_shared[row][k] = ( A_shared[row][k] - sum ) / L_shared[k][k];
        }
        GroupMemoryBarrierWithGroupSync(); // Sync before moving to the next column
    }

    // STEP 3: Forward Substitution (L * y = b)
    {
        for ( int i = 0; i < MATRIX_SIZE; ++i )
        {
            if ( row == i )
            {
                float3 sum = 0.0f;
                for ( int j = 0; j < i; ++j )
                {
                    sum += L_shared[i][j] * b_shared[j]; // Reusing b_shared to hold 'y'
                }
                b_shared[i] = ( b_shared[i] - sum ) / L_shared[i][i];
            }
            GroupMemoryBarrierWithGroupSync();
        }
    }

    // STEP 4: Backward Substitution (L^T * x = y)
    {
        for ( int i = MATRIX_SIZE - 1; i >= 0; --i )
        {
            if ( row == i )
            {
                float3 sum = 0.0f;
                for ( int j = i + 1; j < MATRIX_SIZE; ++j )
                {
                    sum += L_shared[j][i] * b_shared[j]; // L^T swap indices: L_shared[j][i]
                }
                b_shared[i] = ( b_shared[i] - sum ) / L_shared[i][i]; // b_shared now holds 'x'
            }
            GroupMemoryBarrierWithGroupSync();
        }
    }
    
    // STEP 5: Write the final solution vector x back to global VRAM
    if ( gtid.x == 0 ) {
        for ( int i = 0; i < 4; ++i ) {
            gHarmonics32[0].p_1_4[i] = b_shared[i];
            gHarmonics32[0].p_5_8[i] = b_shared[i + 4];
            gHarmonics32[0].p_9_12[i] = b_shared[i + 8];
            gHarmonics32[0].p_13_16[i] = b_shared[i + 12];
            gHarmonics32[0].p_17_20[i] = b_shared[i + 16];
            gHarmonics32[0].p_21_24[i] = b_shared[i + 20];
            gHarmonics32[0].q_1_4[i] = b_shared[i + 24];
            gHarmonics32[0].q_5_8[i] = b_shared[i + 28];
        }
        gHarmonics32[0].c_0 = b_shared[32];

        for ( int i = 0; i < 12; ++i ) {
            PackPair( b_shared[i * 2], b_shared[i * 2 + 1], gHarmonics16[0].p[i] );
        }

        for ( int j = 0; j < 4; ++j ) {
            PackPair( b_shared[24 + j * 2], b_shared[24 + j * 2 + 1], gHarmonics16[0].q[j] );
        }
        
        gHarmonics16[0].bias = b_shared[32];
    }
}