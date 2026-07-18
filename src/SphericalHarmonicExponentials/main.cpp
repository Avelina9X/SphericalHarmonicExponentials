#include "pch.hpp"

extern "C"
{
	// Enable the Agility SDK components
	__declspec( dllexport ) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION;
	__declspec( dllexport ) extern const char* D3D12SDKPath = reinterpret_cast<const char*>( u8".\\D3D12\\" );
}