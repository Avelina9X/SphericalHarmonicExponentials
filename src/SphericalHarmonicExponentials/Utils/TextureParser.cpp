#include "pch.hpp"

#include "TextureParser.hpp"

void TextureParser::ParseTextures()
{
	wchar_t moduleName[_MAX_PATH] = {};
	if ( !GetModuleFileNameW( nullptr, moduleName, _MAX_PATH ) ) {
		throw std::system_error( std::error_code( static_cast<int>( GetLastError() ), std::system_category() ), "GetModuleFileNameW" );
	}

	std::filesystem::path modulePath = std::filesystem::path( moduleName ).parent_path();
	std::filesystem::path workingPath = std::filesystem::current_path();

	std::filesystem::path texturesPath;

	while ( texturesPath == std::filesystem::path() ) {
		for ( auto &i : std::filesystem::directory_iterator( modulePath ) ) {
			if ( i.is_directory() && i.path().filename() == L"textures" ) {
				texturesPath = i.path();
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

	while ( texturesPath == std::filesystem::path() ) {
		for ( auto &i : std::filesystem::directory_iterator( workingPath ) ) {
			if ( i.is_directory() && i.path().filename() == L"textures" ) {
				texturesPath = i.path();
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

	if ( texturesPath != std::filesystem::path() ) {
		for ( auto &i : std::filesystem::directory_iterator( texturesPath ) ) {
			if ( i.is_regular_file() ) {
				if ( i.path().extension() == ".dds" || i.path().extension() == ".DDS" ) {
					mTextures[i.path().filename().string()] = i.path();
				}
			}
		}
	}
	else {
		assert( false );
	}
}
