#pragma once

#include <filesystem>

class TextureParser
{
public:
	std::map<std::string, std::filesystem::path> mTextures;

	void ParseTextures();
};