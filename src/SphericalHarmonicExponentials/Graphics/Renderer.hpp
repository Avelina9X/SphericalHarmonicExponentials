#pragma once

#include "Utils/HeapAllocator.hpp"
#include "Resources/EnvironmentResources.hpp"

struct RendererData
{
	DirectX::XMVECTOR EyePosition;
	DirectX::XMMATRIX ViewProj;
	DirectX::XMFLOAT3 Albedo = { 1.0f, 1.0f, 1.0f };
	float Roughness = 0.5;
	float Metallic = 1.0f;
};

class Renderer
{
public:
	Renderer( UINT inFrameCount, UINT inSphereTessellation = 256 );

	void CreateResources( ID3D12Device *inDevice, ID3D12GraphicsCommandList *inCommandList, HeapAllocator &inAllocator, UINT64 inCurrentGraphicsFenceValue );
	void LoadShaders( ID3D12Device *inDevice, DXGI_FORMAT inBackBufferFormat, DXGI_FORMAT inDepthBufferFormat, UINT inSampleCount, D3D_ROOT_SIGNATURE_VERSION inVersion );
	void Cleanup( UINT64 inCompletedGraphicsFenceValue );
	void Destroy();

	void CommitData( UINT inFrameIndex );
	void Draw( ID3D12GraphicsCommandList *inCommandList, EnvironmentResources &inResources, D3D12_GPU_DESCRIPTOR_HANDLE inBRDF ) const;

	RendererData mRendererData = {};

	size_t mViewportWidth = 0;
	size_t mViewportHeight = 0;
	float mCameraPitch = 0.0f;
	float mCameraYaw = DirectX::XM_PIDIV2;
	bool mEnableIBL = true;
	bool mEnableSH = true;
	bool mUseHalfSH = false;

protected:
	// Upload data
	UINT64 mUploadFenceValue = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource> mUploadVBO;
	Microsoft::WRL::ComPtr<ID3D12Resource> mUploadIBO;

	// Sphere data
	Microsoft::WRL::ComPtr<ID3D12Resource> mSphereVBO;
	Microsoft::WRL::ComPtr<ID3D12Resource> mSphereIBO;
	D3D12_VERTEX_BUFFER_VIEW mSphereVertexBufferView;
	D3D12_INDEX_BUFFER_VIEW mSphereIndexBufferView;
	UINT mSphereTessellation = 0;
	UINT mSphereIndexCount = 0;

	// Frame resources
	UINT mFrameCount = 0;
	UINT mFrameIndex = 0;

	struct FrameResource
	{
		Microsoft::WRL::ComPtr<ID3D12Resource> mResource;
		D3D12_GPU_VIRTUAL_ADDRESS mBufferAddress;
		//D3D12_CPU_DESCRIPTOR_HANDLE mHandleCPU;
		//D3D12_GPU_DESCRIPTOR_HANDLE mHandleGPU;
		void *mDataPointer = nullptr;
	};
	std::vector<FrameResource> mFrameResources;

	// Shading states
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mShadingRootSignatureIBL;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mShadingPipelineStateIBL;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mShadingRootSignatureSH;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mShadingPipelineStateSH32;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mShadingPipelineStateSH16;

	// Vertex Data
	struct VertexData
	{
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Normal;
	};

	void ComputeSphere( std::vector<VertexData> &ioVertices, std::vector<UINT> &ioIndices ) const;
};