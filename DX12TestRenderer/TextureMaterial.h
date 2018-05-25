#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <string>
#include "d3dx12.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <wincodec.h>
#include <iostream>
#include "Debug.h"
#include "Mesh.h"
class TextureMaterial
{
public:
	TextureMaterial(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, LPCWSTR pTexture);
	void Render(Mesh* pMesh, D3D12_GPU_VIRTUAL_ADDRESS pGPUAddress);
	~TextureMaterial();
protected:
	ID3D12Device * device;
	ID3D12GraphicsCommandList* commandList;

	// drawing objects stuff //
	ID3D12PipelineState* pipelineStateObject; //pso containing a pipeline state (in this case the vertex data for 1 object)

	ID3D12RootSignature* rootSignature; //root signature defines data shaders will access

	ID3D12DescriptorHeap* mainDescriptorHeap;

	ID3D12Resource* textureBufferUploadHeap;

	BYTE* imageData;
	
	ID3D12Resource* textureBuffer; //the resource heap containing our texture


	//load and decode image from file
	int LoadImageDataFromFile(BYTE** imageData, D3D12_RESOURCE_DESC& resourceDescription, LPCWSTR filename, int &bytesPerRow);

	//get DXGI format from the WIC format GUID
	DXGI_FORMAT GetDXGIFormatFromWICFormat(WICPixelFormatGUID& wicFormatGUID);

	//converted format for dxgi unknown format
	WICPixelFormatGUID GetConvertToWICFormat(WICPixelFormatGUID& wicFormatGUID);

	//get the bit depth
	int GetDXGIFormatBitsPerPixel(DXGI_FORMAT& dxgiFormat);

	static ID3D12Resource* CreateTextureDefaultBuffer(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmdList,
		const void* initData,
		int bytesPerRow,
		D3D12_RESOURCE_DESC& textureDesc,
		ID3D12Resource*& uploadBuffer
	);

};

