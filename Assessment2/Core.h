#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>
#include <vector>

#pragma comment(lib, "d3d12")
#pragma comment(lib, "dxgi")

#define USE_DEBUG

class GPUFence
{
public:
	ID3D12Fence* fence;
	HANDLE eventHandle;
	long long value;
	void create(ID3D12Device5* device)
	{
		value = 0;
		device->CreateFence(value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
		eventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
	}
	void signal(ID3D12CommandQueue* queue)
	{
		queue->Signal(fence, ++value);
	}
	void wait()
	{
		if (fence->GetCompletedValue() < value)
		{
			fence->SetEventOnCompletion(value, eventHandle);
			WaitForSingleObject(eventHandle, INFINITE);
		}
	}
	~GPUFence()
	{
		CloseHandle(eventHandle);
		fence->Release();
	}
};

class Barrier
{
public:
	static void add(ID3D12Resource *res, D3D12_RESOURCE_STATES first, D3D12_RESOURCE_STATES second, ID3D12GraphicsCommandList4* commandList)
	{
		D3D12_RESOURCE_BARRIER rb;
		memset(&rb, 0, sizeof(D3D12_RESOURCE_BARRIER));
		rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		rb.Transition.pResource = res;
		rb.Transition.StateBefore = first;
		rb.Transition.StateAfter = second;
		rb.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &rb);
	}
};

class DescriptorHeap
{
public:
	ID3D12DescriptorHeap* heap;
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
	unsigned int incrementSize;
	int used;

	void init(ID3D12Device5* device, int num)
	{
		D3D12_DESCRIPTOR_HEAP_DESC uavcbvHeapDesc = {};
		uavcbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		uavcbvHeapDesc.NumDescriptors = num;
		uavcbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		device->CreateDescriptorHeap(&uavcbvHeapDesc, IID_PPV_ARGS(&heap));
		cpuHandle = heap->GetCPUDescriptorHandleForHeapStart();
		gpuHandle = heap->GetGPUDescriptorHandleForHeapStart();
		incrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		used = 0;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE getNextCPUHandle()
	{
		if (used > 0)
		{
			cpuHandle.ptr += incrementSize;
		}
		used++;
		return cpuHandle;
	}


};


class Core
{
public:
	ID3D12Device5* device;
	ID3D12CommandQueue* graphicsQueue;
	ID3D12CommandQueue* copyQueue;
	ID3D12CommandQueue* computeQueue;
	IDXGISwapChain3* swapchain;

	ID3D12DescriptorHeap* backbufferHeap;
	ID3D12Resource** backbuffers;

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle;
	ID3D12DescriptorHeap* dsvHeap;
	ID3D12Resource* dsv;

	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;

	ID3D12CommandAllocator* graphicsCommandAllocator[2];
	ID3D12GraphicsCommandList4* graphicsCommandList[2];
	ID3D12RootSignature* rootSignature;
	unsigned int srvTableIndex;
	GPUFence graphicsQueueFence[2];
	int width;
	int height;
	HWND windowHandle;
	DescriptorHeap srvHeap;
	void init(HWND hwnd, int _width, int _height)
	{
		// Find Adapter
		IDXGIFactory4* factory;
		CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory));
		ID3D12Debug1* debug;
#ifdef USE_DEBUG
		D3D12GetDebugInterface(IID_PPV_ARGS(&debug));
		debug->EnableDebugLayer();
		debug->Release();
#endif
		int i = 0;
		IDXGIAdapter1* adapterf;
		std::vector<IDXGIAdapter1*> adapters;
		while (factory->EnumAdapters1(i, &adapterf) != DXGI_ERROR_NOT_FOUND)
		{
			adapters.push_back(adapterf);
			i++;
		}
		DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		long long maxVideoMemory = 0;
		int useAdapterIndex = 0;
		for (int i = 0; i < adapters.size(); i++)
		{
			DXGI_ADAPTER_DESC desc;
			adapters[i]->GetDesc(&desc);
			if (desc.DedicatedVideoMemory > maxVideoMemory)
			{
				maxVideoMemory = desc.DedicatedVideoMemory;
				useAdapterIndex = i;
			}
		}
		IDXGIAdapter* adapter = adapters[useAdapterIndex];

