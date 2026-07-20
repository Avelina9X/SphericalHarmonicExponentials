static const float3x3 LINEAR_REC2020_TO_LINEAR_SRGB = float3x3(
    1.6605, -0.1246, -0.0182,
    -0.5876, 1.1329, -0.1006,
    -0.0728, -0.0083, 1.1187
);

static const float3x3 LINEAR_SRGB_TO_LINEAR_REC2020 = float3x3(
    0.6274, 0.0691, 0.0164,
    0.3293, 0.9195, 0.0880,
    0.0433, 0.0113, 0.8956
);

// Converted to column major from blender: https://github.com/blender/blender/blob/fc08f7491e7eba994d86b610e5ec757f9c62ac81/release/datafiles/colormanagement/config.ocio#L358
static const float3x3 AgXInsetMatrix = float3x3(
    0.856627153315983, 0.137318972929847, 0.11189821299995,
    0.0951212405381588, 0.761241990602591, 0.0767994186031903,
    0.0482516061458583, 0.101439036467562, 0.811302368396859
);

// Converted to column major and inverted from https://github.com/EaryChow/AgX_LUT_Gen/blob/ab7415eca3cbeb14fd55deb1de6d7b2d699a1bb9/AgXBaseRec2020.py#L25
// https://github.com/google/filament/blob/bac8e58ee7009db4d348875d274daf4dd78a3bd1/filament/src/ToneMapper.cpp#L273-L278
static const float3x3 AgXOutsetMatrix = float3x3(
    1.1271005818144368, -0.1413297634984383, -0.14132976349843826,
    -0.11060664309660323, 1.157823702216272, -0.11060664309660294,
    -0.016493938717834573, -0.016493938717834257, 1.2519364065950405
);

static const float AgxMinEv = -12.47393;
static const float AgxMaxEv = 4.026069;

// Sample usage
float3 agxCdl( float3 color, float3 slope, float3 offset, float3 power, float saturation ) {
    color = mul( color, LINEAR_SRGB_TO_LINEAR_REC2020 ); // From three.js

    // 1. agx()
    // Input transform (inset)
    color = mul( color, AgXInsetMatrix );

    color = max( color, 1e-10 ); // From Filament: avoid 0 or negative numbers for log2

    // Log2 space encoding
    color = clamp( log2( color ), AgxMinEv, AgxMaxEv );
    color = ( color - AgxMinEv ) / ( AgxMaxEv - AgxMinEv );

    color = clamp( color, 0.0, 1.0 ); // From Filament

    // Apply sigmoid function approximation
    // Mean error^2: 3.6705141e-06
    float3 x2 = color * color;
    float3 x4 = x2 * x2;
    color = +15.5 * x4 * x2
            - 40.14 * x4 * color
            + 31.96 * x4
            - 6.868 * x2 * color
            + 0.4298 * x2
            + 0.1191 * color
            - 0.00232;

    // 2. agxLook()
    color = pow( color * slope + offset, power );
    const float3 lw = float3( 0.2126, 0.7152, 0.0722 );
    float luma = dot( color, lw );
    color = luma + saturation * ( color - luma );

    // 3. agxEotf()
    // Inverse input transform (outset)
    color = mul( color, AgXOutsetMatrix );

    // sRGB IEC 61966-2-1 2.2 Exponent Reference EOTF Display
    // NOTE: We're linearizing the output here. Comment/adjust when
    // *not* using a sRGB render target
    color = pow( max( 0.0, color ), 2.2 ); // From filament: max()

    color = mul( color, LINEAR_REC2020_TO_LINEAR_SRGB ); // From three.js
    // Gamut mapping. Simple clamp for now.
    color = clamp( color, 0.0, 1.0 );

    return color;
}

float3 agx( float3 color ) {
    return agxCdl( color, 1.0, 0.0, 1.0, 1.0 );
}

float3 agxGolden( float3 color ) {
    return agxCdl( color, float3( 1.0, 0.9, 0.5 ), float3( 0.0, 0.0, 0.0 ), float3( 0.8, 0.8, 0.8 ), 1.3 );
}

float3 agxPunchy( float3 color ) {
    return agxCdl( color, float3( 1.0, 1.0, 1.0 ), float3( 0.0, 0.0, 0.0 ), float3( 1.35, 1.35, 1.35 ), 1.4 );
}