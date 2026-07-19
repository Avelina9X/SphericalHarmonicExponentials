#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>

#include <dxgi1_6.h>
#include <d3d12.h>
#include <D3Dcompiler.h>
#include "d3dx12.h"
#include <DirectXMath.h>
#include "DirectXTex.h"

#define USE_PIX
#include <pix3.h>

#include <wrl.h>
#include <string>
#include <stdexcept>

inline std::string HrToString( HRESULT hr )
{
	char s_str[64] = {};
	sprintf_s( s_str, "HRESULT of 0x%08X", static_cast<UINT>( hr ) );
	return std::string( s_str );
}

class HrException : public std::runtime_error
{
public:
	HrException( HRESULT hr ) : std::runtime_error( HrToString( hr ) ), m_hr( hr ) {}
	HRESULT Error() const { return m_hr; }
private:
	const HRESULT m_hr;
};

#define SAFE_RELEASE(p) if (p) (p)->Release()

inline void ThrowIfFailed( HRESULT hr )
{
	if ( FAILED( hr ) )
	{
		throw HrException( hr );
	}
}