#pragma once

class HeapAllocator
{
public:
	void Create( ID3D12Device *inDevice, ID3D12DescriptorHeap *inHeap );
	void Destroy();
	void Allocate( D3D12_CPU_DESCRIPTOR_HANDLE *outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE *outGpuHandle );
	void Free( D3D12_CPU_DESCRIPTOR_HANDLE inCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE inGpuHandle );

protected:
	ID3D12DescriptorHeap *mHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_TYPE mHeapType = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;

	D3D12_CPU_DESCRIPTOR_HANDLE mHeapStartCpu;
	D3D12_GPU_DESCRIPTOR_HANDLE mHeapStartGpu;

	UINT mHeapHandleIncrement;
	std::vector<UINT> mFreeIndices;
};