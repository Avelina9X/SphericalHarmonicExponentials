#include "pch.hpp"

#include "MaterialParser.hpp"

void MaterialParser::ParseMaterials()
{
	wchar_t moduleName[_MAX_PATH] = {};
	if ( !GetModuleFileNameW( nullptr, moduleName, _MAX_PATH ) ) {
		throw std::system_error( std::error_code( static_cast<int>( GetLastError() ), std::system_category() ), "GetModuleFileNameW" );
	}

	std::filesystem::path modulePath = std::filesystem::path( moduleName ).parent_path();
	std::filesystem::path workingPath = std::filesystem::current_path();

	std::filesystem::path materialsPath;

	while ( materialsPath == std::filesystem::path() ) {
		for ( auto &i : std::filesystem::directory_iterator( modulePath ) ) {
			if ( i.is_directory() && i.path().filename() == L"materials" ) {
				materialsPath = i.path();
				break;
			}
		}

		if ( modulePath.has_parent_path() ) {
			modulePath = modulePath.parent_path();
		}
		else {
			break;
		}
	}

	while ( materialsPath == std::filesystem::path() ) {
		for ( auto &i : std::filesystem::directory_iterator( workingPath ) ) {
			if ( i.is_directory() && i.path().filename() == L"materials" ) {
				materialsPath = i.path();
				break;
			}
		}

		if ( workingPath.has_parent_path() ) {
			workingPath = workingPath.parent_path();
		}
		else {
			break;
		}
	}

	if ( materialsPath != std::filesystem::path() ) {
		for ( auto &i : std::filesystem::directory_iterator( materialsPath ) ) {
			if ( i.is_regular_file() ) {
				if ( i.path().extension() == ".dxtf_mat" ) {
					mMaterials[i.path().stem().string()] = i.path();
				}
			}
		}
	}
	else {
		assert( false );
	}
}
