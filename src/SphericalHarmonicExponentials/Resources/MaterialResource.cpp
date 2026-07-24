#include "pch.hpp"

#include "MaterialResource.hpp"

#include "json.hpp"
#include <fstream>

void MaterialResource::LoadMaterial( const std::map<std::string, TextureResource> &inTextureResources )
{
	std::ifstream filestream( mPath );

	nlohmann::json data;
	filestream >> data;

	auto diffuseData = data["diffuse"].get<nlohmann::json>();
	std::string diffusePath = diffuseData["texture"].get<std::string>();
	DirectX::XMFLOAT4 diffuseStrength( diffuseData["strength"].get<std::array<float, 4>>().data() );
	mAlbedoStrength = { diffuseStrength.x, diffuseStrength.y, diffuseStrength.z };

	auto normalData = data["normal"].get<nlohmann::json>();
	std::string normalPath = normalData["texture"].get<std::string>();
	mNormalStrength = normalData["strength"].get<float>();

	auto ormData = data["orm"].get<nlohmann::json>();
	std::string ormPath = ormData["texture"].get<std::string>();
	mOcclusionStrength = ormData["occlusion_strength"].get<float>();
	mRoughnessStrength = ormData["roughness_strength"].get<float>();
	mMetallicStrength = ormData["metalness_strength"].get<float>();

	mAlbedoSrvHandleGPU = inTextureResources.at( diffusePath ).GetShaderResourceView();
	mNormalSrvHandleGPU = inTextureResources.at( normalPath ).GetShaderResourceView();
	mORMSrvHandleGPU = inTextureResources.at( ormPath ).GetShaderResourceView();
}
