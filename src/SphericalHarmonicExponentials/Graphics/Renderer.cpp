#include "pch.hpp"

#include "Renderer.hpp"

#include "Utils/ReadData.hpp"

Renderer::Renderer( UINT inFrameCount, UINT inSphereTessellation )
{
	mFrameCount = inFrameCount;
	mSphereTessellation = inSphereTessellation;

	mFrameResources.resize( inFrameCount );
}

void Renderer::CreateResources( ID3D12Device *inDevice, ID3D12GraphicsCommandList *inCommandList, HeapAllocator &inAllocator, UINT64 inCurrentGraphicsFenceValue )
{
	// Create sphere mesh
	{
		std::vector<VertexData> vertices;
		std::vector<UINT> indices;

		ComputeSphere( vertices, indices );

		mSphereIndexCount = indices.size();
		mUploadFenceValue = inCurrentGraphicsFenceValue;

		const UINT vertexBufferBytes = vertices.size() * sizeof( VertexData );
		const UINT indexBufferBytes = indices.size() * sizeof( UINT );

		CD3DX12_HEAP_PROPERTIES uploadHeap( D3D12_HEAP_TYPE_UPLOAD );
		CD3DX12_HEAP_PROPERTIES defaultHeap( D3D12_HEAP_TYPE_DEFAULT );

		CD3DX12_RESOURCE_DESC vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer( vertexBufferBytes );
		CD3DX12_RESOURCE_DESC indexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer( indexBufferBytes );

		// Create default resources
		ThrowIfFailed( inDevice->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&vertexBufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS( mSphereVBO.ReleaseAndGetAddressOf() )
		) );
		ThrowIfFailed( inDevice->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&indexBufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS( mSphereIBO.ReleaseAndGetAddressOf() )
		) );

		// Create upload heaps
		ThrowIfFailed( inDevice->CreateCommittedResource(
			&uploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&vertexBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS( mUploadVBO.GetAddressOf() )
		) );
		ThrowIfFailed( inDevice->CreateCommittedResource(
			&uploadHeap,
			D3D12_HEAP_FLAG_NONE,
			&indexBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS( mUploadIBO.GetAddressOf() )
		) );

		// Describe intermediate data
		D3D12_SUBRESOURCE_DATA vertexData = {};
		vertexData.pData = vertices.data();
		vertexData.RowPitch = vertexBufferBytes;
		vertexData.SlicePitch = vertexBufferBytes;

		D3D12_SUBRESOURCE_DATA indexData = {};
		indexData.pData = indices.data();
		indexData.RowPitch = indexBufferBytes;
		indexData.SlicePitch = indexBufferBytes;

		// Schedule uploads
		UpdateSubresources<1>( inCommandList, mSphereVBO.Get(), mUploadVBO.Get(), 0, 0, 1, &vertexData );
		UpdateSubresources<1>( inCommandList, mSphereIBO.Get(), mUploadIBO.Get(), 0, 0, 1, &indexData );

		// Barrier transition
		{
			CD3DX12_RESOURCE_BARRIER barriers[2] = {
				CD3DX12_RESOURCE_BARRIER::Transition(
					mSphereVBO.Get(),
					D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
				),
					CD3DX12_RESOURCE_BARRIER::Transition(
					mSphereIBO.Get(),
					D3D12_RESOURCE_STATE_COPY_DEST,
					D3D12_RESOURCE_STATE_INDEX_BUFFER
				)
			};

			inCommandList->ResourceBarrier( 2, barriers );
		}

		// Intialize VBO and IBO views
		mSphereVertexBufferView.BufferLocation = mSphereVBO->GetGPUVirtualAddress();
		mSphereVertexBufferView.StrideInBytes = sizeof( VertexData );
		mSphereVertexBufferView.SizeInBytes = vertexBufferBytes;

		mSphereIndexBufferView.BufferLocation = mSphereIBO->GetGPUVirtualAddress();
		mSphereIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
		mSphereIndexBufferView.SizeInBytes = indexBufferBytes;
	}

	// Create scene constant buffer resources
	{
		const UINT bufferBytes = ( sizeof( RendererData ) + 255 ) / 256 * 256;
		CD3DX12_HEAP_PROPERTIES uploadHeap( D3D12_HEAP_TYPE_UPLOAD );
		CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer( bufferBytes );

		for ( UINT i = 0; i < mFrameCount; ++i ) {
			ThrowIfFailed( inDevice->CreateCommittedResource(
				&uploadHeap,
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS( mFrameResources[i].mResource.ReleaseAndGetAddressOf() )
			) );

			mFrameResources[i].mBufferAddress = mFrameResources[i].mResource->GetGPUVirtualAddress();

			// Map
			CD3DX12_RANGE readRange( 0, 0 );
			ThrowIfFailed( mFrameResources[i].mResource->Map( 0, &readRange, &mFrameResources[i].mDataPointer ) );
		}
	}
}

