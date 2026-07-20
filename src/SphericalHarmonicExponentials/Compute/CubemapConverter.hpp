#pragma once

#include "Resources/EnvironmentResources.hpp"

class CubemapConverter
{
public:
	void CreateResources( ID3D12Device *inDevice, D3D_ROOT_SIGNATURE_VERSION inVersion );
	void Execute( ID3D12GraphicsCommandList *inCommandList, EnvironmentResources &inResources, float inClampValue = 1000.0f );
	void Destroy();

protected:
	// Equi-to-cubemap compute states
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mEquirectRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mEquirectPipelineState;

	// Cubemap mipping compute states
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mCubeMipRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mCubeMipPipelineState;
};