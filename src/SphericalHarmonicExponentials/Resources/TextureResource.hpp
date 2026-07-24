#pragma once

#include "Utils/HeapAllocator.hpp"
#include <filesystem>

class TextureResource
{
public:
	TextureResource( std::filesystem::path inPath ) : mPath( inPath ) {};

	void LoadTexture( ID3D12Device *inDevice, ID3D12GraphicsCommandList *inCommandList, HeapAllocator &inAllocator, UINT64 inCurrentGraphicsFenceValue );
	void Cleanup( UINT64 inCompletedGraphicsFenceValue );
	void Destroy();

	D3D12_GPU_DESCRIPTOR_HANDLE GetShaderResourceView() const { return mSrvHandleGPU; }

protected:
	std::filesystem::path mPath;

	bool mLoaded = false;
	UINT64 mLoadedFenceValue = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource> mUploadHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mResource;
	D3D12_CPU_DESCRIPTOR_HANDLE mSrvHandleCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE mSrvHandleGPU;
};