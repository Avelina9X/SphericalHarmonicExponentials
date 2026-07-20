#pragma once

#include "Utils/HeapAllocator.hpp"
#include "Utils/EnvironmentParser.hpp"

#include "Resources/EnvironmentResources.hpp"

#include "Compute/IntegrateBRDF.hpp"
#include "Compute/CubemapConverter.hpp"
#include "Compute/DiffusePrefilterIBL.hpp"
#include "Compute/SpecularPrefilterIBL.hpp"

#include "Graphics/Renderer.hpp"

class Application
{
public:
	~Application();

	void Initialize( HWND inWindow, int inWidth, int inHeight );

	void OnActivated() {}
	void OnDeactivated() {}
	void OnSuspending() {}
	void OnResuming() {}
	void OnWindowSizeChanged( UINT inWidth, UINT inHeight );
	void OnDestroy();

	void GetDefaultSize( LONG &outWidth, LONG &outHeight ) const noexcept
	{
		outWidth = 1920;
		outHeight = 1080;
	}

	void Tick();

protected:
	void Prepare();
	void Execute();
	void Clear();
	void Resolve();
	void Present();

	void CreateDeviceResources();
	void CreateSizedResources(); // Size dependent assets

	void WaitForGPU() noexcept;
	void MoveToNextFrame();

	void OnDeviceLost();
	void CreateDevice(); // Factory, Adaptor, Device
	void GetHardwareAdapter( IDXGIAdapter1 **outAdapter );

	static constexpr UINT kSampleCount = 4;
	static constexpr UINT kBackBufferCount = 3;
	static constexpr DXGI_FORMAT kBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	static constexpr DXGI_FORMAT kDepthBufferFormat = DXGI_FORMAT_D16_UNORM;


	// Application state
	HWND mWindow = nullptr;
	UINT mOutputWidth = 0;
	UINT mOutputHeight = 0;

	// Common D3D objects
	Microsoft::WRL::ComPtr<IDXGIAdapter1> mAdapter;
	Microsoft::WRL::ComPtr<ID3D12Device> mDevice;
	Microsoft::WRL::ComPtr<IDXGIFactory4> mFactory;

	// Sync objects
	UINT mBackBufferIndex = 0;
	UINT64 mFenceValues[kBackBufferCount] = {};
	Microsoft::WRL::ComPtr<ID3D12Fence> mFence;
	Microsoft::WRL::Wrappers::Event mFenceEvent;

	UINT64 mComputeFenceValues[kBackBufferCount] = {};
	Microsoft::WRL::ComPtr<ID3D12Fence> mComputeFence;
	Microsoft::WRL::Wrappers::Event mComputeFenceEvent;

	// Graphics pipeline
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCommandAllocators[kBackBufferCount];
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

	// Compute pipeline
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mComputeCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mComputeCommandAllocators[kBackBufferCount];
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mComputeCommandList;

	// Heaps
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;
	UINT mRtvDescriptorSize = 0;

	static constexpr UINT kSrvHeapSize = 1U << 16;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvHeap;
	HeapAllocator mSrvHeapAllocator;

	// Rendering resources
	Microsoft::WRL::ComPtr<IDXGISwapChain3> mSwapChain;
	Microsoft::WRL::ComPtr<ID3D12Resource> mRenderTargets[kBackBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> mMSAARenderTarget;
	Microsoft::WRL::ComPtr<ID3D12Resource> mDepthStencil;

	// App data
	std::unique_ptr<EnvironmentParser> mEnvironmentParser;

	std::unique_ptr<IntegrateBRDF> mIntegratedBRDF;
	std::unique_ptr<CubemapConverter> mCubemapConverter;
	std::unique_ptr<DiffusePrefilterIBL> mDiffusePrefilterIBL;
	std::unique_ptr<SpecularPrefilterIBL> mSpecularPrefilterIBL;

	std::unique_ptr<Renderer> mRenderer;

	std::string mSelectedEnvironment = "";
	std::map<std::string, EnvironmentResources> mEnvironmentResources;
};