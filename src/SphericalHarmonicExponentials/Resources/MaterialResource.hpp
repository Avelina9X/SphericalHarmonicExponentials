#pragma once

#include "Resources/TextureResource.hpp"

class MaterialResource
{
public:
	MaterialResource( std::filesystem::path inPath ) : mPath( inPath ) {};

	void LoadMaterial( const std::map<std::string, TextureResource> &inTextureResources );

	D3D12_GPU_DESCRIPTOR_HANDLE mAlbedoSrvHandleGPU;
	D3D12_GPU_DESCRIPTOR_HANDLE mNormalSrvHandleGPU;
	D3D12_GPU_DESCRIPTOR_HANDLE mORMSrvHandleGPU;

	DirectX::XMFLOAT3 mAlbedoStrength;
	float mNormalStrength;
	float mOcclusionStrength;
	float mRoughnessStrength;
	float mMetallicStrength;

protected:
	std::filesystem::path mPath;

};