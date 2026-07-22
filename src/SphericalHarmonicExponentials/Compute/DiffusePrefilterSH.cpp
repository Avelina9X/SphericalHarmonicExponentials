#include "pch.hpp"

#include "DiffusePrefilterSH.hpp"

#include "Utils/ReadData.hpp"

void DiffusePrefilterSH::CreateResources( ID3D12Device *inDevice, D3D_ROOT_SIGNATURE_VERSION inVersion )
{
	// Create harmonics array
	{
		CD3DX12_HEAP_PROPERTIES defaultHeap( D3D12_HEAP_TYPE_DEFAULT );

		const UINT numGroups = ( EnvironmentResources::kDiffuseCubemapResolution / 16 ) * ( EnvironmentResources::kDiffuseCubemapResolution / 16 ) * 6;

		const UINT harmonicCoeffBytes = sizeof( float ) * 9 * 3 + sizeof( float ); // 9*3 floats + pad float 
		const UINT harmonicArrayBytes = harmonicCoeffBytes * numGroups;

		CD3DX12_RESOURCE_DESC arrayBufferDesc = CD3DX12_RESOURCE_DESC::Buffer( harmonicArrayBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS );

		// Create GPU resources
		ThrowIfFailed( inDevice->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&arrayBufferDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS( mHarmonicsArray.ReleaseAndGetAddressOf() )
		) );

		mHarmonicsArrayAddress = mHarmonicsArray->GetGPUVirtualAddress();
	}

	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS  |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;

	// Prefilter PSO and RS
	{
		{
			CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
			ranges[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE );

			CD3DX12_ROOT_PARAMETER1 rootParameters[3];
			rootParameters[0].InitAsConstants( 2, 0 );
			rootParameters[1].InitAsDescriptorTable( 1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL );
			rootParameters[2].InitAsUnorderedAccessView( 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE );

			CD3DX12_STATIC_SAMPLER_DESC linearWrapSampler(
				0,
				D3D12_FILTER_ANISOTROPIC,
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				D3D12_TEXTURE_ADDRESS_MODE_WRAP
			);

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
			computeRootSignatureDesc.Init_1_1( _countof( rootParameters ), rootParameters, 1, &linearWrapSampler, rootSignatureFlags );

			Microsoft::WRL::ComPtr<ID3DBlob> signature;
			Microsoft::WRL::ComPtr<ID3DBlob> error;
			ThrowIfFailed( D3DX12SerializeVersionedRootSignature( &computeRootSignatureDesc, inVersion, &signature, &error ) );
			ThrowIfFailed( inDevice->CreateRootSignature( 0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS( mPrefilterRootSignature.ReleaseAndGetAddressOf() ) ) );
		}

		{
			std::vector csBlob = DX::ReadData( L"./SH_DiffusePrefilterCS.cso" );

			D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
			computePsoDesc.pRootSignature = mPrefilterRootSignature.Get();
			computePsoDesc.CS = CD3DX12_SHADER_BYTECODE( csBlob.data(), csBlob.size() );

			ThrowIfFailed( inDevice->CreateComputePipelineState( &computePsoDesc, IID_PPV_ARGS( mPrefilterPipelineState.ReleaseAndGetAddressOf() ) ) );
		}
	}

	// Accumulator PSO and RS
	{
		{
			CD3DX12_ROOT_PARAMETER1 rootParameters[4];
			rootParameters[0].InitAsConstants( 2, 0 );
			rootParameters[1].InitAsShaderResourceView( 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE );
			rootParameters[2].InitAsUnorderedAccessView( 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE );
			rootParameters[3].InitAsUnorderedAccessView( 1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE );

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
			computeRootSignatureDesc.Init_1_1( _countof( rootParameters ), rootParameters, 0, nullptr, rootSignatureFlags );

			Microsoft::WRL::ComPtr<ID3DBlob> signature;
			Microsoft::WRL::ComPtr<ID3DBlob> error;
			ThrowIfFailed( D3DX12SerializeVersionedRootSignature( &computeRootSignatureDesc, inVersion, &signature, &error ) );
			ThrowIfFailed( inDevice->CreateRootSignature( 0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS( mAccumulatorRootSignature.ReleaseAndGetAddressOf() ) ) );
		}

		{
			std::vector csBlob = DX::ReadData( L"./SH_DiffusePrefilterAccCS.cso" );

			D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
			computePsoDesc.pRootSignature = mAccumulatorRootSignature.Get();
			computePsoDesc.CS = CD3DX12_SHADER_BYTECODE( csBlob.data(), csBlob.size() );

			ThrowIfFailed( inDevice->CreateComputePipelineState( &computePsoDesc, IID_PPV_ARGS( mAccumulatorPipelineState.ReleaseAndGetAddressOf() ) ) );
		}
	}
}

