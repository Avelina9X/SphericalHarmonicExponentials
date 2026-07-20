#include "pch.hpp"

#include "Application.hpp"

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

#include <DirectXColors.h>

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#include <system_error>
#include <format>
#include <algorithm>

using Microsoft::WRL::ComPtr;


#pragma region State Changes

Application::~Application()
{}

void Application::Initialize( HWND inWindow, int inWidth, int inHeight )
{
	mWindow = inWindow;
	mOutputWidth = std::max( inWidth, 1 );
	mOutputHeight = std::max( inHeight, 1 );

	mEnvironmentParser = std::make_unique<EnvironmentParser>();
	mEnvironmentParser->ParseEnvironments();

	for ( const auto &[name, path] : mEnvironmentParser->mEnvironments ) {
		mEnvironmentResources.emplace( name, path );
	}

	mIntegratedBRDF = std::make_unique<IntegrateBRDF>();
	mCubemapConverter = std::make_unique<CubemapConverter>();
	mDiffusePrefilterIBL = std::make_unique<DiffusePrefilterIBL>();
	mSpecularPrefilterIBL = std::make_unique<SpecularPrefilterIBL>();

	mRenderer = std::make_unique<Renderer>( kBackBufferCount, 256 );

	CreateDevice();
	CreateDeviceResources();
	CreateSizedResources();
}

void Application::OnWindowSizeChanged( UINT inWidth, UINT inHeight )
{
	if ( !mWindow ) return;

	if ( mOutputWidth == inWidth && mOutputHeight == inHeight ) return;

	mOutputWidth = inWidth;
	mOutputHeight = inHeight;

	CreateSizedResources();
}

void Application::OnDestroy()
{
	WaitForGPU();

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	mSrvHeapAllocator.Destroy();

	for ( auto &[name, resources] : mEnvironmentResources ) {
		resources.Destroy();
	}

	for ( UINT i = 0; i < kBackBufferCount; ++i ) {
		mCommandAllocators[i].Reset();
		mComputeCommandAllocators[i].Reset();
		mRenderTargets[i].Reset();
	}
	mMSAARenderTarget.Reset();

	mDepthStencil.Reset();
	mFence.Reset();
	mComputeFence.Reset();
	mCommandList.Reset();
	mComputeCommandList.Reset();
	mSwapChain.Reset();
	mRtvHeap.Reset();
	mDsvHeap.Reset();
	mSrvHeap.Reset();
	mCommandQueue.Reset();
	mComputeCommandQueue.Reset();

	mDevice.Reset();
	mFactory.Reset();
}

#pragma endregion

static void sDrawCubemap( float inRes, D3D12_GPU_DESCRIPTOR_HANDLE *inHandles )
{
	ImVec2 origin = ImGui::GetCursorPos();

	ImVec2 imSize = { inRes, inRes };

	ImGui::SetCursorPos( { origin.x + inRes * 1, origin.y + inRes * 0 } );
	ImGui::Image( inHandles[2].ptr, imSize );

	ImGui::SetCursorPos( { origin.x + inRes * 0, origin.y + inRes * 1 } );
	ImGui::Image( inHandles[1].ptr, imSize );

	ImGui::SetCursorPos( { origin.x + inRes * 1, origin.y + inRes * 1 } );
	ImGui::Image( inHandles[4].ptr, imSize );

	ImGui::SetCursorPos( { origin.x + inRes * 2, origin.y + inRes * 1 } );
	ImGui::Image( inHandles[0].ptr, imSize );

	ImGui::SetCursorPos( { origin.x + inRes * 3, origin.y + inRes * 1 } );
	ImGui::Image( inHandles[5].ptr, imSize );

	ImGui::SetCursorPos( { origin.x + inRes * 1, origin.y + inRes * 2 } );
	ImGui::Image( inHandles[3].ptr, imSize );
}

