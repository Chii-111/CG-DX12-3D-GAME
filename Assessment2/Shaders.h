#pragma once

#include <d3d12.h>
#include <d3dcompiler.h>
#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream> 
#include <Windows.h> 

#include "Core.h"

#pragma comment(lib, "dxguid.lib")

struct ConstantBufferVariable
{
	unsigned int offset;
	unsigned int size;
};

class ConstantBuffer
{
public:
	std::string name;
	std::map<std::string, ConstantBufferVariable> constantBufferData;
	ID3D12Resource* constantBuffer;
	unsigned char* buffer;
	unsigned int cbSizeInBytes;
	unsigned int numInstances;
	unsigned int offsetIndex;
	void init(Core* core, unsigned int sizeInBytes, unsigned int maxDrawCalls = 1024)
	{
		cbSizeInBytes = (sizeInBytes + 255) & ~255;
		unsigned int cbSizeInBytesAligned = cbSizeInBytes * maxDrawCalls;
		numInstances = maxDrawCalls;
		offsetIndex = 0;
		HRESULT hr;
		D3D12_HEAP_PROPERTIES heapprops;
		memset(&heapprops, 0, sizeof(D3D12_HEAP_PROPERTIES));
		heapprops.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapprops.CreationNodeMask = 1;
		heapprops.VisibleNodeMask = 1;
		D3D12_RESOURCE_DESC cbDesc;
		memset(&cbDesc, 0, sizeof(D3D12_RESOURCE_DESC));
		cbDesc.Width = cbSizeInBytesAligned;
		cbDesc.Height = 1;
		cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		cbDesc.DepthOrArraySize = 1;
		cbDesc.MipLevels = 1;
		cbDesc.SampleDesc.Count = 1;
		cbDesc.SampleDesc.Quality = 0;
		cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		hr = core->device->CreateCommittedResource(&heapprops, D3D12_HEAP_FLAG_NONE, &cbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, __uuidof(ID3D12Resource), (void**)&constantBuffer);
		D3D12_RANGE readRange = { 0, 0 };
		hr = constantBuffer->Map(0, &readRange, (void**)&buffer);
	}
	void update(std::string name, void* data) 
	{
		ConstantBufferVariable cbVariable = constantBufferData[name];
		unsigned int offset = offsetIndex * cbSizeInBytes;
		memcpy(&buffer[offset + cbVariable.offset], data, cbVariable.size);
	}
	D3D12_GPU_VIRTUAL_ADDRESS getGPUAddress() const
	{
		return (constantBuffer->GetGPUVirtualAddress() + (offsetIndex * cbSizeInBytes));
	}
	void next()
	{
		offsetIndex++;
		if (offsetIndex >= numInstances)
		{
			offsetIndex = 0;
		}
	}
	void free()
	{
		if (constantBuffer) {
			constantBuffer->Unmap(0, NULL);
			constantBuffer->Release();
			constantBuffer = nullptr;
		}
	}
};

class Shader
{
public:
	ID3DBlob* ps = nullptr;
	ID3DBlob* vs = nullptr;
	std::vector<ConstantBuffer> psConstantBuffers;
	std::vector<ConstantBuffer> vsConstantBuffers;
	std::map<std::string, int> textureBindPoints;
	int hasLayout;

	void initConstantBuffers(Core* core, ID3DBlob* shader, std::vector<ConstantBuffer>& buffers)
	{
		ID3D12ShaderReflection* reflection = nullptr;
		D3DReflect(shader->GetBufferPointer(), shader->GetBufferSize(), IID_PPV_ARGS(&reflection));
		if (!reflection) return;

		D3D12_SHADER_DESC desc;
		reflection->GetDesc(&desc);
		for (int i = 0; i < desc.ConstantBuffers; i++)
		{
			ConstantBuffer buffer;
			ID3D12ShaderReflectionConstantBuffer* constantBuffer = reflection->GetConstantBufferByIndex(i);
			D3D12_SHADER_BUFFER_DESC cbDesc;
			constantBuffer->GetDesc(&cbDesc);
			buffer.name = cbDesc.Name;
			unsigned int totalSize = 0;
			for (int j = 0; j < cbDesc.Variables; j++)
			{
				ID3D12ShaderReflectionVariable* var = constantBuffer->GetVariableByIndex(j);
				D3D12_SHADER_VARIABLE_DESC vDesc;
				var->GetDesc(&vDesc);
				ConstantBufferVariable bufferVariable;
				bufferVariable.offset = vDesc.StartOffset;
				bufferVariable.size = vDesc.Size;
				buffer.constantBufferData.insert({ vDesc.Name, bufferVariable });
				totalSize += bufferVariable.size;
			}
			buffer.init(core, totalSize);
			buffers.push_back(buffer);
		}
		for (int i = 0; i < desc.BoundResources; i++)
		{
			D3D12_SHADER_INPUT_BIND_DESC bindDesc;
			reflection->GetResourceBindingDesc(i, &bindDesc);
			if (bindDesc.Type == D3D_SIT_TEXTURE)
			{
				textureBindPoints.insert({ bindDesc.Name, bindDesc.BindPoint });
			}
		}
		reflection->Release();
	}

