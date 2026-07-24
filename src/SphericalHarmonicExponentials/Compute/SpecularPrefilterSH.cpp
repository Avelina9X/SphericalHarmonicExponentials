#include "pch.hpp"

#include "SpecularPrefilterSH.hpp"

#include "Utils/ReadData.hpp"

void SpecularPrefilterSH::CreateResources( ID3D12Device *inDevice, HeapAllocator &inAllocator, D3D_ROOT_SIGNATURE_VERSION inVersion )
{
	// Create specular collector resources
	{
		CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R16G16B16A16_FLOAT,
			kSpecularCollectorResolution,
			kSpecularCollectorResolution,
			kSpecularRoughnessLevelsSH,
			1
		);
		textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		CD3DX12_HEAP_PROPERTIES defaultHeap( D3D12_HEAP_TYPE_DEFAULT );

		// Create GPU resource for Image
		ThrowIfFailed( inDevice->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS( mSpecularCollector.ReleaseAndGetAddressOf() )
		) );

		CD3DX12_SHADER_RESOURCE_VIEW_DESC srvDesc = CD3DX12_SHADER_RESOURCE_VIEW_DESC::Tex2DArray( textureDesc.Format, kSpecularRoughnessLevelsSH, 1 );
		inAllocator.Allocate( &mSpecularCollectorSrvHandleCPU, &mSpecularCollectorSrvHandleGPU );
		inDevice->CreateShaderResourceView( mSpecularCollector.Get(), &srvDesc, mSpecularCollectorSrvHandleCPU );

		CD3DX12_UNORDERED_ACCESS_VIEW_DESC uavDesc = CD3DX12_UNORDERED_ACCESS_VIEW_DESC::Tex2DArray( textureDesc.Format, kSpecularRoughnessLevelsSH );
		inAllocator.Allocate( &mSpecularCollectorUavHandleCPU, &mSpecularCollectorUavHandleGPU );
		inDevice->CreateUnorderedAccessView( mSpecularCollector.Get(), nullptr, &uavDesc, mSpecularCollectorUavHandleCPU );
	}

	// Create specular accumulator
	{
		const UINT matrixByteWidth = sizeof( float ) * 33 * 36;
		static_assert( matrixByteWidth % 16 == 0 );

		const UINT totalByteWidth = matrixByteWidth * kSpecularCollectorResolution * kVerticlaGroupsPerDispatch;

		CD3DX12_HEAP_PROPERTIES defaultHeap( D3D12_HEAP_TYPE_DEFAULT );

		CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer( totalByteWidth, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS );

		ThrowIfFailed( inDevice->CreateCommittedResource(
			&defaultHeap,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS( mSpecularAccumulator.ReleaseAndGetAddressOf())
		) );

		mSpecularAccumulatorAddress = mSpecularAccumulator->GetGPUVirtualAddress();
	}

	// Create specular collector PSO and RS
	{
		{
			CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
			ranges[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE ); // TODO: check volatility
			ranges[1].Init( D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE ); // TODO: check volatility

			CD3DX12_ROOT_PARAMETER1 rootParameters[3];
			rootParameters[0].InitAsConstants( 6, 0 );
			rootParameters[1].InitAsDescriptorTable( 1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL );
			rootParameters[2].InitAsDescriptorTable( 1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL );

			CD3DX12_STATIC_SAMPLER_DESC linearWrapSampler(
				0,
				D3D12_FILTER_ANISOTROPIC,
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				D3D12_TEXTURE_ADDRESS_MODE_WRAP,
				D3D12_TEXTURE_ADDRESS_MODE_WRAP
			);

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
			computeRootSignatureDesc.Init_1_1( _countof( rootParameters ), rootParameters, 1, &linearWrapSampler ); // TODO: flags?

			Microsoft::WRL::ComPtr<ID3DBlob> signature;
			Microsoft::WRL::ComPtr<ID3DBlob> error;
			ThrowIfFailed( D3DX12SerializeVersionedRootSignature( &computeRootSignatureDesc, inVersion, &signature, &error ) );
			ThrowIfFailed( inDevice->CreateRootSignature( 0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS( mPrefilterRootSignature.ReleaseAndGetAddressOf() ) ) );
		}

		{
			std::vector csBlob = DX::ReadData( L"./SH_SpecularPrefilterCS.cso" );

			D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
			computePsoDesc.pRootSignature = mPrefilterRootSignature.Get();
			computePsoDesc.CS = CD3DX12_SHADER_BYTECODE( csBlob.data(), csBlob.size() );

			ThrowIfFailed( inDevice->CreateComputePipelineState( &computePsoDesc, IID_PPV_ARGS( mPrefilterPipelineState.ReleaseAndGetAddressOf() ) ) );
		}
	}

	// Create specular accumulator PSO and RS
	{
		{
			CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
			ranges[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE );

			CD3DX12_ROOT_PARAMETER1 rootParameters[3];
			rootParameters[0].InitAsConstants( 8, 0 );
			rootParameters[1].InitAsDescriptorTable( 1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL );
			rootParameters[2].InitAsUnorderedAccessView( 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_ALL );

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
			computeRootSignatureDesc.Init_1_1( _countof( rootParameters ), rootParameters, 0, nullptr ); // TODO: flags?

			Microsoft::WRL::ComPtr<ID3DBlob> signature;
			Microsoft::WRL::ComPtr<ID3DBlob> error;
			ThrowIfFailed( D3DX12SerializeVersionedRootSignature( &computeRootSignatureDesc, inVersion, &signature, &error ) );
			ThrowIfFailed( inDevice->CreateRootSignature( 0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS( mAccumulatorRootSignature.ReleaseAndGetAddressOf() ) ) );
		}

		{
			std::vector csBlob = DX::ReadData( L"./SH_SpecularPrefilterAccCS.cso" );

			D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
			computePsoDesc.pRootSignature = mAccumulatorRootSignature.Get();
			computePsoDesc.CS = CD3DX12_SHADER_BYTECODE( csBlob.data(), csBlob.size() );

			ThrowIfFailed( inDevice->CreateComputePipelineState( &computePsoDesc, IID_PPV_ARGS( mAccumulatorPipelineState.ReleaseAndGetAddressOf() ) ) );
		}
	}

	// Create specular final accumulator PSO and RS
	{
		{
			CD3DX12_ROOT_PARAMETER1 rootParameters[2];
			rootParameters[0].InitAsConstants( 1, 0 );
			rootParameters[1].InitAsUnorderedAccessView( 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_ALL );

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
			computeRootSignatureDesc.Init_1_1( _countof( rootParameters ), rootParameters, 0, nullptr ); // TODO: flags?

			Microsoft::WRL::ComPtr<ID3DBlob> signature;
			Microsoft::WRL::ComPtr<ID3DBlob> error;
			ThrowIfFailed( D3DX12SerializeVersionedRootSignature( &computeRootSignatureDesc, inVersion, &signature, &error ) );
			ThrowIfFailed( inDevice->CreateRootSignature( 0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS( mSumRootSignature.ReleaseAndGetAddressOf() ) ) );
		}

		{
			std::vector csBlob = DX::ReadData( L"./SH_SpecularSumCS.cso" );

			D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
			computePsoDesc.pRootSignature = mSumRootSignature.Get();
			computePsoDesc.CS = CD3DX12_SHADER_BYTECODE( csBlob.data(), csBlob.size() );

			ThrowIfFailed( inDevice->CreateComputePipelineState( &computePsoDesc, IID_PPV_ARGS( mSumPipelineState.ReleaseAndGetAddressOf() ) ) );
		}
	}

	// Create specular solver PSO and RS
	{
		{
			CD3DX12_ROOT_PARAMETER1 rootParameters[3];
			rootParameters[0].InitAsShaderResourceView( 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE, D3D12_SHADER_VISIBILITY_ALL );
			rootParameters[1].InitAsUnorderedAccessView( 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_ALL );
			rootParameters[2].InitAsUnorderedAccessView( 1, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE, D3D12_SHADER_VISIBILITY_ALL );

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
			computeRootSignatureDesc.Init_1_1( _countof( rootParameters ), rootParameters, 0, nullptr ); // TODO: flags?

			Microsoft::WRL::ComPtr<ID3DBlob> signature;
			Microsoft::WRL::ComPtr<ID3DBlob> error;
			ThrowIfFailed( D3DX12SerializeVersionedRootSignature( &computeRootSignatureDesc, inVersion, &signature, &error ) );
			ThrowIfFailed( inDevice->CreateRootSignature( 0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS( mSolverRootSignature.ReleaseAndGetAddressOf() ) ) );
		}

		{
			std::vector csBlob = DX::ReadData( L"./SH_SpecularSolverCS.cso" );

			D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
			computePsoDesc.pRootSignature = mSolverRootSignature.Get();
			computePsoDesc.CS = CD3DX12_SHADER_BYTECODE( csBlob.data(), csBlob.size() );

			ThrowIfFailed( inDevice->CreateComputePipelineState( &computePsoDesc, IID_PPV_ARGS( mSolverPipelineState.ReleaseAndGetAddressOf() ) ) );
		}
	}
}

