#include "pch.hpp"

#include "CubemapConverter.hpp"

#include "Utils/ReadData.hpp"

void CubemapConverter::CreateResources( ID3D12Device *inDevice, D3D_ROOT_SIGNATURE_VERSION inVersion )
{
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS  |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;

	// Create equi-to-cubemap compute PSO and RS
	{
		{
			CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
			ranges[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE ); // TODO: check volatility
			ranges[1].Init( D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE ); // TODO: check volatility

			CD3DX12_ROOT_PARAMETER1 rootParameters[3];
			rootParameters[0].InitAsConstants( 3, 0 );
			rootParameters[1].InitAsDescriptorTable( 1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL );
			rootParameters[2].InitAsDescriptorTable( 1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL );

			CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(
				0,
				D3D12_FILTER_ANISOTROPIC,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP
			);

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
			computeRootSignatureDesc.Init_1_1( _countof( rootParameters ), rootParameters, 1, &linearClampSampler, rootSignatureFlags );

			Microsoft::WRL::ComPtr<ID3DBlob> signature;
			Microsoft::WRL::ComPtr<ID3DBlob> error;
			ThrowIfFailed( D3DX12SerializeVersionedRootSignature( &computeRootSignatureDesc, inVersion, &signature, &error ) );
			ThrowIfFailed( inDevice->CreateRootSignature( 0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS( mEquirectRootSignature.ReleaseAndGetAddressOf() ) ) );
		}

		{
			std::vector csBlob = DX::ReadData( L"./CubemapConvertCS.cso" );

			D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
			computePsoDesc.pRootSignature = mEquirectRootSignature.Get();
			computePsoDesc.CS = CD3DX12_SHADER_BYTECODE( csBlob.data(), csBlob.size() );

			ThrowIfFailed( inDevice->CreateComputePipelineState( &computePsoDesc, IID_PPV_ARGS( mEquirectPipelineState.ReleaseAndGetAddressOf() ) ) );
		}
	}

	// Create cubemap mipping compute PSO and RS
	{
		{
			CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
			ranges[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE ); // TODO: check volatility
			ranges[1].Init( D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE ); // TODO: check volatility

			CD3DX12_ROOT_PARAMETER1 rootParameters[3];
			rootParameters[0].InitAsConstants( 2, 0 );
			rootParameters[1].InitAsDescriptorTable( 1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL );
			rootParameters[2].InitAsDescriptorTable( 1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL );

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
			computeRootSignatureDesc.Init_1_1( _countof( rootParameters ), rootParameters, 0, nullptr, rootSignatureFlags );

			Microsoft::WRL::ComPtr<ID3DBlob> signature;
			Microsoft::WRL::ComPtr<ID3DBlob> error;
			ThrowIfFailed( D3DX12SerializeVersionedRootSignature( &computeRootSignatureDesc, inVersion, &signature, &error ) );
			ThrowIfFailed( inDevice->CreateRootSignature( 0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS( mCubeMipRootSignature.ReleaseAndGetAddressOf() ) ) );
		}

		{
			std::vector csBlob = DX::ReadData( L"./CubemapMipCS.cso" );

			D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
			computePsoDesc.pRootSignature = mCubeMipRootSignature.Get();
			computePsoDesc.CS = CD3DX12_SHADER_BYTECODE( csBlob.data(), csBlob.size() );

			ThrowIfFailed( inDevice->CreateComputePipelineState( &computePsoDesc, IID_PPV_ARGS( mCubeMipPipelineState.ReleaseAndGetAddressOf() ) ) );
		}
	}
}

void CubemapConverter::Execute( ID3D12GraphicsCommandList *inCommandList, EnvironmentResources &inResources, float inClampValue )
{
	const UINT cubemapResolution = inResources.mCubemapResolution;

	// If previously created, transition back to UAV
	if ( inResources.mEnvironmentDataLoaded ) {
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition( inResources.mUnfilteredCubemap.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
		inCommandList->ResourceBarrier( 1, &barrier );
	}

	// Execute equirect-to-cubemap compute shader
	{
		inCommandList->SetPipelineState( mEquirectPipelineState.Get() );
		inCommandList->SetComputeRootSignature( mEquirectRootSignature.Get() );

		UINT resolution[2] = { cubemapResolution, cubemapResolution };
		inCommandList->SetComputeRoot32BitConstants( 0, 2, resolution, 0 );
		inCommandList->SetComputeRoot32BitConstants( 0, 1, &inClampValue, 2 );

		inCommandList->SetComputeRootDescriptorTable( 1, inResources.mEquirectangularSrvHandleGPU );
		inCommandList->SetComputeRootDescriptorTable( 2, inResources.mUnfilteredCubemapUavHandleGPU[0] );

		inCommandList->Dispatch( cubemapResolution / 16, cubemapResolution / 16, 6 );

		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV( inResources.mUnfilteredCubemap.Get() );
		inCommandList->ResourceBarrier( 1, &barrier );
	}

	// Execute mip chain
	{
		inCommandList->SetPipelineState( mCubeMipPipelineState.Get() );
		inCommandList->SetComputeRootSignature( mCubeMipRootSignature.Get() );

		UINT resolution[2] = { cubemapResolution, cubemapResolution };

		for ( UINT i = 0; i < inResources.mCubemapMipLevels - 1; ++i ) {
			inCommandList->SetComputeRoot32BitConstants( 0, 2, resolution, 0 );

			resolution[0] /= 2;
			resolution[1] /= 2;

			inCommandList->SetComputeRootDescriptorTable( 1, inResources.mUnfilteredCubemapUavHandleGPU[i] );
			inCommandList->SetComputeRootDescriptorTable( 2, inResources.mUnfilteredCubemapUavHandleGPU[i + 1] );

			inCommandList->Dispatch( std::max( 1U, resolution[0] / 16 ), std::max( 1U, resolution[1] / 16 ), 6 );

			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV( inResources.mUnfilteredCubemap.Get() );
			inCommandList->ResourceBarrier( 1, &barrier );
		}

		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition( inResources.mUnfilteredCubemap.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON );
		inCommandList->ResourceBarrier( 1, &barrier );
	}
}
