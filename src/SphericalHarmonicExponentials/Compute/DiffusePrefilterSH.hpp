#pragma once

#include "Resources/EnvironmentResources.hpp"

class DiffusePrefilterSH
{
public:
	void CreateResources( ID3D12Device *inDevice, D3D_ROOT_SIGNATURE_VERSION inVersion );
	void Execute( ID3D12GraphicsCommandList *inCommandList, EnvironmentResources &inResources );
	void Destroy();

protected:
	Microsoft::WRL::ComPtr<ID3D12Resource> mHarmonicsArray;
	D3D12_GPU_VIRTUAL_ADDRESS mHarmonicsArrayAddress;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mPrefilterRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPrefilterPipelineState;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mAccumulatorRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mAccumulatorPipelineState;
};