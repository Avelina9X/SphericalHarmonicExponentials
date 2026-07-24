#pragma once

#include <filesystem>

class MaterialParser
{
public:
	std::map<std::string, std::filesystem::path> mMaterials;

	void ParseMaterials();
};