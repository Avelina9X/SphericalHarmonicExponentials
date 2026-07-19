#pragma once

#include <filesystem>

class EnvironmentParser
{
public:
	std::map<std::string, std::filesystem::path> mEnvironments;

	void ParseEnvironments();
};