		// Create Device
		D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device));

		for (auto& adapter : adapters)
		{
			adapter->Release();
		}

		// Create Queues
		D3D12_COMMAND_QUEUE_DESC graphicsQueueDesc;
		memset(&graphicsQueueDesc, 0, sizeof(D3D12_COMMAND_QUEUE_DESC));
		graphicsQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		device->CreateCommandQueue(&graphicsQueueDesc, IID_PPV_ARGS(&graphicsQueue));
		D3D12_COMMAND_QUEUE_DESC copyQueueDesc;
		memset(&copyQueueDesc, 0, sizeof(D3D12_COMMAND_QUEUE_DESC));
		copyQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
		device->CreateCommandQueue(&copyQueueDesc, IID_PPV_ARGS(&copyQueue));
		D3D12_COMMAND_QUEUE_DESC computeQueueDesc;
		memset(&computeQueueDesc, 0, sizeof(D3D12_COMMAND_QUEUE_DESC));
		computeQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
		device->CreateCommandQueue(&computeQueueDesc, IID_PPV_ARGS(&computeQueue));

		// Create Swapchain
		DXGI_SWAP_CHAIN_DESC1 scDesc;
		memset(&scDesc, 0, sizeof(DXGI_SWAP_CHAIN_DESC1));
		scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		scDesc.Width = _width;
		scDesc.Height = _height;
		scDesc.SampleDesc.Count = 1; 
		scDesc.SampleDesc.Quality = 0;
		scDesc.BufferCount = 2;
		scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		IDXGISwapChain1* swapChain1;
		factory->CreateSwapChainForHwnd(graphicsQueue, hwnd, &scDesc, nullptr, nullptr, &swapChain1);
		swapChain1->QueryInterface(&swapchain); 
		swapChain1->Release();

		factory->Release();

		D3D12_DESCRIPTOR_HEAP_DESC renderTargetViewHeapDesc;
		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle;
		memset(&renderTargetViewHeapDesc, 0, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
		renderTargetViewHeapDesc.NumDescriptors = scDesc.BufferCount;
		renderTargetViewHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		renderTargetViewHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		device->CreateDescriptorHeap(&renderTargetViewHeapDesc, IID_PPV_ARGS(&backbufferHeap));
		renderTargetViewHandle = backbufferHeap->GetCPUDescriptorHandleForHeapStart();
		backbuffers = new ID3D12Resource *[scDesc.BufferCount];
		backbuffers[0] = NULL;
		backbuffers[1] = NULL;

		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
		memset(&dsvHeapDesc, 0, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));
		dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
		dsv = NULL;

		width = _width;
		height = _height;
		updateScreenResources(_width, _height);

		device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&graphicsCommandAllocator[0]));
		device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&graphicsCommandList[0]));
		device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&graphicsCommandAllocator[1]));
		device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&graphicsCommandList[1]));

		graphicsQueueFence[0].create(device);
		graphicsQueueFence[1].create(device);

		srvHeap.init(device, 16384);
		createRootSignature();

		windowHandle = hwnd;
	}
	void updateScreenResources(int _width, int _height)
	{
		for (unsigned int i = 0; i < 2; i++)
		{
			if (backbuffers[i] != NULL)
			{
				backbuffers[i]->Release();
			}
		}
		if (_width != width || _height != height)
		{
			swapchain->ResizeBuffers(0, _width, _height, DXGI_FORMAT_UNKNOWN, 0);
		}
		DXGI_SWAP_CHAIN_DESC desc;
		swapchain->GetDesc(&desc);
		width = desc.BufferDesc.Width;
		height = desc.BufferDesc.Height;

		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle;
		renderTargetViewHandle = backbufferHeap->GetCPUDescriptorHandleForHeapStart();
		unsigned int renderTargetViewDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		for (unsigned int i = 0; i < 2; i++)
		{
			swapchain->GetBuffer(i, IID_PPV_ARGS(&backbuffers[i]));

			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
			rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; 
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

			device->CreateRenderTargetView(backbuffers[i], &rtvDesc, renderTargetViewHandle);
			renderTargetViewHandle.ptr += renderTargetViewDescriptorSize;
		}

		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = (float)width;
		viewport.Height = (float)height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		scissorRect.left = 0;
		scissorRect.top = 0;
		scissorRect.right = width;
		scissorRect.bottom = height;

		if (dsv != NULL)
		{
			dsv->Release();
		}
		D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
		depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;
		D3D12_CLEAR_VALUE depthClearValue = {};
		depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		depthClearValue.DepthStencil.Depth = 1.0f;
		depthClearValue.DepthStencil.Stencil = 0;
		D3D12_HEAP_PROPERTIES heapprops;
		memset(&heapprops, 0, sizeof(D3D12_HEAP_PROPERTIES));
		heapprops.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapprops.CreationNodeMask = 1;
		heapprops.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC dsvDesc;
		memset(&dsvDesc, 0, sizeof(D3D12_RESOURCE_DESC));
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.Width = width;
		dsvDesc.Height = height;
		dsvDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		dsvDesc.DepthOrArraySize = 1;
		dsvDesc.MipLevels = 1;
		dsvDesc.SampleDesc.Count = 1;
		dsvDesc.SampleDesc.Quality = 0;
		dsvDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		dsvDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		device->CreateCommittedResource(&heapprops, D3D12_HEAP_FLAG_NONE, &dsvDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue, __uuidof(ID3D12Resource), (void**)&dsv);
		device->CreateDepthStencilView(dsv, &depthStencilDesc, dsvHeap->GetCPUDescriptorHandleForHeapStart());
	}
	void createRootSignature()
	{
		std::vector<D3D12_ROOT_PARAMETER> parameters;
		D3D12_ROOT_PARAMETER rootParameterCBVS;
		rootParameterCBVS.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameterCBVS.Descriptor.ShaderRegister = 0; 
		rootParameterCBVS.Descriptor.RegisterSpace = 0;
		rootParameterCBVS.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		parameters.push_back(rootParameterCBVS);
		D3D12_ROOT_PARAMETER rootParameterCBPS;
		rootParameterCBPS.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameterCBPS.Descriptor.ShaderRegister = 0; 
		rootParameterCBPS.Descriptor.RegisterSpace = 0;
		rootParameterCBPS.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		parameters.push_back(rootParameterCBPS);

		D3D12_DESCRIPTOR_RANGE srvRange = {};
		srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange.NumDescriptors = 8; 
		srvRange.BaseShaderRegister = 0; 
		srvRange.RegisterSpace = 0;
		srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		D3D12_ROOT_PARAMETER rootParameterTex;
		rootParameterTex.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameterTex.DescriptorTable.NumDescriptorRanges = 1;
		rootParameterTex.DescriptorTable.pDescriptorRanges = &srvRange;
		rootParameterTex.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		parameters.push_back(rootParameterTex);

		D3D12_STATIC_SAMPLER_DESC staticSampler = {};
		staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler.MipLODBias = 0;
		staticSampler.MaxAnisotropy = 1;
		staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		staticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		staticSampler.MinLOD = 0.0f;
		staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
		staticSampler.ShaderRegister = 0;
		staticSampler.RegisterSpace = 0;
		staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_ROOT_SIGNATURE_DESC desc = {};
		desc.NumParameters = parameters.size();
		desc.pParameters = &parameters[0];
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		
		desc.NumStaticSamplers = 1;
		desc.pStaticSamplers = &staticSampler;
		
		ID3DBlob* serialized;
		ID3DBlob* error;
		D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &error);
		//MessageBoxA(NULL, (const char*)error->GetBufferPointer(), "RootSignature Error", 0);
		device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
		srvTableIndex = 1;
		serialized->Release();
	}
	void resetCommandList()
	{
		unsigned int frameIndex = swapchain->GetCurrentBackBufferIndex();
		graphicsCommandAllocator[frameIndex]->Reset();
		graphicsCommandList[frameIndex]->Reset(graphicsCommandAllocator[frameIndex], NULL);
	}
	void runCommandList()
	{
		getCommandList()->Close();
		ID3D12CommandList* lists[] = { getCommandList() };
		graphicsQueue->ExecuteCommandLists(1, lists);
	}
	void uploadResource(ID3D12Resource* dstResource, const void* data, unsigned int size, D3D12_RESOURCE_STATES targetState, D3D12_PLACED_SUBRESOURCE_FOOTPRINT *texFootprint = NULL)
	{
		unsigned int frameIndex = swapchain->GetCurrentBackBufferIndex();
		ID3D12Resource* uploadBuffer;
		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		D3D12_RESOURCE_DESC bufferDesc = {};
		bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		bufferDesc.Width = size;
		bufferDesc.Height = 1;
		bufferDesc.DepthOrArraySize = 1;
		bufferDesc.MipLevels = 1;
		bufferDesc.SampleDesc.Count = 1;
		bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&uploadBuffer));

		void* mappeddata = nullptr;
		uploadBuffer->Map(0, nullptr, &mappeddata);
		memcpy(mappeddata, data, size);
		uploadBuffer->Unmap(0, nullptr);

		resetCommandList();

		if (texFootprint != NULL)
		{
			D3D12_TEXTURE_COPY_LOCATION src = {};
			src.pResource = uploadBuffer;
			src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			src.PlacedFootprint = *texFootprint;
			D3D12_TEXTURE_COPY_LOCATION dst = {};
			dst.pResource = dstResource;
			dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dst.SubresourceIndex = 0;
			getCommandList()->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
		} else
		{
			getCommandList()->CopyBufferRegion(dstResource, 0, uploadBuffer, 0, size);
		}


		Barrier::add(dstResource, D3D12_RESOURCE_STATE_COPY_DEST, targetState, getCommandList());

		getCommandList()->Close();
		ID3D12CommandList* lists[] = { getCommandList() };
		graphicsQueue->ExecuteCommandLists(1, lists);

		flushGraphicsQueue();

		uploadBuffer->Release();
	}
	ID3D12GraphicsCommandList4* getCommandList()
	{
		unsigned int frameIndex = swapchain->GetCurrentBackBufferIndex();
		return graphicsCommandList[frameIndex];
	}
	void beginFrame()
	{
		unsigned int frameIndex = swapchain->GetCurrentBackBufferIndex();
		graphicsQueueFence[frameIndex].wait();
		D3D12_CPU_DESCRIPTOR_HANDLE renderTargetViewHandle = backbufferHeap->GetCPUDescriptorHandleForHeapStart();
		unsigned int renderTargetViewDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		renderTargetViewHandle.ptr += frameIndex * renderTargetViewDescriptorSize;
		resetCommandList();
		Barrier::add(backbuffers[frameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET, getCommandList());
		getCommandList()->OMSetRenderTargets(1, &renderTargetViewHandle, FALSE, &dsvHandle);
		float color[4];
		color[0] = 0;
		color[1] = 0;
		color[2] = 1.0;
		color[3] = 1.0;
		getCommandList()->ClearRenderTargetView(renderTargetViewHandle, color, 0, NULL);
		getCommandList()->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
	}
	
	void clearDepthBuffer()
	{
		getCommandList()->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
	}
void beginRenderPass()
	{
		unsigned int frameIndex = swapchain->GetCurrentBackBufferIndex();
		getCommandList()->RSSetViewports(1, &viewport);
		getCommandList()->RSSetScissorRects(1, &scissorRect);
		getCommandList()->SetGraphicsRootSignature(rootSignature);

		ID3D12DescriptorHeap* heaps[] = { srvHeap.heap };
		getCommandList()->SetDescriptorHeaps(1, &srvHeap.heap);
	}
	int frameIndex()
	{
		return swapchain->GetCurrentBackBufferIndex();
	}
	void finishFrame()
	{
		unsigned int frameIndex = swapchain->GetCurrentBackBufferIndex();
		Barrier::add(backbuffers[frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT, getCommandList());
		runCommandList();
		graphicsQueueFence[frameIndex].signal(graphicsQueue);
		swapchain->Present(1, 0);
	}
	void flushGraphicsQueue()
	{
		graphicsQueueFence[0].signal(graphicsQueue);
		graphicsQueueFence[0].wait();
	}
	~Core()
	{
		for (int i = 0; i < 2; i++)
		{
			graphicsQueueFence[i].signal(graphicsQueue);
			graphicsQueueFence[i].wait();
		}
		srvHeap.heap->Release();  
		rootSignature->Release();
		graphicsCommandList[0]->Release();
		graphicsCommandAllocator[0]->Release();
		graphicsCommandList[1]->Release();
		graphicsCommandAllocator[1]->Release();
		backbuffers[0]->Release();
		backbuffers[1]->Release();
		delete[] backbuffers;
		backbufferHeap->Release();
		dsv->Release();
		dsvHeap->Release();
		swapchain->Release();
		computeQueue->Release();
		copyQueue->Release();
		graphicsQueue->Release();
		device->Release();
	}
};

template<typename T>
T use()
{
	static T t;
	return t;
}
