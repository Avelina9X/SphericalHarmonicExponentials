#include "pch.hpp"

#include "TextureResource.hpp"

void TextureResource::LoadTexture( ID3D12Device *inDevice, ID3D12GraphicsCommandList *inCommandList, HeapAllocator &inAllocator, UINT64 inCurrentGraphicsFenceValue )
{
	DirectX::ScratchImage srcImage;

	if ( mPath == "$DEFAULT_DIFFUSE" || mPath == "$DEFAULT_ORM" ) {
		ThrowIfFailed( srcImage.Initialize2D( DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1 ) );
		uint8_t *pixels = srcImage.GetPixels();
		pixels[0] = 255;
		pixels[1] = 255;
		pixels[2] = 255;
		pixels[3] = 255;
	}
	else if ( mPath == "$DEFAULT_NORMAL" ) {
		ThrowIfFailed( srcImage.Initialize2D( DXGI_FORMAT_R8G8_UNORM, 1, 1, 1, 1 ) );
		uint8_t *pixels = srcImage.GetPixels();
		pixels[0] = 127;
		pixels[1] = 127;
	}
	else {
		ThrowIfFailed( DirectX::LoadFromDDSFile( mPath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, srcImage ) );
	}

	const DirectX::TexMetadata &metadata = srcImage.GetMetadata();

	// Create texture desc from Image
	CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		metadata.format,
		metadata.width,
		metadata.height,
		metadata.arraySize,
		metadata.mipLevels
	);

	CD3DX12_HEAP_PROPERTIES defaultHeap( D3D12_HEAP_TYPE_DEFAULT );
	CD3DX12_HEAP_PROPERTIES uploadHeap( D3D12_HEAP_TYPE_UPLOAD );

	std::vector<D3D12_SUBRESOURCE_DATA> subresources( srcImage.GetImageCount() );
	for ( size_t i = 0; i < subresources.size(); ++i ) {
		subresources[i].RowPitch = srcImage.GetImages()[i].rowPitch;
		subresources[i].SlicePitch = srcImage.GetImages()[i].slicePitch;
		subresources[i].pData = srcImage.GetImages()[i].pixels;
	}

	// Create GPU resource for Image
	ThrowIfFailed( inDevice->CreateCommittedResource(
		&defaultHeap,
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS( mResource.ReleaseAndGetAddressOf() )
	) );

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize( mResource.Get(), 0, subresources.size() );
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

	// Schedule upload
	UpdateSubresources( inCommandList, mResource.Get(), mUploadHeap.Get(), 0, 0, subresources.size(), subresources.data() );

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		mResource.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	);
	inCommandList->ResourceBarrier( 1, &barrier );

	CD3DX12_SHADER_RESOURCE_VIEW_DESC srvDesc = CD3DX12_SHADER_RESOURCE_VIEW_DESC::Tex2D(
		textureDesc.Format,
		textureDesc.MipLevels
	);

	inAllocator.Allocate( &mSrvHandleCPU, &mSrvHandleGPU );
	inDevice->CreateShaderResourceView( mResource.Get(), &srvDesc, mSrvHandleCPU );

	mLoaded = true;
	mLoadedFenceValue = inCurrentGraphicsFenceValue;
}

void TextureResource::Cleanup( UINT64 inCompletedGraphicsFenceValue )
{
	if ( mLoadedFenceValue && inCompletedGraphicsFenceValue >= mLoadedFenceValue ) {
		mUploadHeap.Reset();
	}
}

void TextureResource::Destroy()
{
	mLoaded = false;
	mLoadedFenceValue = 0;

	mUploadHeap.Reset();
	mResource.Reset();
}