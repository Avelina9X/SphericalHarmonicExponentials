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