void Renderer::LoadShaders( ID3D12Device *inDevice, DXGI_FORMAT inBackBufferFormat, DXGI_FORMAT inDepthBufferFormat, UINT inSampleCount, D3D_ROOT_SIGNATURE_VERSION inVersion )
{
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;

	CD3DX12_STATIC_SAMPLER_DESC clampSampler(
		0,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP
	);
	clampSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	CD3DX12_STATIC_SAMPLER_DESC anisotropicSampler(
		1,
		D3D12_FILTER_ANISOTROPIC,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP
	);
	anisotropicSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// Create IBL root signature
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[3] = {};
		ranges[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC );
		ranges[1].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC );
		ranges[2].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC );

		CD3DX12_ROOT_PARAMETER1 rootParameters[4] = {};
		rootParameters[0].InitAsConstantBufferView( 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL );
		rootParameters[1].InitAsDescriptorTable( 1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL );
		rootParameters[2].InitAsDescriptorTable( 1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL );
		rootParameters[3].InitAsDescriptorTable( 1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL );

		CD3DX12_STATIC_SAMPLER_DESC samplers[] = { clampSampler, anisotropicSampler };

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1( _countof( rootParameters ), rootParameters, _countof( samplers ), samplers, rootSignatureFlags );

		Microsoft::WRL::ComPtr<ID3DBlob> signature;
		Microsoft::WRL::ComPtr<ID3DBlob> error;
		ThrowIfFailed( D3DX12SerializeVersionedRootSignature( &rootSignatureDesc, inVersion, &signature, &error ) );
		ThrowIfFailed( inDevice->CreateRootSignature( 0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS( mShadingRootSignatureIBL.ReleaseAndGetAddressOf() ) ) );
	}

	// Create SH root signature
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[1] = {};
		ranges[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC );

		CD3DX12_ROOT_PARAMETER1 rootParameters[4] = {};
		rootParameters[0].InitAsConstantBufferView( 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_ALL );
		rootParameters[1].InitAsDescriptorTable( 1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL );
		rootParameters[2].InitAsShaderResourceView( 1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE, D3D12_SHADER_VISIBILITY_PIXEL );
		rootParameters[3].InitAsShaderResourceView( 2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE, D3D12_SHADER_VISIBILITY_PIXEL );

		CD3DX12_STATIC_SAMPLER_DESC samplers[] = { clampSampler };

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1( _countof( rootParameters ), rootParameters, _countof( samplers ), samplers, rootSignatureFlags );

		Microsoft::WRL::ComPtr<ID3DBlob> signature;
		Microsoft::WRL::ComPtr<ID3DBlob> error;
		ThrowIfFailed( D3DX12SerializeVersionedRootSignature( &rootSignatureDesc, inVersion, &signature, &error ) );
		ThrowIfFailed( inDevice->CreateRootSignature( 0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS( mShadingRootSignatureSH.ReleaseAndGetAddressOf() ) ) );
	}

	// Create PSOs
	{
		std::vector vsBlob = DX::ReadData( L"./ShadeVS.cso" );
		std::vector psBlobIBL = DX::ReadData( L"./IBL_ShadePS.cso" );
		std::vector psBlobSH32 = DX::ReadData( L"./SH_Shade32PS.cso" );
		std::vector psBlobSH16 = DX::ReadData( L"./SH_Shade16PS.cso" );

		// Define the vertex input layout
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof( inputElementDescs ) };
		psoDesc.VS = CD3DX12_SHADER_BYTECODE( vsBlob.data(), vsBlob.size() );
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC( D3D12_DEFAULT );
		psoDesc.RasterizerState.FrontCounterClockwise = TRUE;
		psoDesc.BlendState = CD3DX12_BLEND_DESC( D3D12_DEFAULT );
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC( D3D12_DEFAULT );
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = inBackBufferFormat;
		psoDesc.DSVFormat = inDepthBufferFormat;
		psoDesc.SampleDesc.Count = inSampleCount;

		psoDesc.pRootSignature = mShadingRootSignatureIBL.Get();
		psoDesc.PS = CD3DX12_SHADER_BYTECODE( psBlobIBL.data(), psBlobIBL.size() );
		ThrowIfFailed( inDevice->CreateGraphicsPipelineState( &psoDesc, IID_PPV_ARGS( mShadingPipelineStateIBL.ReleaseAndGetAddressOf() ) ) );

		psoDesc.pRootSignature = mShadingRootSignatureSH.Get();
		psoDesc.PS = CD3DX12_SHADER_BYTECODE( psBlobSH32.data(), psBlobSH32.size() );
		ThrowIfFailed( inDevice->CreateGraphicsPipelineState( &psoDesc, IID_PPV_ARGS( mShadingPipelineStateSH32.ReleaseAndGetAddressOf() ) ) );

		psoDesc.pRootSignature = mShadingRootSignatureSH.Get();
		psoDesc.PS = CD3DX12_SHADER_BYTECODE( psBlobSH16.data(), psBlobSH16.size() );
		ThrowIfFailed( inDevice->CreateGraphicsPipelineState( &psoDesc, IID_PPV_ARGS( mShadingPipelineStateSH16.ReleaseAndGetAddressOf() ) ) );
	}
}