void DiffusePrefilterSH::Execute( ID3D12GraphicsCommandList *inCommandList, EnvironmentResources &inResources )
{
	// Execute SH generation
	{
		CD3DX12_RESOURCE_BARRIER barrier1 = CD3DX12_RESOURCE_BARRIER::Transition( inResources.mDiffuseCubemap.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );
		inCommandList->ResourceBarrier( 1, &barrier1 );

		inCommandList->SetPipelineState( mPrefilterPipelineState.Get() );
		inCommandList->SetComputeRootSignature( mPrefilterRootSignature.Get() );

		UINT groupCount[2] = { EnvironmentResources::kDiffuseCubemapResolution / 16, EnvironmentResources::kDiffuseCubemapResolution / 16 };
		inCommandList->SetComputeRoot32BitConstants( 0, 2, groupCount, 0 );

		inCommandList->SetComputeRootDescriptorTable( 1, inResources.mDiffuseCubemapSrvHandleGPU );
		inCommandList->SetComputeRootUnorderedAccessView( 2, mHarmonicsArrayAddress );

		inCommandList->Dispatch( groupCount[0], groupCount[1], 6 );

		CD3DX12_RESOURCE_BARRIER barrier2 = CD3DX12_RESOURCE_BARRIER::Transition( inResources.mDiffuseCubemap.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON );
		inCommandList->ResourceBarrier( 1, &barrier2 );
	}

	// Execute SH accumulation
	{
		CD3DX12_RESOURCE_BARRIER barriers1[2] = {
			CD3DX12_RESOURCE_BARRIER::Transition( inResources.mDiffuseHarmonics32.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS ),
			CD3DX12_RESOURCE_BARRIER::Transition( inResources.mDiffuseHarmonics16.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS )
		};
		inCommandList->ResourceBarrier( 2, barriers1 );

		inCommandList->SetPipelineState( mAccumulatorPipelineState.Get() );
		inCommandList->SetComputeRootSignature( mAccumulatorRootSignature.Get() );

		UINT groupCount[2] = { EnvironmentResources::kDiffuseCubemapResolution / 16, EnvironmentResources::kDiffuseCubemapResolution / 16 };
		inCommandList->SetComputeRoot32BitConstants( 0, 2, groupCount, 0 );

		inCommandList->SetComputeRootShaderResourceView( 1, mHarmonicsArrayAddress );
		inCommandList->SetComputeRootUnorderedAccessView( 2, inResources.mDiffuseHarmonics32Address );
		inCommandList->SetComputeRootUnorderedAccessView( 3, inResources.mDiffuseHarmonics16Address );

		inCommandList->Dispatch( 1, 1, 1 );

		CD3DX12_RESOURCE_BARRIER barriers2[2] = {
			CD3DX12_RESOURCE_BARRIER::Transition( inResources.mDiffuseHarmonics32.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON ),
			CD3DX12_RESOURCE_BARRIER::Transition( inResources.mDiffuseHarmonics16.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON )
		};
		inCommandList->ResourceBarrier( 2, barriers2 );
	}
}