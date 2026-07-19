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
};