#pragma once

#include "Utils/HeapAllocator.hpp"
#include "Utils/EnvironmentParser.hpp"
#include "Utils/TextureParser.hpp"
#include "Utils/MaterialParser.hpp"

#include "Resources/EnvironmentResources.hpp"
#include "Resources/TextureResource.hpp"
#include "Resources/MaterialResource.hpp"

#include "Compute/IntegrateBRDF.hpp"
#include "Compute/CubemapConverter.hpp"
#include "Compute/DiffusePrefilterIBL.hpp"
#include "Compute/SpecularPrefilterIBL.hpp"
#include "Compute/DiffusePrefilterSH.hpp"
#include "Compute/SpecularPrefilterSH.hpp"

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
	void SetMaterialData( const std::string &inName, const MaterialResource &inMaterial );
	void ComputeEnvironmentData( const std::string &inName, EnvironmentResources &inResources );

	void Tick();

protected:
	void Prepare();
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

	// Graphics pipeline
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCommandAllocators[kBackBufferCount];
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;

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
	std::map<std::string, EnvironmentResources> mEnvironmentResources;
	std::string mSelectedEnvironment = "";

	std::unique_ptr<TextureParser> mTextureParser;
	std::map<std::string, TextureResource> mTextureResources;

	std::unique_ptr<MaterialParser> mMaterialParser;
	std::map<std::string, MaterialResource> mMaterialResources;
	std::string mSelectedMaterial = "_default";

	std::unique_ptr<IntegrateBRDF> mIntegratedBRDF;
	std::unique_ptr<CubemapConverter> mCubemapConverter;
	std::unique_ptr<DiffusePrefilterIBL> mDiffusePrefilterIBL;
	std::unique_ptr<SpecularPrefilterIBL> mSpecularPrefilterIBL;
	std::unique_ptr<DiffusePrefilterSH> mDiffusePrefilterSH;
	std::unique_ptr<SpecularPrefilterSH> mSpecularPrefilterSH;

	std::unique_ptr<Renderer> mRenderer;

	float mClampValue = 1000.0f;
	float mSpecularPrefilterMipBias = 1.0f;

	bool mUncappedFramerate = false;
};