void Application::Tick()
{

	const UINT64 completedGraphicsFenceValue = mFence->GetCompletedValue();
	const UINT64 currentGraphicsFenceValue = mFenceValues[mBackBufferIndex];

	ID3D12DescriptorHeap* ppHeaps[] = { mSrvHeap.Get() };
	Prepare();

	mCommandList->SetDescriptorHeaps( 1, ppHeaps );

	// Start the Dear ImGui frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGuiIO &io = ImGui::GetIO();

	ImGui::SetNextWindowPos( { 32.0f, 32.0f }, ImGuiCond_Once );
	ImGui::SetNextWindowSize( { 0.0f, ImGui::GetMainViewport()->Size.y / 2 - 32 }, ImGuiCond_Once );
	ImGui::Begin( "Settings", nullptr, ImGuiWindowFlags_AlwaysVerticalScrollbar );
	{

		float width = std::max( 256.0f, ImGui::GetContentRegionAvail().x );


		ImGui::Text( "Frame time %.3f ms (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate );
		//ImGui::Text( "Graphics Fences: %d %d %d", mFenceValues[0], mFenceValues[1], mFenceValues[2] );
		//ImGui::Text( "Compute Fences:  %d %d %d", mComputeFenceValues[0], mComputeFenceValues[1], mComputeFenceValues[2] );

		ImGui::Dummy( { 256.0f, 0.0f } );
		ImGui::SeparatorText( "Shading" );

		if ( ImGui::CollapsingHeader( "BRDF" ) ) {
			ImGui::Image( mIntegratedBRDF->GetShaderResourceView().ptr, { width, width } );
		}

		ImGui::Checkbox( "Enable IBL", &mRenderer->mEnableIBL );
		ImGui::Checkbox( "Enable SH", &mRenderer->mEnableSH );
		ImGui::Checkbox( "Half Precision SH", &mRenderer->mUseHalfSH );

		ImGui::Dummy( { 256.0f, 0.0f } );
		ImGui::SeparatorText( "HDRIs" );

		for ( auto &[name, resources] : mEnvironmentResources ) {
			if ( ImGui::CollapsingHeader( name.c_str() ) ) {
				ImGui::PushID( name.c_str() );
				assert( resources.mEquirectangularLoaded );

				ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_NoAutoOpenOnLog;

				if ( ImGui::Button( resources.mEnvironmentDataLoaded ? "Recompute" : "Compute" ) ) {
					//WaitForGPU();

					mCubemapConverter->Execute( mCommandList.Get(), resources );
					mDiffusePrefilterIBL->Execute( mCommandList.Get(), resources );
					mSpecularPrefilterIBL->Execute( mCommandList.Get(), resources );

					resources.mEnvironmentDataLoaded = true;
					mSelectedEnvironment = name;
				}

				if ( ImGui::TreeNodeEx( "Equirectangular Map", flags | ImGuiTreeNodeFlags_DefaultOpen ) )
					ImGui::Image( resources.mEquirectangularSrvHandleGPU.ptr, { width, width / 2 } );

				if ( resources.mEnvironmentDataLoaded ) {
					float cubeRes = width / 4;

					if ( ImGui::TreeNodeEx( "Unfiltered Cubemap", flags ) )
						sDrawCubemap( cubeRes, resources.mUnfilteredCubemapFaceHandleGPU );

					if ( ImGui::TreeNodeEx( "Diffuse Cubemap", flags ) )
						sDrawCubemap( cubeRes, resources.mDiffuseCubemapFaceHandleGPU );

					if ( ImGui::TreeNodeEx( "Specular Cubemap", flags ) )
						sDrawCubemap( cubeRes, resources.mSpecularCubemapFaceHandleGPU );
				}
				ImGui::PopID();
			}
		}
	}
	ImGui::End();

	Execute();

	// Update renderer data
	{
		if ( !ImGui::IsAnyItemActive() && ImGui::IsMouseDragging( ImGuiMouseButton_Middle ) ) {
			mRenderer->mCameraPitch += io.MouseDelta.y * 0.0025f;
			mRenderer->mCameraYaw -= io.MouseDelta.x * 0.0025f;

			mRenderer->mCameraPitch = std::clamp( mRenderer->mCameraPitch, -DirectX::XM_PIDIV2 * 0.99f, DirectX::XM_PIDIV2 * 0.99f );
			mRenderer->mCameraYaw = std::fmod( mRenderer->mCameraYaw, DirectX::XM_2PI );
		}

		mRenderer->mViewportWidth = mOutputWidth;
		mRenderer->mViewportHeight = mOutputHeight;

		mRenderer->CommitData( mBackBufferIndex );
	}

	Clear();

	mCommandList->SetDescriptorHeaps( 1, ppHeaps );

	if ( auto pair = mEnvironmentResources.find( mSelectedEnvironment ); pair != mEnvironmentResources.end() && pair->second.mEnvironmentDataLoaded ) {
		mRenderer->Draw( mCommandList.Get(), pair->second, mIntegratedBRDF->GetShaderResourceView() );
	}

	Resolve();

	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData( ImGui::GetDrawData(), mCommandList.Get() );
	if ( ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) {
		// Update and Render additional Platform Windows
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}

	Present();
}


#pragma region Resource Creation

void Application::CreateDeviceResources()
{
	// Set device to developer mode
	ThrowIfFailed( mDevice->SetStablePowerState( TRUE ) );

	// Create command queue
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		ThrowIfFailed( mDevice->CreateCommandQueue( &queueDesc, IID_PPV_ARGS( mCommandQueue.ReleaseAndGetAddressOf() ) ) );
		mCommandQueue->SetName( L"Graphics Queue" );

		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
		ThrowIfFailed( mDevice->CreateCommandQueue( &queueDesc, IID_PPV_ARGS( mComputeCommandQueue.ReleaseAndGetAddressOf() ) ) );
		mComputeCommandQueue->SetName( L"Compute Queue" );
	}

	// Create RTV and DSV heaps
	{
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = kBackBufferCount + 1;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.NumDescriptors = kSrvHeapSize;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		ThrowIfFailed( mDevice->CreateDescriptorHeap( &rtvHeapDesc, IID_PPV_ARGS( mRtvHeap.ReleaseAndGetAddressOf() ) ) );
		ThrowIfFailed( mDevice->CreateDescriptorHeap( &dsvHeapDesc, IID_PPV_ARGS( mDsvHeap.ReleaseAndGetAddressOf() ) ) );
		ThrowIfFailed( mDevice->CreateDescriptorHeap( &srvHeapDesc, IID_PPV_ARGS( mSrvHeap.ReleaseAndGetAddressOf() ) ) );

		mRtvDescriptorSize = mDevice->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );

		mSrvHeapAllocator.Create( mDevice.Get(), mSrvHeap.Get() );
	}

	// Create an allocator for each backbuffer in the swap chain
	for ( UINT i = 0; i < kBackBufferCount; ++i ) {
		ThrowIfFailed( mDevice->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( mCommandAllocators[i].ReleaseAndGetAddressOf() ) ) );
		ThrowIfFailed( mDevice->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS( mComputeCommandAllocators[i].ReleaseAndGetAddressOf() ) ) );
	}

	// Create a command list for recording graphics commands and keep it open
	ThrowIfFailed( mDevice->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT, mCommandAllocators[0].Get(), nullptr, IID_PPV_ARGS( mCommandList.ReleaseAndGetAddressOf() ) ) );
	mCommandList->SetName( L"Graphics Command List" );

	// Create a fence for tracking GPU execution progress
	ThrowIfFailed( mDevice->CreateFence( mFenceValues[mBackBufferIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( mFence.ReleaseAndGetAddressOf() ) ) );
	mFenceValues[mBackBufferIndex]++;

	mFenceEvent.Attach( CreateEventEx( nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE ) );
	if ( !mFenceEvent.IsValid() ) {
		throw std::system_error( std::error_code( static_cast<int>( GetLastError() ), std::system_category() ), "CreateEventEx" );
	}

	// Create a command list for recording compute commands and keep it open
	ThrowIfFailed( mDevice->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_COMPUTE, mComputeCommandAllocators[0].Get(), nullptr, IID_PPV_ARGS( mComputeCommandList.ReleaseAndGetAddressOf() ) ) );
	mComputeCommandList->SetName( L"Compute Command List" );

	// Create a fence for tracking GPU execution progress
	ThrowIfFailed( mDevice->CreateFence( mComputeFenceValues[mBackBufferIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( mComputeFence.ReleaseAndGetAddressOf() ) ) );
	mComputeFenceValues[mBackBufferIndex]++;

	mComputeFenceEvent.Attach( CreateEventEx( nullptr, nullptr, 0, EVENT_MODIFY_STATE | SYNCHRONIZE ) );
	if ( !mComputeFenceEvent.IsValid() ) {
		throw std::system_error( std::error_code( static_cast<int>( GetLastError() ), std::system_category() ), "CreateEventEx" );
	}

	// Check SM6.0 support
	D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = { D3D_SHADER_MODEL_6_10 };
	if ( FAILED( mDevice->CheckFeatureSupport( D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof( shaderModel ) ) )
		|| ( shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_0 ) ) {
		throw std::runtime_error( "Shader Model 6.0 is not supported!" );
	}
	else {
		OutputDebugStringA( std::format( "Shader Model {:x} supported!\n", static_cast<uint32_t>( shaderModel.HighestShaderModel ) ).c_str() );
	}

	D3D12_FEATURE_DATA_ROOT_SIGNATURE rootFeatureData = {};
	rootFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if ( FAILED( mDevice->CheckFeatureSupport( D3D12_FEATURE_ROOT_SIGNATURE, &rootFeatureData, sizeof( rootFeatureData ) ) ) ) {
		rootFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	// ImGui Init
	{
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch
		io.IniFilename = NULL;
		//io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; 

		// Setup Platform backend
		ImGui_ImplWin32_Init( mWindow );

		// Setup Renderer backend
		ImGui_ImplDX12_InitInfo init_info = {};
		init_info.Device = mDevice.Get();
		init_info.CommandQueue = mCommandQueue.Get();
		init_info.NumFramesInFlight = kBackBufferCount;
		init_info.RTVFormat = kBackBufferFormat;
		init_info.UserData = &mSrvHeapAllocator;

		// Allocating SRV descriptors (for textures) is up to the application, so we provide callbacks.
		// The example_win32_directx12/main.cpp application include a simple free-list based allocator.
		init_info.SrvDescriptorHeap = mSrvHeap.Get();
		init_info.SrvDescriptorAllocFn = []( ImGui_ImplDX12_InitInfo *inInitInfo, D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle ) {
			HeapAllocator *srvHeapAllocator = reinterpret_cast<HeapAllocator *>( inInitInfo->UserData );
			srvHeapAllocator->Allocate( outCpuHandle, outGpuHandle );
		};
		init_info.SrvDescriptorFreeFn = []( ImGui_ImplDX12_InitInfo *inInitInfo, D3D12_CPU_DESCRIPTOR_HANDLE inCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE inGpuHandle ) {
			HeapAllocator *srvHeapAllocator = reinterpret_cast<HeapAllocator *>( inInitInfo->UserData );
			srvHeapAllocator->Free( inCpuHandle, inGpuHandle );
		};

		ImGui_ImplDX12_Init( &init_info );
	}

	// Load all environments
	{
		for ( auto &[name, resources] : mEnvironmentResources ) {
			resources.LoadTexture( mDevice.Get(), mCommandList.Get(), mSrvHeapAllocator, mFenceValues[mBackBufferIndex] );
		}
	}

	// Load Renderer
	mRenderer->CreateResources( mDevice.Get(), mCommandList.Get(), mSrvHeapAllocator, mFenceValues[mBackBufferIndex] );
	mRenderer->LoadShaders( mDevice.Get(), kBackBufferFormat, kDepthBufferFormat, kSampleCount, rootFeatureData.HighestVersion );

	// Create all compute resources
	{
		mIntegratedBRDF->CreateResources( mDevice.Get(), mSrvHeapAllocator, rootFeatureData.HighestVersion );
		mCubemapConverter->CreateResources( mDevice.Get(), rootFeatureData.HighestVersion );
		mDiffusePrefilterIBL->CreateResources( mDevice.Get(), rootFeatureData.HighestVersion );
		mSpecularPrefilterIBL->CreateResources( mDevice.Get(), rootFeatureData.HighestVersion );
	}

	// Build BRDF
	mComputeCommandList->SetDescriptorHeaps( 1, mSrvHeap.GetAddressOf() );
	mIntegratedBRDF->Execute( mComputeCommandList.Get() );

	// Close compute command list and execute
	ThrowIfFailed( mComputeCommandList->Close() );
	ID3D12CommandList* computeCommandLists[] = { mComputeCommandList.Get() };
	mComputeCommandQueue->ExecuteCommandLists( 1, computeCommandLists );

	// Close graphics command list and execute
	ThrowIfFailed( mCommandList->Close() );
	ID3D12CommandList* graphicsCommandLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists( 1, graphicsCommandLists );

	// Wait for GPU to complete execution before going out of scope
	WaitForGPU();

	// Cleanup upload heaps
	{
		const UINT64 completedFence = mFence->GetCompletedValue();

		for ( auto &[name, resources] : mEnvironmentResources ) {
			resources.Cleanup( completedFence );
		}

		mRenderer->Cleanup( completedFence );
	}
}

void Application::CreateSizedResources()
{
	// Wait until all previous work GPU is complete
	WaitForGPU();

	// Release resources that are tied to the swap chain and update fence values.
	for ( UINT i = 0; i < kBackBufferCount; ++i )
	{
		mRenderTargets[i].Reset();
		mFenceValues[i] = mFenceValues[mBackBufferIndex];

		mComputeFenceValues[i] = mComputeFenceValues[mBackBufferIndex];
	}

	// If the swap chain doesn't exist create it
	if ( !mSwapChain ) {
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.Width = mOutputWidth;
		swapChainDesc.Height = mOutputHeight;
		swapChainDesc.Format = kBackBufferFormat;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = kBackBufferCount;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

		// Tearing support
		swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

		// Specify fullscreen desc
		DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullScreenDesc = {};
		fullScreenDesc.Windowed = TRUE;

		// Create a swap chain for the window
		ComPtr<IDXGISwapChain1> swapChain;
		ThrowIfFailed( mFactory->CreateSwapChainForHwnd(
			mCommandQueue.Get(),
			mWindow,
			&swapChainDesc,
			&fullScreenDesc,
			nullptr,
			swapChain.GetAddressOf()
		) );

		// Upcast to the SwapChain4 interface
		ThrowIfFailed( swapChain.As( &mSwapChain ) );

		// Disable exclusive fullscreen mode and implicit ALT+ENTER shortcut
		ThrowIfFailed( mFactory->MakeWindowAssociation( mWindow, DXGI_MWA_NO_ALT_ENTER ) );
	}
	// Otherwise resize it
	else {
		HRESULT hr = mSwapChain->ResizeBuffers( kBackBufferCount, mOutputWidth, mOutputHeight, kBackBufferFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING );

		if ( hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET ) {
			// If the device was removed for any reason, a new device and swap chain will need to be created.
			OnDeviceLost();

			// Everything is set up now. Do not continue execution of this method. OnDeviceLost will reenter this method
			// and correctly set up the new device.
			return;
		}
		else {
			ThrowIfFailed( hr );
		}
	}

	// Obtain back buffers for this window and create render target views for each of them
	for ( UINT i = 0; i < kBackBufferCount; ++i ) {
		ThrowIfFailed( mSwapChain->GetBuffer( i, IID_PPV_ARGS( mRenderTargets[i].GetAddressOf() ) ) ); // Note: no release, we Reset earlier

		const auto cpuHandle = mRtvHeap->GetCPUDescriptorHandleForHeapStart();

		const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor( cpuHandle, i, mRtvDescriptorSize );
		mDevice->CreateRenderTargetView( mRenderTargets[i].Get(), nullptr, rtvDescriptor );
	}

	// Reset backbuffer index to the current backbuffer
	mBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

	// Allocate a Tex2D resource for the depth buffer and create a DSV on this resource
	{
		// Use the default heap
		const CD3DX12_HEAP_PROPERTIES heapProperties( D3D12_HEAP_TYPE_DEFAULT );

		// Create depth stencil resource desc
		D3D12_RESOURCE_DESC depthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			kDepthBufferFormat,
			mOutputWidth,
			mOutputHeight,
			1,
			1,
			kSampleCount
		);
		depthStencilDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		// Set optimized clear value
		const CD3DX12_CLEAR_VALUE depthOptimizedClearValue( kDepthBufferFormat, 1.0f, 0u );

		// Create the depth stenicl resource
		ThrowIfFailed( mDevice->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&depthStencilDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS( mDepthStencil.ReleaseAndGetAddressOf() )
		) );

		// Define depth stencil view desc
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = kDepthBufferFormat;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;

		const auto cpuHandle = mDsvHeap->GetCPUDescriptorHandleForHeapStart();
		mDevice->CreateDepthStencilView( mDepthStencil.Get(), &dsvDesc, cpuHandle );
	}

	// Allocate the MSSA Tex2D and RTV
	{
		// Use the default heap
		const CD3DX12_HEAP_PROPERTIES heapProperties( D3D12_HEAP_TYPE_DEFAULT );

		// Create depth stencil resource desc
		D3D12_RESOURCE_DESC msaaDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			kBackBufferFormat,
			mOutputWidth,
			mOutputHeight,
			1,
			1,
			kSampleCount
		);
		msaaDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		// Set optimized clear value
		const CD3DX12_CLEAR_VALUE renderOptimizedClearValue( kBackBufferFormat, DirectX::Colors::Transparent );

		// Create the depth stenicl resource
		ThrowIfFailed( mDevice->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&msaaDesc,
			D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
			&renderOptimizedClearValue,
			IID_PPV_ARGS( mMSAARenderTarget.ReleaseAndGetAddressOf() )
		) );

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
		rtvDesc.Format = kBackBufferFormat;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;

		CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle( mRtvHeap->GetCPUDescriptorHandleForHeapStart(), kBackBufferCount, mRtvDescriptorSize );
		mDevice->CreateRenderTargetView( mMSAARenderTarget.Get(), &rtvDesc, cpuHandle );
	}

	OutputDebugStringA( std::format( "Resized to {}x{}\n", mOutputWidth, mOutputHeight ).c_str() );
}

