#pragma once

#include "Utils/HeapAllocator.hpp"
#include <filesystem>

class EnvironmentResources
{
public:
	EnvironmentResources( std::filesystem::path inPath ) : mPath( inPath ) {};

	void LoadTexture( ID3D12Device *inDevice, ID3D12GraphicsCommandList *inCommandList, HeapAllocator &inAllocator, UINT64 inCurrentGraphicsFenceValue );
	void Cleanup( UINT64 inCompletedGraphicsFenceValue );
	void Destroy();

	static constexpr UINT kDiffuseCubemapResolution = 32;

	static constexpr UINT kRoughnessLevelsIBL = 5;

	size_t mCubemapResolution = 0;
	size_t mCubemapMipLevels = 0;

	std::filesystem::path mPath;

	bool mEquirectangularLoaded = false;
	UINT64 mEquirectangularFenceValue = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource> mUploadHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mEquirectangular;
	D3D12_CPU_DESCRIPTOR_HANDLE mEquirectangularSrvHandleCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE mEquirectangularSrvHandleGPU;

	bool mEnvironmentDataLoaded = false;

	Microsoft::WRL::ComPtr<ID3D12Resource> mUnfilteredCubemap;
	D3D12_CPU_DESCRIPTOR_HANDLE mUnfilteredCubemapSrvHandleCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE mUnfilteredCubemapSrvHandleGPU;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> mUnfilteredCubemapUavHandleCPU;
	std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> mUnfilteredCubemapUavHandleGPU;
	D3D12_CPU_DESCRIPTOR_HANDLE mUnfilteredCubemapFaceHandleCPU[6];
	D3D12_GPU_DESCRIPTOR_HANDLE mUnfilteredCubemapFaceHandleGPU[6];

	Microsoft::WRL::ComPtr<ID3D12Resource> mDiffuseCubemap;
	D3D12_CPU_DESCRIPTOR_HANDLE mDiffuseCubemapSrvHandleCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE mDiffuseCubemapSrvHandleGPU;
	D3D12_CPU_DESCRIPTOR_HANDLE mDiffuseCubemapUavHandleCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE mDiffuseCubemapUavHandleGPU;
	D3D12_CPU_DESCRIPTOR_HANDLE mDiffuseCubemapFaceHandleCPU[6];
	D3D12_GPU_DESCRIPTOR_HANDLE mDiffuseCubemapFaceHandleGPU[6];

	Microsoft::WRL::ComPtr<ID3D12Resource> mSpecularCubemap;
	D3D12_CPU_DESCRIPTOR_HANDLE mSpecularCubemapSrvHandleCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE mSpecularCubemapSrvHandleGPU;
	D3D12_CPU_DESCRIPTOR_HANDLE mSpecularCubemapUavHandleCPU[kRoughnessLevelsIBL];
	D3D12_GPU_DESCRIPTOR_HANDLE mSpecularCubemapUavHandleGPU[kRoughnessLevelsIBL];
	D3D12_CPU_DESCRIPTOR_HANDLE mSpecularCubemapFaceHandleCPU[6];
	D3D12_GPU_DESCRIPTOR_HANDLE mSpecularCubemapFaceHandleGPU[6];

	Microsoft::WRL::ComPtr<ID3D12Resource> mDiffuseHarmonics32;
	D3D12_GPU_VIRTUAL_ADDRESS mDiffuseHarmonics32Address;

	Microsoft::WRL::ComPtr<ID3D12Resource> mDiffuseHarmonics16;
	D3D12_GPU_VIRTUAL_ADDRESS mDiffuseHarmonics16Address;
};