	void loadPS(Core* core, std::string hlsl)
	{
		if (hlsl.empty()) {
			MessageBoxA(NULL, "Pixel Shader content is empty! Check file path.", "Shader Error", MB_ICONERROR);
			exit(0);
		}
		ID3DBlob* status = nullptr;
		HRESULT hr = D3DCompile(hlsl.c_str(), hlsl.length(), NULL, NULL, NULL, "PS", "ps_5_0", 0, 0, &ps, &status);
		if (FAILED(hr))
		{
			char* errorMsg = (char*)status->GetBufferPointer();
			MessageBoxA(NULL, errorMsg, "Pixel Shader Compile Error", MB_ICONERROR);
			if (status) status->Release();
			exit(0);
		}
		if (status) status->Release();
		initConstantBuffers(core, ps, psConstantBuffers);
	}

	void loadVS(Core* core, std::string hlsl)
	{
		if (hlsl.empty()) {
			MessageBoxA(NULL, "Vertex Shader content is empty! Check file path.", "Shader Error", MB_ICONERROR);
			exit(0);
		}
		ID3DBlob* status = nullptr;
		HRESULT hr = D3DCompile(hlsl.c_str(), hlsl.length(), NULL, NULL, NULL, "VS", "vs_5_0", 0, 0, &vs, &status);
		if (FAILED(hr))
		{
			char* errorMsg = (char*)status->GetBufferPointer();
			MessageBoxA(NULL, errorMsg, "Vertex Shader Compile Error", MB_ICONERROR);
			if (status) status->Release();
			exit(0);
		}
		if (status) status->Release();
		initConstantBuffers(core, vs, vsConstantBuffers);
	}

	void updateConstant(std::string constantBufferName, std::string variableName, void* data, std::vector<ConstantBuffer>& buffers)
	{
		for (int i = 0; i < buffers.size(); i++)
		{
			if (buffers[i].name == constantBufferName)
			{
				buffers[i].update(variableName, data);
				return;
			}
		}
	}
	void updateConstantVS(std::string constantBufferName, std::string variableName, void* data)
	{
		updateConstant(constantBufferName, variableName, data, vsConstantBuffers);
	}
	void updateConstantPS(std::string constantBufferName, std::string variableName, void* data)
	{
		updateConstant(constantBufferName, variableName, data, psConstantBuffers);
	}
	void updateTexturePS(Core* core, std::string name, int heapOffset) {
		if (textureBindPoints.find(name) == textureBindPoints.end()) return; 
		UINT bindPoint = textureBindPoints[name];
		D3D12_GPU_DESCRIPTOR_HANDLE handle = core->srvHeap.gpuHandle;
		handle.ptr = handle.ptr + (UINT64)(heapOffset - bindPoint) * (UINT64)core->srvHeap.incrementSize;
		core->getCommandList()->SetGraphicsRootDescriptorTable(2, handle);
	}
	void apply(Core* core)
	{
		for (int i = 0; i < vsConstantBuffers.size(); i++)
		{
			core->getCommandList()->SetGraphicsRootConstantBufferView(0, vsConstantBuffers[i].getGPUAddress());
			vsConstantBuffers[i].next();
		}
		for (int i = 0; i < psConstantBuffers.size(); i++)
		{
			core->getCommandList()->SetGraphicsRootConstantBufferView(1, psConstantBuffers[i].getGPUAddress());
			psConstantBuffers[i].next();
		}
	}
	void free()
	{
		if (ps) { ps->Release(); ps = nullptr; }
		if (vs) { vs->Release(); vs = nullptr; }

		for (auto& cb : psConstantBuffers) cb.free();
		psConstantBuffers.clear();
		for (auto& cb : vsConstantBuffers) cb.free();
		vsConstantBuffers.clear();
	}
};

class Shaders
{
public:
	std::map<std::string, Shader> shaders;

	std::string readFile(std::string filename)
	{
		std::ifstream file(filename);
		if (!file.is_open()) {
			std::string msg = "Could not open shader file: " + filename + "\nCheck Working Directory!";
			MessageBoxA(NULL, msg.c_str(), "File Error", MB_ICONERROR);
			return "";
		}
		std::stringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
	}

	void load(Core* core, std::string shadername, std::string vsfilename, std::string psfilename)
	{
		if (shaders.find(shadername) != shaders.end()) return;

		Shader shader;
		
		std::string psSrc = readFile(psfilename);
		std::string vsSrc = readFile(vsfilename);

		if (psSrc.empty() || vsSrc.empty()) {
			exit(0); 
		}

		shader.loadPS(core, psSrc);
		shader.loadVS(core, vsSrc);
		shaders.insert({ shadername, shader });
	}

	void updateConstantVS(std::string name, std::string constantBufferName, std::string variableName, void* data)
	{
		shaders[name].updateConstantVS(constantBufferName, variableName, data);
	}
	void updateConstantPS(std::string name, std::string constantBufferName, std::string variableName, void* data)
	{
		shaders[name].updateConstantPS(constantBufferName, variableName, data);
	}
	void updateTexturePS(Core* core, std::string name, std::string textureName, int heapOffset)
	{
		shaders[name].updateTexturePS(core, textureName, heapOffset);
	}
	Shader* find(std::string name)
	{
		return &shaders[name];
	}
	void apply(Core* core, std::string name)
	{
		shaders[name].apply(core);
	}
	~Shaders()
	{
		for (auto it = shaders.begin(); it != shaders.end(); )
		{
			it->second.free();
			shaders.erase(it++);
		}
	}
};