#pragma endregion


#pragma region Prepare/Clear/Resolve/Present

void Application::Prepare()
{
	// Reset command list and allocator.
	ThrowIfFailed( mCommandAllocators[mBackBufferIndex]->Reset() );
	ThrowIfFailed( mCommandList->Reset( mCommandAllocators[mBackBufferIndex].Get(), nullptr ) );

	ThrowIfFailed( mComputeCommandAllocators[mBackBufferIndex]->Reset() );
	ThrowIfFailed( mComputeCommandList->Reset( mComputeCommandAllocators[mBackBufferIndex].Get(), nullptr ) );

	// Transition the render target into the correct state to allow for drawing into it.
	const D3D12_RESOURCE_BARRIER barriers[] =  {
		CD3DX12_RESOURCE_BARRIER::Transition(
			mMSAARenderTarget.Get(),
			D3D12_RESOURCE_STATE_RESOLVE_SOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		),
			CD3DX12_RESOURCE_BARRIER::Transition(
				mRenderTargets[mBackBufferIndex].Get(),
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_RESOLVE_DEST
			)
	};
	mCommandList->ResourceBarrier( 2, barriers );
}

void Application::Execute()
{
	PIXBeginEvent( mComputeCommandQueue.Get(), PIX_COLOR_DEFAULT, L"Execute" );

	ThrowIfFailed( mComputeCommandList->Close() );
	mComputeCommandQueue->ExecuteCommandLists( 1, CommandListCast( mComputeCommandList.GetAddressOf() ) );

	// Schedule a Signal command in the queue.
	const UINT64 currentFenceValue = mComputeFenceValues[mBackBufferIndex];
	ThrowIfFailed( mComputeCommandQueue->Signal( mComputeFence.Get(), currentFenceValue ) );

	// If the next frame is not ready to be rendered yet, wait until it is ready.
	if ( mComputeFence->GetCompletedValue() < mComputeFenceValues[mBackBufferIndex] ) {
		ThrowIfFailed( mComputeFence->SetEventOnCompletion( mComputeFenceValues[mBackBufferIndex], mComputeFenceEvent.Get() ) );
		std::ignore = WaitForSingleObjectEx( mComputeFenceEvent.Get(), INFINITE, FALSE );
	}

	// Set the fence value for the next frame.
	mComputeFenceValues[( mBackBufferIndex + 1 ) % kBackBufferCount] = currentFenceValue + 1;

	mCommandQueue->Wait( mComputeFence.Get(), currentFenceValue );

	PIXEndEvent( mComputeCommandQueue.Get() );
}

