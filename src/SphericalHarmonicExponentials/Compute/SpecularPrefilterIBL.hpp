#pragma once

#include "Resources/EnvironmentResources.hpp"

class SpecularPrefilterIBL
{
public:
	void CreateResources( ID3D12Device *inDevice, D3D_ROOT_SIGNATURE_VERSION inVersion );
	void Execute( ID3D12GraphicsCommandList *inCommandList, EnvironmentResources &inResources, float inMipBias = 1.0f );
	void Destroy();

protected:
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPipelineState;
};