#include "pch.hpp"

#include "SpecularPrefilterIBL.hpp"

#include "Utils/ReadData.hpp"

void SpecularPrefilterIBL::CreateResources( ID3D12Device *inDevice, D3D_ROOT_SIGNATURE_VERSION inVersion )
{
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
		ranges[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE ); // TODO: check volatility
		ranges[1].Init( D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE ); // TODO: check volatility

		CD3DX12_ROOT_PARAMETER1 rootParameters[3];
		rootParameters[0].InitAsConstants( 5, 0 );
		rootParameters[1].InitAsDescriptorTable( 1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL );
		rootParameters[2].InitAsDescriptorTable( 1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL );

		CD3DX12_STATIC_SAMPLER_DESC linearWrapSampler(
			0,
			D3D12_FILTER_ANISOTROPIC,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP
		);

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS  |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
		computeRootSignatureDesc.Init_1_1( _countof( rootParameters ), rootParameters, 1, &linearWrapSampler, rootSignatureFlags );

		Microsoft::WRL::ComPtr<ID3DBlob> signature;
		Microsoft::WRL::ComPtr<ID3DBlob> error;
		ThrowIfFailed( D3DX12SerializeVersionedRootSignature( &computeRootSignatureDesc, inVersion, &signature, &error ) );
		ThrowIfFailed( inDevice->CreateRootSignature( 0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS( mRootSignature.ReleaseAndGetAddressOf() ) ) );
	}

	{
		std::vector csBlob = DX::ReadData( L"./IBL_SpecularPrefilterCS.cso" );

		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = mRootSignature.Get();
		computePsoDesc.CS = CD3DX12_SHADER_BYTECODE( csBlob.data(), csBlob.size() );

		ThrowIfFailed( inDevice->CreateComputePipelineState( &computePsoDesc, IID_PPV_ARGS( mPipelineState.ReleaseAndGetAddressOf() ) ) );
	}
}

void SpecularPrefilterIBL::Execute( ID3D12GraphicsCommandList *inCommandList, EnvironmentResources &inResources, float inMipBias )
{
	PIXBeginEvent( inCommandList, PIX_COLOR_DEFAULT, L"SpecularPrefilterIBL" );

	// If previously created, transition back to UAV
	if ( inResources.mEnvironmentDataLoaded ) {
		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition( inResources.mSpecularCubemap.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
		inCommandList->ResourceBarrier( 1, &barrier );
	}

	inCommandList->SetPipelineState( mPipelineState.Get() );
	inCommandList->SetComputeRootSignature( mRootSignature.Get() );

	UINT resolution[2] = { inResources.mCubemapResolution, inResources.mCubemapResolution };

	for ( UINT i = 0; i < inResources.kRoughnessLevelsIBL; ++i ) {
		inCommandList->SetComputeRoot32BitConstants( 0, 2, resolution, 0 );

		float roughness = std::max( static_cast<float>( i ) / ( inResources.kRoughnessLevelsIBL - 1 ), 0.05f );
		inCommandList->SetComputeRoot32BitConstants( 0, 1, &roughness, 3 );
		inCommandList->SetComputeRoot32BitConstants( 0, 1, &inMipBias, 4 );

		inCommandList->SetComputeRootDescriptorTable( 1, inResources.mUnfilteredCubemapSrvHandleGPU );
		inCommandList->SetComputeRootDescriptorTable( 2, inResources.mSpecularCubemapUavHandleGPU[i] );

		for ( UINT j = 0; j < 6; ++j ) {
			inCommandList->SetComputeRoot32BitConstant( 0, j, 2 );
			inCommandList->Dispatch( resolution[0], resolution[1], 1 );
		}

		resolution[0] /= 2;
		resolution[1] /= 2;
	}

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition( inResources.mSpecularCubemap.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );
	inCommandList->ResourceBarrier( 1, &barrier );

	PIXEndEvent( inCommandList );
}