void Application::Clear()
{
	PIXBeginEvent( mCommandList.Get(), PIX_COLOR_DEFAULT, L"Clear" );

	// Clear the RTV and DSV
	{
		const auto cpuHandleRTV = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
		const auto cpuHandleDSV = mDsvHeap->GetCPUDescriptorHandleForHeapStart();

		const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor( cpuHandleRTV, kBackBufferCount, mRtvDescriptorSize );
		const CD3DX12_CPU_DESCRIPTOR_HANDLE dsvDescriptor( cpuHandleDSV );

		mCommandList->OMSetRenderTargets( 1, &rtvDescriptor, FALSE, &dsvDescriptor );
		mCommandList->ClearRenderTargetView( rtvDescriptor, DirectX::Colors::Transparent, 0, nullptr );
		mCommandList->ClearDepthStencilView( dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr );
	}

	// Set viewport and scissor rect
	{
		const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>( mOutputWidth ), static_cast<float>( mOutputHeight ), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
		const D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>( mOutputWidth ), static_cast<LONG>( mOutputHeight ) };
		mCommandList->RSSetViewports( 1, &viewport );
		mCommandList->RSSetScissorRects( 1, &scissorRect );
	}

	PIXEndEvent( mCommandList.Get() );
}

void Application::Resolve()
{
	PIXBeginEvent( mCommandList.Get(), PIX_COLOR_DEFAULT, L"Resolve" );

	// Transition the MSAA RTV into resolve state
	{
		const D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			mMSAARenderTarget.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_RESOLVE_SOURCE
		);
		mCommandList->ResourceBarrier( 1, &barrier );
	}

	mCommandList->ResolveSubresource( mRenderTargets[mBackBufferIndex].Get(), 0, mMSAARenderTarget.Get(), 0, kBackBufferFormat );

	// Transition the backbuffer RTV into the render state
	{
		const D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			mRenderTargets[mBackBufferIndex].Get(),
			D3D12_RESOURCE_STATE_RESOLVE_DEST,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		);
		mCommandList->ResourceBarrier( 1, &barrier );
	}

	// Clear the RTV and DSV
	{
		const auto cpuHandleRTV = mRtvHeap->GetCPUDescriptorHandleForHeapStart();
		const CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor( cpuHandleRTV, mBackBufferIndex, mRtvDescriptorSize );
		mCommandList->OMSetRenderTargets( 1, &rtvDescriptor, FALSE, nullptr );
	}

	// Set viewport and scissor rect
	{
		const D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>( mOutputWidth ), static_cast<float>( mOutputHeight ), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
		const D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>( mOutputWidth ), static_cast<LONG>( mOutputHeight ) };
		mCommandList->RSSetViewports( 1, &viewport );
		mCommandList->RSSetScissorRects( 1, &scissorRect );
	}

	PIXEndEvent( mCommandList.Get() );
}