void Renderer::Cleanup( UINT64 inCompletedGraphicsFenceValue )
{
	if ( inCompletedGraphicsFenceValue >= mUploadFenceValue ) {
		mUploadVBO.Reset();
		mUploadIBO.Reset();
	}
}

void Renderer::CommitData( UINT inFrameIndex )
{
	mFrameIndex = inFrameIndex;

	float zoom = 0.0f;

	DirectX::XMVECTOR cameraPos = DirectX::XMVectorSet( std::sin( mCameraYaw ) * std::cos( mCameraPitch ), std::sin( mCameraPitch ), std::cos( mCameraYaw ) * std::cos( mCameraPitch ), 0.0f );
	cameraPos = DirectX::XMVectorScale( DirectX::XMVector3Normalize( cameraPos ), std::lerp( 2.0f, 1.1f, zoom ) );

	DirectX::XMMATRIX view = DirectX::XMMatrixLookAtRH( cameraPos, DirectX::g_XMZero, DirectX::XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f ) );

	float aspectRatio = static_cast<float>( mViewportWidth ) / static_cast<float>( mViewportHeight );
	if ( mEnableIBL && mEnableSH ) aspectRatio /= 2.0f;

	DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovRH( DirectX::XM_PI / 2.5f, aspectRatio, 0.01f, 100.0f );

	mRendererData.EyePosition = DirectX::XMMatrixInverse( nullptr, view ).r[3];
	mRendererData.ViewProj = DirectX::XMMatrixMultiplyTranspose( view, proj );

	memcpy( mFrameResources[mFrameIndex].mDataPointer, &mRendererData, sizeof( RendererData ) );
}

