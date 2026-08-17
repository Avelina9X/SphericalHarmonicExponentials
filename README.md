# Spherical Harmonic Exponentials for Efficient Glossy Reflections in D3D12
This repo is an implementation of [Activision's paper](https://onlinelibrary.wiley.com/doi/abs/10.1111/cgf.70219) in D3D12, showcasing the application of Spherical Harmonics for specular indirect lighting.

## Features
- End-to-end computation of both diffuse and specular spherical harmonics implemented entirely in compute shaders.
- Computation of both diffuse and specular IBL to serve as a comparison.
- Variety of pixel shader implementations which demonstrate the performance differences between:
  - FP32, FP16 and FP11/11/10 coefficients
  - Full precision operations and native FP16 (`N`)
  - Coefficients stored in a Structured Buffer and a Constant Buffer (`CBV`)
- Includes environment maps used in the original paper and example textures obtained from [Intel Sponza](https://www.intel.com/content/www/us/en/developer/topic-technology/graphics-research/samples.html)
- AgX tonemapping to visually match the original paper with adjustable exposure

## Compiling
Pull the latest source and compile with Visual Studio 2022+. All external dependencies are included in the repo and by NuGet.

## Running
Once compiled, the executable can be launched without any additional setup.  
Requires an SM6.2 capable GPU, Native 16 bit op support is optional.
> **NOTE:** developer mode MUST be enabled in Windows settings as the software requests a stable power state to facilitate benchmarking.

### Controls
- Drag Middle Mouse to orbit the view
- `-` and `=` to cycle through environment maps
- `[` and `]` to cycle through textures

### Settings
- `Environment Clamp` clamps the maximum pixel values when converting the environment map to an unfiltered cubemap. This affects the diffuse and specular results for both IBL and SH.
- `IBL Specular Bias` adds a LOD bias when performing IBL Specular Prefiltering to reduce artefacts.
- `SH Levels Alpha` dictates the number of uniformly spaced alpha samples used for specular SH computation.
- `SH Min/Max Alpha` dictates the upper and lower bounds of the alpha samples. (Note that alpha = roughness^2)
After adjusting settings you must click "Recompute" to update the associate environment.

<br/>

## Quirks in the Codebase
I'm gonna point out a couple 'quirks' in the codebase for those that are interested, because these have been a source of confusion and suffering!

- In `ImportanceSampleGGX` for several shaders:

  ```hlsl
  float cosTheta = min( 1, sqrt( ( 1.0f - Xi.y ) / ( 1.0f + ( a * a - 1.0f ) * Xi.y ) ) );
  float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );
  ```
  We require the `min( 1, ...` for `cosTheta` as the compiler my attempt to optimize away the sqrt and cause NaN errors.

- In `FibonacciSphere` for the specular SH prefiltering and accumulation shaders:

  ```hlsl
  float r = sqrt( 1.0f - saturate( y * y ) );
  ```
  We require the `saturate` despite this value being bounded by [0, 1] as ***nondeterministically*** we may encounter NaNs without the saturate.

- In `Renderer.cpp`:

  ```c++
  if ( !inResources.mCreatedThisFrame ) {
  	CD3DX12_RESOURCE_BARRIER barriers[] = {
  		CD3DX12_RESOURCE_BARRIER::Transition( inResources.mDiffuseHarmonicsCBV16.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER ),
  		CD3DX12_RESOURCE_BARRIER::Transition( inResources.mSpecularHarmonicsCBV16.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER ),
  	};
  	inCommandList->ResourceBarrier( _countof( barriers ), barriers );
  }
  else {
  	inResources.mCreatedThisFrame = false;
  }
  ```
  We must perform a barrier on the 16 bit SH CBVs in every frame following the one it which they were last updated otherwise we experience cache invalidation issue when multiple frames are in flight. There is absolutely no reason this should be necessary. The resources will automatically promote to the correct state without intervention. However without the 'pointless' barrier we experience glitches. No one can figure out why it occurs in the first place, nor why this fixes things.