void Application::Present()
{
	PIXBeginEvent( mCommandQueue.Get(), PIX_COLOR_DEFAULT, L"Present" );

	// Transition the backbuffer RTV into the present state
	{
		const D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			mRenderTargets[mBackBufferIndex].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT
		);
		mCommandList->ResourceBarrier( 1, &barrier );
	}

	// Close and execute the command list
	ThrowIfFailed( mCommandList->Close() );
	mCommandQueue->ExecuteCommandLists( 1, CommandListCast( mCommandList.GetAddressOf() ) );

	// Present without vsync
	HRESULT hr = mSwapChain->Present( 0, DXGI_PRESENT_ALLOW_TEARING );
	//HRESULT hr = mSwapChain->Present( 2, 0 );

	// If the device was reset we must completely reinitialize the renderer
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
		OnDeviceLost();
	}
	else {
		ThrowIfFailed( hr );
		MoveToNextFrame();
	}

	PIXEndEvent( mCommandQueue.Get() );
}

#pragma endregion


#pragma region GPU Sync

void Application::WaitForGPU() noexcept
{
	if ( mCommandQueue && mFence && mFenceEvent.IsValid() ) {
		// Schedule a Signal command in the GPU queue.
		UINT64 fenceValue = mFenceValues[mBackBufferIndex];
		UINT64 computeFenceValue = mComputeFenceValues[mBackBufferIndex];

		ThrowIfFailed( mCommandQueue->Signal( mFence.Get(), fenceValue ) );
		ThrowIfFailed( mComputeCommandQueue->Signal( mComputeFence.Get(), computeFenceValue ) );

		if ( true ) {
			// Wait until the Signal has been processed.
			ThrowIfFailed( mFence->SetEventOnCompletion( fenceValue, mFenceEvent.Get() ) );
			ThrowIfFailed( mComputeFence->SetEventOnCompletion( computeFenceValue, mComputeFenceEvent.Get() ) );

			if ( true ) {
				//std::ignore = WaitForSingleObjectEx( mFenceEvent.Get(), INFINITE, FALSE );

				HANDLE events[2] = { mFenceEvent.Get(), mComputeFenceEvent.Get() };
				std::ignore = WaitForMultipleObjectsEx( 2, events, TRUE, INFINITE, FALSE );

				// Increment the fence value for the current frame.
				mFenceValues[mBackBufferIndex]++;
				mComputeFenceValues[mBackBufferIndex]++;
			}
		}
	}
}

