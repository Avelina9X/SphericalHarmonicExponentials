#pragma once

#include "Resources/EnvironmentResources.hpp"

class DiffusePrefilterIBL
{
public:
	void CreateResources( ID3D12Device *inDevice, D3D_ROOT_SIGNATURE_VERSION inVersion );
	void Execute( ID3D12GraphicsCommandList *inCommandList, EnvironmentResources &inResources );
	void Destroy();

protected:
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPipelineState;
};