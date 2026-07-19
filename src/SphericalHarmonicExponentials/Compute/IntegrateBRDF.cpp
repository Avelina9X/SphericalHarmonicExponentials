#include "pch.hpp"

#include "IntegrateBRDF.hpp"

#include "Utils/ReadData.hpp"

void IntegrateBRDF::CreateResources( ID3D12Device *inDevice, HeapAllocator &inAllocator, D3D_ROOT_SIGNATURE_VERSION inVersion )
{
	{
		CD3DX12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			DXGI_FORMAT_R16G16_FLOAT,
			kIntegratedBRDFResolution,
			kIntegratedBRDFResolution,
			1,
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
			IID_PPV_ARGS( mResource.ReleaseAndGetAddressOf() )
		) );

		CD3DX12_SHADER_RESOURCE_VIEW_DESC srvDesc = CD3DX12_SHADER_RESOURCE_VIEW_DESC::Tex2D( textureDesc.Format, 1 );
		inAllocator.Allocate( &mSrvHandleCPU, &mSrvHandleGPU );
		inDevice->CreateShaderResourceView( mResource.Get(), &srvDesc, mSrvHandleCPU );

		CD3DX12_UNORDERED_ACCESS_VIEW_DESC uavDesc = CD3DX12_UNORDERED_ACCESS_VIEW_DESC::Tex2D( textureDesc.Format );
		inAllocator.Allocate( &mUavHandleCPU, &mUavHandleGPU );
		inDevice->CreateUnorderedAccessView( mResource.Get(), nullptr, &uavDesc, mUavHandleCPU );

		mResource->SetName( L"Integrated BRDF" );
	}

	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		ranges[0].Init( D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE ); // TODO: check volatility

		CD3DX12_ROOT_PARAMETER1 rootParameters[2];
		rootParameters[0].InitAsConstants( 2, 0 );
		rootParameters[1].InitAsDescriptorTable( 1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL );

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS  |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC computeRootSignatureDesc;
		computeRootSignatureDesc.Init_1_1( _countof( rootParameters ), rootParameters, 0, nullptr, rootSignatureFlags );

		Microsoft::WRL::ComPtr<ID3DBlob> signature;
		Microsoft::WRL::ComPtr<ID3DBlob> error;
		ThrowIfFailed( D3DX12SerializeVersionedRootSignature( &computeRootSignatureDesc, inVersion, &signature, &error ) );
		ThrowIfFailed( inDevice->CreateRootSignature( 0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS( mRootSignature.ReleaseAndGetAddressOf() ) ) );

		mRootSignature->SetName( L"IntegrateBRDF RS" );
	}

	{
		std::vector csBlob = DX::ReadData( L"./IntegrateBRDF_CS.cso" );

		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = mRootSignature.Get();
		computePsoDesc.CS = CD3DX12_SHADER_BYTECODE( csBlob.data(), csBlob.size() );

		ThrowIfFailed( inDevice->CreateComputePipelineState( &computePsoDesc, IID_PPV_ARGS( mPipelineState.ReleaseAndGetAddressOf() ) ) );

		mPipelineState->SetName( L"IntegrateBRDF PSO" );
	}
}

void IntegrateBRDF::Execute( ID3D12GraphicsCommandList *inCommandList )
{
	inCommandList->SetPipelineState( mPipelineState.Get() );
	inCommandList->SetComputeRootSignature( mRootSignature.Get() );

	UINT groupCount[2] = { kIntegratedBRDFResolution, kIntegratedBRDFResolution };
	inCommandList->SetComputeRoot32BitConstants( 0, 2, groupCount, 0 );

	inCommandList->SetComputeRootDescriptorTable( 1, mUavHandleGPU );

	inCommandList->Dispatch( kIntegratedBRDFResolution, kIntegratedBRDFResolution, 1 );

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition( mResource.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON );
	inCommandList->ResourceBarrier( 1, &barrier );
}