void Application::MoveToNextFrame()
{
	// Schedule a Signal command in the queue.
	const UINT64 currentFenceValue = mFenceValues[mBackBufferIndex];

	ThrowIfFailed( mCommandQueue->Signal( mFence.Get(), currentFenceValue ) );

	// Update the back buffer index.
	mBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

	// If the next frame is not ready to be rendered yet, wait until it is ready.
	if ( mFence->GetCompletedValue() < mFenceValues[mBackBufferIndex] ) {
		ThrowIfFailed( mFence->SetEventOnCompletion( mFenceValues[mBackBufferIndex], mFenceEvent.Get() ) );
		std::ignore = WaitForSingleObjectEx( mFenceEvent.Get(), INFINITE, FALSE );
	}

	// Set the fence value for the next frame.
	mFenceValues[mBackBufferIndex] = currentFenceValue + 1;
}

#pragma endregion


#pragma region Device Setup/Teardown

void Application::OnDeviceLost()
{
	OnDestroy();

	CreateDevice();
	CreateDeviceResources();
	CreateSizedResources();
}

void Application::CreateDevice()
{
	DWORD dxgiFactoryFlags = 0;

#ifdef _DEBUG
	{
		// Enable debug layer
		ComPtr<ID3D12Debug> debugController;
		if ( SUCCEEDED( D3D12GetDebugInterface( IID_PPV_ARGS( debugController.GetAddressOf() ) ) ) ) {
			debugController->EnableDebugLayer();
		}
		else {
			assert( false );
		}

		ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
		if ( SUCCEEDED( DXGIGetDebugInterface1( 0, IID_PPV_ARGS( dxgiInfoQueue.GetAddressOf() ) ) ) ) {
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

			dxgiInfoQueue->SetBreakOnSeverity( DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true );
			dxgiInfoQueue->SetBreakOnSeverity( DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true );
		}
		else {
			assert( false );
		}
	}
#endif

	// Create the factory
	ThrowIfFailed( CreateDXGIFactory2( dxgiFactoryFlags, IID_PPV_ARGS( mFactory.ReleaseAndGetAddressOf() ) ) );

	// Create adaptor
	GetHardwareAdapter( mAdapter.ReleaseAndGetAddressOf() );

	// Create the DX12 device
	ThrowIfFailed( D3D12CreateDevice(
		mAdapter.Get(),
		D3D_FEATURE_LEVEL_12_1,
		IID_PPV_ARGS( mDevice.ReleaseAndGetAddressOf() )
	) );

#ifndef NDEBUG
	// Configure debug device (if active).
	ComPtr<ID3D12InfoQueue> d3dInfoQueue;
	if ( SUCCEEDED( mDevice.As( &d3dInfoQueue ) ) ) {
	#ifdef _DEBUG
		d3dInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_CORRUPTION, true );
		d3dInfoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_ERROR, true );
	#endif
		D3D12_MESSAGE_ID hide[] =
		{
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
			// Workarounds for debug layer issues on hybrid-graphics systems
			D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_WRONGSWAPCHAINBUFFERREFERENCE,
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
		};
		D3D12_INFO_QUEUE_FILTER filter = {};
		filter.DenyList.NumIDs = static_cast<UINT>( std::size( hide ) );
		filter.DenyList.pIDList = hide;
		d3dInfoQueue->AddStorageFilterEntries( &filter );
	}
	else {
		assert( false );
	}
