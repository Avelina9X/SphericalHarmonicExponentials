#include "pch.hpp"

#include "HeapAllocator.hpp"

void HeapAllocator::Create( ID3D12Device *inDevice, ID3D12DescriptorHeap *inHeap )
{
	assert( mHeap == nullptr && mFreeIndices.empty() );

	mHeap = inHeap;

	D3D12_DESCRIPTOR_HEAP_DESC desc = inHeap->GetDesc();
	mHeapType = desc.Type;
	mHeapStartCpu = inHeap->GetCPUDescriptorHandleForHeapStart();
	mHeapStartGpu = inHeap->GetGPUDescriptorHandleForHeapStart();
	mHeapHandleIncrement = inDevice->GetDescriptorHandleIncrementSize( mHeapType );

	mFreeIndices.reserve( desc.NumDescriptors );

	for ( UINT n = desc.NumDescriptors; n > 0; --n ) {
		mFreeIndices.push_back( n - 1 );
	}
}

void HeapAllocator::Destroy()
{
	mHeap = nullptr;
	mFreeIndices.clear();
}

void HeapAllocator::Allocate( D3D12_CPU_DESCRIPTOR_HANDLE *outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE *outGpuHandle )
{
	assert( !mFreeIndices.empty() );

	UINT index = mFreeIndices.back();
	mFreeIndices.pop_back();

	*outCpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE( mHeapStartCpu, index, mHeapHandleIncrement );
	*outGpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE( mHeapStartGpu, index, mHeapHandleIncrement );
}

void HeapAllocator::Free( D3D12_CPU_DESCRIPTOR_HANDLE inCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE inGpuHandle )
{
	UINT cpuIdx = static_cast<UINT>( ( inCpuHandle.ptr - mHeapStartCpu.ptr ) / mHeapHandleIncrement );
	UINT gpuIdx = static_cast<UINT>( ( inGpuHandle.ptr - mHeapStartGpu.ptr ) / mHeapHandleIncrement );

	assert( cpuIdx == gpuIdx );

	mFreeIndices.push_back( cpuIdx );
}
