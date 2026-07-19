#pragma once

#include "Utils/HeapAllocator.hpp"

class IntegrateBRDF
{
public:
	static constexpr UINT kIntegratedBRDFResolution = 256;

	void CreateResources( ID3D12Device *inDevice, HeapAllocator &inAllocator, D3D_ROOT_SIGNATURE_VERSION inVersion );
	void Execute( ID3D12GraphicsCommandList *inCommandList );

	D3D12_GPU_DESCRIPTOR_HANDLE GetShaderResourceView() const { return mSrvHandleGPU; }

protected:
	Microsoft::WRL::ComPtr<ID3D12Resource> mResource;
	D3D12_CPU_DESCRIPTOR_HANDLE mSrvHandleCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE mSrvHandleGPU;
	D3D12_CPU_DESCRIPTOR_HANDLE mUavHandleCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE mUavHandleGPU;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPipelineState;
};