void SpecularPrefilterSH::Execute( ID3D12GraphicsCommandList *inCommandList, EnvironmentResources &inResources )
{
	PIXBeginEvent( inCommandList, PIX_COLOR_DEFAULT, L"SpecularPrefilterSH" );

	assert( mSpecularRoughnessLevelChoice <= kSpecularRoughnessLevelsSH );

	// Execute specular collection
	{
		CD3DX12_RESOURCE_BARRIER barrier1 = CD3DX12_RESOURCE_BARRIER::Transition( mSpecularCollector.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
		inCommandList->ResourceBarrier( 1, &barrier1 );

		inCommandList->SetPipelineState( mPrefilterPipelineState.Get() );
		inCommandList->SetComputeRootSignature( mPrefilterRootSignature.Get() );

		UINT resolutionLevels[3] = { kSpecularCollectorResolution, kSpecularCollectorResolution, mSpecularRoughnessLevelChoice };
		float minMaxRoughness[2] = { mMinAlphaLevel, mMaxAlphaLevel };

		inCommandList->SetComputeRoot32BitConstants( 0, 3, resolutionLevels, 0 );
		inCommandList->SetComputeRoot32BitConstants( 0, 2, minMaxRoughness, 3 );

		inCommandList->SetComputeRootDescriptorTable( 1, inResources.mUnfilteredCubemapSrvHandleGPU );
		inCommandList->SetComputeRootDescriptorTable( 2, mSpecularCollectorUavHandleGPU );

		for ( UINT i = 0; i < mSpecularRoughnessLevelChoice; ++i ) {
			inCommandList->SetComputeRoot32BitConstant( 0, i, 5 );
			inCommandList->Dispatch( resolutionLevels[0], resolutionLevels[1], 1 );
		}

		CD3DX12_RESOURCE_BARRIER barrier2 = CD3DX12_RESOURCE_BARRIER::Transition( mSpecularCollector.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON );
		inCommandList->ResourceBarrier( 1, &barrier2 );
	}

	// Execute specular row accumulation
	{
		CD3DX12_RESOURCE_BARRIER barrier1 = CD3DX12_RESOURCE_BARRIER::Transition( mSpecularAccumulator.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
		inCommandList->ResourceBarrier( 1, &barrier1 );

		inCommandList->SetPipelineState( mAccumulatorPipelineState.Get() );
		inCommandList->SetComputeRootSignature( mAccumulatorRootSignature.Get() );

		UINT resolutionLevels[3] = { kSpecularCollectorResolution, kSpecularCollectorResolution, mSpecularRoughnessLevelChoice };
		float minMaxRoughness[2] = { mMinAlphaLevel, 1.0f };

		inCommandList->SetComputeRoot32BitConstants( 0, 3, resolutionLevels, 0 );
		inCommandList->SetComputeRoot32BitConstants( 0, 2, minMaxRoughness, 3 );

		inCommandList->SetComputeRootDescriptorTable( 1, mSpecularCollectorSrvHandleGPU );

		inCommandList->SetComputeRoot32BitConstant( 0, 1, 7 ); // Set clear

		for ( UINT i = 0; i < mSpecularRoughnessLevelChoice; ++i ) {
			for ( UINT j = 0; j < kSpecularCollectorResolution; j += kVerticlaGroupsPerDispatch ) {

				inCommandList->SetComputeRootUnorderedAccessView( 2, mSpecularAccumulatorAddress );
				inCommandList->SetComputeRoot32BitConstant( 0, i, 5 );
				inCommandList->SetComputeRoot32BitConstant( 0, j, 6 );
				assert( resolutionLevels[0] / 256 > 0 );
				assert( resolutionLevels[0] % 256 == 0 );
				inCommandList->Dispatch( resolutionLevels[0] / 256, kVerticlaGroupsPerDispatch, 1 );

				CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV( mSpecularAccumulator.Get() );
				inCommandList->ResourceBarrier( 1, &barrier );

				inCommandList->SetComputeRoot32BitConstant( 0, 0, 7 ); // Unset clear
			}
		}
	}

	// Execute final sum accumulation
	{
		inCommandList->SetPipelineState( mSumPipelineState.Get() );
		inCommandList->SetComputeRootSignature( mSumRootSignature.Get() );

		inCommandList->SetComputeRoot32BitConstant( 0, kSpecularCollectorResolution * kVerticlaGroupsPerDispatch, 0 );
		inCommandList->SetComputeRootUnorderedAccessView( 1, mSpecularAccumulatorAddress );

		inCommandList->Dispatch( 1, 1, 1 );

		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition( mSpecularAccumulator.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON );
		inCommandList->ResourceBarrier( 1, &barrier );
	}

	// Execute specular solve
	{
		CD3DX12_RESOURCE_BARRIER barriers1[2] = {
			CD3DX12_RESOURCE_BARRIER::Transition( inResources.mSpecularHarmonics32.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS ),
			CD3DX12_RESOURCE_BARRIER::Transition( inResources.mSpecularHarmonics16.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS )
		};
		inCommandList->ResourceBarrier( 2, barriers1 );

		inCommandList->SetPipelineState( mSolverPipelineState.Get() );
		inCommandList->SetComputeRootSignature( mSolverRootSignature.Get() );

		inCommandList->SetComputeRootShaderResourceView( 0, mSpecularAccumulatorAddress );
		inCommandList->SetComputeRootUnorderedAccessView( 1, inResources.mSpecularHarmonics32Address );
		inCommandList->SetComputeRootUnorderedAccessView( 2, inResources.mSpecularHarmonics16Address );

		inCommandList->Dispatch( 1, 1, 1 );

		CD3DX12_RESOURCE_BARRIER barriers2[2] = {
			CD3DX12_RESOURCE_BARRIER::Transition( inResources.mSpecularHarmonics32.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE ),
			CD3DX12_RESOURCE_BARRIER::Transition( inResources.mSpecularHarmonics16.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE )
		};
		inCommandList->ResourceBarrier( 2, barriers2 );
	}

	PIXEndEvent( inCommandList );
}