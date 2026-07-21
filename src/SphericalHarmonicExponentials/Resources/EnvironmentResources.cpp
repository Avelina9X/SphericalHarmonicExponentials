#include "pch.hpp"

#include "EnvironmentResources.hpp"

void EnvironmentResources::LoadTexture( ID3D12Device *inDevice, ID3D12GraphicsCommandList *inCommandList, HeapAllocator &inAllocator, UINT64 inCurrentGraphicsFenceValue )
{
	assert( !mEquirectangularLoaded );

	{
		// Load image from disk
		DirectX::ScratchImage hdrOrigImage, hdrImage;
		ThrowIfFailed( DirectX::LoadFromHDRFile( mPath.c_str(), nullptr, hdrOrigImage ) );
		ThrowIfFailed( DirectX::Convert(
			*hdrOrigImage.GetImage( 0, 0, 0 ),
			DXGI_FORMAT_R16G16B16A16_FLOAT,
			DirectX::TEX_FILTER_DEFAULT,
			DirectX::TEX_THRESHOLD_DEFAULT,
			hdrImage
		) );

		// Create texture desc from Image
		CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			hdrImage.GetMetadata().format,
			hdrImage.GetMetadata().width,
			hdrImage.GetMetadata().height,
			1, // Mip levels
			1  // Array size
		);

		CD3DX12_HEAP_PROPERTIES defaultHeap( D3D12_HEAP_TYPE_DEFAULT );
		CD3DX12_HEAP_PROPERTIES uploadHeap( D3D12_HEAP_TYPE_UPLOAD );

		// Create GPU resource for Image
		ThrowIfFailed( inDevice->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS( mEquirectangular.ReleaseAndGetAddressOf() )
		) );

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize( mEquirectangular.Get(), 0, 1 );
		CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer( uploadBufferSize );

		// Create upload resource for Image
		ThrowIfFailed( inDevice->CreateCommittedResource(
			&uploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS( mUploadHeap.ReleaseAndGetAddressOf() )
		) );

		// Specify resource data
		D3D12_SUBRESOURCE_DATA textureData = {};
		textureData.pData = hdrImage.GetPixels();
		textureData.RowPitch = hdrImage.GetImage( 0, 0, 0 )->rowPitch;
		textureData.SlicePitch = hdrImage.GetImage( 0, 0, 0 )->slicePitch;

		// Schedule upload
		UpdateSubresources( inCommandList, mEquirectangular.Get(), mUploadHeap.Get(), 0, 0, 1, &textureData );

		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			mEquirectangular.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		);
		inCommandList->ResourceBarrier( 1, &barrier );

		CD3DX12_SHADER_RESOURCE_VIEW_DESC srvDesc = CD3DX12_SHADER_RESOURCE_VIEW_DESC::Tex2D(
			textureDesc.Format,
			1 // Mip levels
		);

		inAllocator.Allocate( &mEquirectangularSrvHandleCPU, &mEquirectangularSrvHandleGPU );
		inDevice->CreateShaderResourceView( mEquirectangular.Get(), &srvDesc, mEquirectangularSrvHandleCPU );

		mEquirectangularFenceValue = inCurrentGraphicsFenceValue;
		mEquirectangularLoaded = true;

		mCubemapResolution = hdrImage.GetMetadata().width / 4;
	}

	// Create cubemap resources
	{
		mCubemapMipLevels = 0;
		DirectX::CalculateMipLevels( mCubemapResolution, mCubemapResolution, mCubemapMipLevels );

		mUnfilteredCubemapUavHandleCPU.resize( mCubemapMipLevels );
		mUnfilteredCubemapUavHandleGPU.resize( mCubemapMipLevels );

		CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R16G16B16A16_FLOAT,
			mCubemapResolution,
			mCubemapResolution,
			6,
			mCubemapMipLevels
		);
		textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		CD3DX12_HEAP_PROPERTIES defaultHeap( D3D12_HEAP_TYPE_DEFAULT );

		// Create GPU resource for Image
		ThrowIfFailed( inDevice->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS( mUnfilteredCubemap.ReleaseAndGetAddressOf() )
		) );

		CD3DX12_SHADER_RESOURCE_VIEW_DESC srvDesc = CD3DX12_SHADER_RESOURCE_VIEW_DESC::TexCube( textureDesc.Format, mCubemapMipLevels );
		inAllocator.Allocate( &mUnfilteredCubemapSrvHandleCPU, &mUnfilteredCubemapSrvHandleGPU );
		inDevice->CreateShaderResourceView( mUnfilteredCubemap.Get(), &srvDesc, mUnfilteredCubemapSrvHandleCPU );

		for ( UINT i = 0; i < mCubemapMipLevels; ++i ) {
			const auto uavDesc = CD3DX12_UNORDERED_ACCESS_VIEW_DESC::Tex2DArray( textureDesc.Format, 6, 0, i );
			inAllocator.Allocate( &mUnfilteredCubemapUavHandleCPU[i], &mUnfilteredCubemapUavHandleGPU[i] );
			inDevice->CreateUnorderedAccessView( mUnfilteredCubemap.Get(), nullptr, &uavDesc, mUnfilteredCubemapUavHandleCPU[i] );
		}

		for ( UINT i = 0; i < 6; ++i ) {
			const auto faceDesc = CD3DX12_SHADER_RESOURCE_VIEW_DESC::Tex2DArray( textureDesc.Format, 1, mCubemapMipLevels, i );
			inAllocator.Allocate( &mUnfilteredCubemapFaceHandleCPU[i], &mUnfilteredCubemapFaceHandleGPU[i] );
			inDevice->CreateShaderResourceView( mUnfilteredCubemap.Get(), &faceDesc, mUnfilteredCubemapFaceHandleCPU[i] );
		}
	}

	// Create diffuse cubemap resources
	{
		CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R16G16B16A16_FLOAT,
			kDiffuseCubemapResolution,
			kDiffuseCubemapResolution,
			6,
			1
		);
		textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		CD3DX12_HEAP_PROPERTIES defaultHeap( D3D12_HEAP_TYPE_DEFAULT );

		// Create GPU resource for Image
		ThrowIfFailed( inDevice->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS( mDiffuseCubemap.ReleaseAndGetAddressOf() )
		) );

		CD3DX12_SHADER_RESOURCE_VIEW_DESC srvDesc = CD3DX12_SHADER_RESOURCE_VIEW_DESC::TexCube( textureDesc.Format, 1 );
		CD3DX12_UNORDERED_ACCESS_VIEW_DESC uavDesc = CD3DX12_UNORDERED_ACCESS_VIEW_DESC::Tex2DArray( textureDesc.Format, 6 );

		inAllocator.Allocate( &mDiffuseCubemapSrvHandleCPU, &mDiffuseCubemapSrvHandleGPU );
		inAllocator.Allocate( &mDiffuseCubemapUavHandleCPU, &mDiffuseCubemapUavHandleGPU );

		inDevice->CreateShaderResourceView( mDiffuseCubemap.Get(), &srvDesc, mDiffuseCubemapSrvHandleCPU );
		inDevice->CreateUnorderedAccessView( mDiffuseCubemap.Get(), nullptr, &uavDesc, mDiffuseCubemapUavHandleCPU );

		for ( UINT i = 0; i < 6; ++i ) {
			CD3DX12_SHADER_RESOURCE_VIEW_DESC faceDesc = CD3DX12_SHADER_RESOURCE_VIEW_DESC::Tex2DArray( textureDesc.Format, 1, 1, i );

			inAllocator.Allocate( &mDiffuseCubemapFaceHandleCPU[i], &mDiffuseCubemapFaceHandleGPU[i] );
			inDevice->CreateShaderResourceView( mDiffuseCubemap.Get(), &faceDesc, mDiffuseCubemapFaceHandleCPU[i] );
		}

		mDiffuseCubemap->SetName( L"Diffuse Cubemap" );
	}

	// Create specular cubemap resources
	{
		CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R16G16B16A16_FLOAT,
			mCubemapResolution,
			mCubemapResolution,
			6,
			kRoughnessLevelsIBL
		);
		textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		CD3DX12_HEAP_PROPERTIES defaultHeap( D3D12_HEAP_TYPE_DEFAULT );

		// Create GPU resource for Image
		ThrowIfFailed( inDevice->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS( mSpecularCubemap.ReleaseAndGetAddressOf() )
		) );

		const auto srvDesc = CD3DX12_SHADER_RESOURCE_VIEW_DESC::TexCube( textureDesc.Format, kRoughnessLevelsIBL );
		inAllocator.Allocate( &mSpecularCubemapSrvHandleCPU, &mSpecularCubemapSrvHandleGPU );
		inDevice->CreateShaderResourceView( mSpecularCubemap.Get(), &srvDesc, mSpecularCubemapSrvHandleCPU );

		for ( UINT i = 0; i < kRoughnessLevelsIBL; ++i ) {
			const auto uavDesc = CD3DX12_UNORDERED_ACCESS_VIEW_DESC::Tex2DArray( textureDesc.Format, 6, 0, i );
			inAllocator.Allocate( &mSpecularCubemapUavHandleCPU[i], &mSpecularCubemapUavHandleGPU[i] );
			inDevice->CreateUnorderedAccessView( mSpecularCubemap.Get(), nullptr, &uavDesc, mSpecularCubemapUavHandleCPU[i] );
		}

		for ( UINT i = 0; i < 6; ++i ) {
			const auto faceDesc = CD3DX12_SHADER_RESOURCE_VIEW_DESC::Tex2DArray( textureDesc.Format, 1, kRoughnessLevelsIBL, i );
			inAllocator.Allocate( &mSpecularCubemapFaceHandleCPU[i], &mSpecularCubemapFaceHandleGPU[i] );
			inDevice->CreateShaderResourceView( mSpecularCubemap.Get(), &faceDesc, mSpecularCubemapFaceHandleCPU[i] );
		}

		mSpecularCubemap->SetName( L"Specular Cubemap" );
	}

	// 32 bit diffuse harmonics
	{
		CD3DX12_HEAP_PROPERTIES defaultHeap( D3D12_HEAP_TYPE_DEFAULT );
		const UINT harmonicCoeffBytes = sizeof( float ) * 9 * 3 + sizeof( float ); // 9*3 floats + pad float 
		CD3DX12_RESOURCE_DESC coeffBufferDesc = CD3DX12_RESOURCE_DESC::Buffer( harmonicCoeffBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS );

		// Create GPU resource
		ThrowIfFailed( inDevice->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&coeffBufferDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS( mDiffuseHarmonics32.ReleaseAndGetAddressOf() )
		) );

		// Write virtual address
		mDiffuseHarmonics32Address = mDiffuseHarmonics32->GetGPUVirtualAddress();
	}

	// 16 bit diffuse harmonics
	{
		CD3DX12_HEAP_PROPERTIES defaultHeap( D3D12_HEAP_TYPE_DEFAULT );
		const UINT harmonicCoeffBytes = sizeof( UINT ) * 4 * 3 + sizeof( float ) * 4; // Packed format
		CD3DX12_RESOURCE_DESC coeffBufferDesc = CD3DX12_RESOURCE_DESC::Buffer( harmonicCoeffBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS );

		// Create GPU resource
		ThrowIfFailed( inDevice->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&coeffBufferDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS( mDiffuseHarmonics16.ReleaseAndGetAddressOf() )
		) );

		// Write virtual address
		mDiffuseHarmonics16Address = mDiffuseHarmonics16->GetGPUVirtualAddress();
	}
}

void EnvironmentResources::Cleanup( UINT64 inCompletedGraphicsFenceValue )
{
	if ( mEquirectangularLoaded && inCompletedGraphicsFenceValue >= mEquirectangularFenceValue ) {
		mUploadHeap.Reset();
	}
}

void EnvironmentResources::Destroy()
{
	mEquirectangularLoaded = false;
	mEnvironmentDataLoaded = false;
	mEquirectangularFenceValue = 0;

	mUploadHeap.Reset();
	mEquirectangular.Reset();
	mUnfilteredCubemap.Reset();

	mUnfilteredCubemapUavHandleCPU.clear();
	mUnfilteredCubemapUavHandleGPU.clear();
}