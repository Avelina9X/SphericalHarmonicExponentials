#pragma once

#include "Utils/HeapAllocator.hpp"
#include "Resources/EnvironmentResources.hpp"

class SpecularPrefilterSH
{
public:
	static constexpr UINT kSpecularCollectorResolution = 256;
	static constexpr UINT kSpecularRoughnessLevelsSH = 64;
	static constexpr UINT kVerticlaGroupsPerDispatch = 16;

	UINT mSpecularRoughnessLevelChoice = 4;
	float mMinAlphaLevel = 0.2f;
	float mMaxAlphaLevel = 1.0f;

	void CreateResources( ID3D12Device *inDevice, HeapAllocator &inAllocator, D3D_ROOT_SIGNATURE_VERSION inVersion );
	void Execute( ID3D12GraphicsCommandList *inCommandList, EnvironmentResources &inResources );
	void Destroy();

	Microsoft::WRL::ComPtr<ID3D12Resource> mSpecularCollector;
	D3D12_CPU_DESCRIPTOR_HANDLE mSpecularCollectorSrvHandleCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE mSpecularCollectorSrvHandleGPU;
	D3D12_CPU_DESCRIPTOR_HANDLE mSpecularCollectorUavHandleCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE mSpecularCollectorUavHandleGPU;

protected:
	Microsoft::WRL::ComPtr<ID3D12Resource> mSpecularAccumulator;
	D3D12_GPU_VIRTUAL_ADDRESS mSpecularAccumulatorAddress;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mPrefilterRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPrefilterPipelineState;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mAccumulatorRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mAccumulatorPipelineState;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mSumRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mSumPipelineState;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mSolverRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mSolverPipelineState;
};