#endif
}

void Application::GetHardwareAdapter( IDXGIAdapter1 **outAdapter )
{
	*outAdapter = nullptr;

	ComPtr<IDXGIAdapter1> adapter;

	ComPtr<IDXGIFactory6> factory6;
	if ( SUCCEEDED( mFactory->QueryInterface( IID_PPV_ARGS( &factory6 ) ) ) ) {
		for ( UINT adapterIndex = 0; SUCCEEDED( factory6->EnumAdapterByGpuPreference( adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS( &adapter ) ) ); ++adapterIndex ) {
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1( &desc );

			if ( desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE ) {
				// Don't select the Basic Render Driver adapter.
				// If you want a software adapter, pass in "/warp" on the command line.
				continue;
			}

			// Check to see whether the adapter supports Direct3D 12, but don't create the
			// actual device yet.
			if ( SUCCEEDED( D3D12CreateDevice( adapter.Get(), D3D_FEATURE_LEVEL_12_1, __uuidof( ID3D12Device ), nullptr ) ) ) {
				break;
			}
		}
	}

	if ( adapter.Get() == nullptr ) {
		for ( UINT adapterIndex = 0; SUCCEEDED( mFactory->EnumAdapters1( adapterIndex, &adapter ) ); ++adapterIndex ) {
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1( &desc );

			if ( desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE ) {
				// Don't select the Basic Render Driver adapter.
				// If you want a software adapter, pass in "/warp" on the command line.
				continue;
			}

			// Check to see whether the adapter supports Direct3D 12, but don't create the
			// actual device yet.
			if ( SUCCEEDED( D3D12CreateDevice( adapter.Get(), D3D_FEATURE_LEVEL_12_1, __uuidof( ID3D12Device ), nullptr ) ) ) {
				break;
			}
		}
	}

	*outAdapter = adapter.Detach();
}

#pragma endregion