void Renderer::Draw( ID3D12GraphicsCommandList *inCommandList, EnvironmentResources &inResources, D3D12_GPU_DESCRIPTOR_HANDLE inBRDF ) const
{
	inCommandList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

	inCommandList->IASetIndexBuffer( &mSphereIndexBufferView );
	inCommandList->IASetVertexBuffers( 0, 1, &mSphereVertexBufferView );

	if ( mEnableIBL ) {
		if( mEnableIBL && mEnableSH ) {
			const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>( mViewportWidth ) / 2, static_cast<float>( mViewportHeight ), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
			const D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>( mViewportWidth ) / 2, static_cast<LONG>( mViewportHeight ) };
			inCommandList->RSSetViewports( 1, &viewport );
			inCommandList->RSSetScissorRects( 1, &scissorRect );
		}

		inCommandList->SetGraphicsRootSignature( mShadingRootSignatureIBL.Get() );
		inCommandList->SetPipelineState( mShadingPipelineStateIBL.Get() );

		inCommandList->SetGraphicsRootConstantBufferView( 0, mFrameResources[mFrameIndex].mBufferAddress );
		inCommandList->SetGraphicsRootDescriptorTable( 1, inBRDF );
		inCommandList->SetGraphicsRootDescriptorTable( 2, inResources.mDiffuseCubemapSrvHandleGPU );
		inCommandList->SetGraphicsRootDescriptorTable( 3, inResources.mSpecularCubemapSrvHandleGPU );

		inCommandList->DrawIndexedInstanced( mSphereIndexCount, 1, 0, 0, 0 );
	}

	if ( mEnableSH ) {
		if( mEnableIBL && mEnableSH ) {
			const D3D12_VIEWPORT viewport = { static_cast<float>( mViewportWidth ) / 2, 0, static_cast<float>( mViewportWidth ) / 2, static_cast<float>( mViewportHeight ), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
			const D3D12_RECT scissorRect = { static_cast<LONG>( mViewportWidth ) / 2, 0, static_cast<LONG>( mViewportWidth ), static_cast<LONG>( mViewportHeight ) };
			inCommandList->RSSetViewports( 1, &viewport );
			inCommandList->RSSetScissorRects( 1, &scissorRect );
		}

		inCommandList->SetGraphicsRootSignature( mShadingRootSignatureSH.Get() );

		if ( mUseHalfSH ) {
			inCommandList->SetPipelineState( mShadingPipelineStateSH16.Get() );
			inCommandList->SetGraphicsRootShaderResourceView( 2, inResources.mDiffuseHarmonics16Address );
			inCommandList->SetGraphicsRootShaderResourceView( 3, inResources.mSpecularHarmonics16Address );
		}
		else {
			inCommandList->SetPipelineState( mShadingPipelineStateSH32.Get() );
			inCommandList->SetGraphicsRootShaderResourceView( 2, inResources.mDiffuseHarmonics32Address );
			inCommandList->SetGraphicsRootShaderResourceView( 3, inResources.mSpecularHarmonics32Address );
		}

		inCommandList->SetGraphicsRootConstantBufferView( 0, mFrameResources[mFrameIndex].mBufferAddress );
		inCommandList->SetGraphicsRootDescriptorTable( 1, inBRDF );

		inCommandList->DrawIndexedInstanced( mSphereIndexCount, 1, 0, 0, 0 );
	}
}

void Renderer::ComputeSphere( std::vector<VertexData>& ioVertices, std::vector<UINT>& ioIndices ) const
{
	ioVertices.clear();
	ioIndices.clear();

	const UINT verticalSegments = mSphereTessellation;
	const UINT horizontalSegments = mSphereTessellation * 2;

	const float radius = 1.0f;

	for ( UINT i = 0; i <= verticalSegments; ++i ) {

		//const float v = 1.0f - i / static_cast<float>( verticalSegments );

		const float latitude = ( i * DirectX::XM_PI / verticalSegments ) - DirectX::XM_PIDIV2;

		float dy, dxz;
		DirectX::XMScalarSinCos( &dy, &dxz, latitude );

		for ( UINT j = 0; j <= horizontalSegments; ++j ) {

			//const float u = j / static_cast<float>( horizontalSegments );

			const float longitude = j * DirectX::XM_2PI / horizontalSegments;

			float dx, dz;
			DirectX::XMScalarSinCos( &dx, &dz, longitude );

			dx *= dxz;
			dz *= dxz;

			ioVertices.emplace_back(
				DirectX::XMFLOAT3{ dx * radius, dy * radius, dz * radius },
				DirectX::XMFLOAT3{ dx, dy, dz }
			);
		}
	}

	const UINT stride = horizontalSegments + 1;

	for ( UINT i = 0; i < verticalSegments; ++i ) {
		for ( UINT j = 0; j <= horizontalSegments; ++j ) {
			const UINT nextI = i + 1;
			const UINT nextJ = ( j + 1 ) % stride;

			ioIndices.push_back( i * stride + nextJ );
			ioIndices.push_back( nextI * stride + j );
			ioIndices.push_back( i * stride + j );

			ioIndices.push_back( nextI * stride + nextJ );
			ioIndices.push_back( nextI * stride + j );
			ioIndices.push_back( i * stride + nextJ );
		}
	}
}
