#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include "d3dx12.h"
#include <string>

// this will only call release if an object exists (prevents exceptions calling release on non existant objects)
#define SAFE_RELEASE(p) { if ( (p) ) { (p)->Release(); (p) = 0; } }

// Handle to the window
HWND hwnd = NULL;
// name of the window
LPCTSTR WindowName = L"DX12TestRenderer";

// title of the window
LPCTSTR WindowTitle = L"DX12TestRenderWindow";

// width and height of the window 
int Width = 1280;
int Height = 720;

// fullscreen setting
bool FullScreen = false;
bool Running = true;

//direct3d stuff
/*
	Render Targets:     Number of frame buffers
	Command Allocators: Number of frame buffers * number of threads
	Fences:             (Number of frame buffers * ??) Number of threads
	Fence Values:       Number of threads
	Fence Events:       Number of threads
	Command Lists:      Number of threads
*/
const int frameBufferCount = 3; // 2 for double buffering, 3 for tripple buffering

ID3D12Device* device; //specifies the device we use to render

IDXGISwapChain3* swapChain; //used to switch between render targets

ID3D12CommandQueue* commandQueue; //container for the command lists

ID3D12DescriptorHeap* rtvDescriptorHeap; //holds resources like render targets

ID3D12Resource* renderTargets[frameBufferCount]; //render target per frame buffer count

ID3D12CommandAllocator* commandAllocator[frameBufferCount]; // command allocator per thread per frame buffer

ID3D12GraphicsCommandList* commandList; //command list which commands can be recorded into and then be executed to render the frame

ID3D12Fence* fence[frameBufferCount]; //an object that is locked while the command list is eing executed by the gpu. 

HANDLE fenceEvent; //a handle to an event for when the fence is unlocked by the gpu

UINT64 fenceValue[frameBufferCount]; //this value is incremented each frame. each fence will have its one value

int frameIndex; //current rtv

int rtvDescriptorSize; //size of the rtv descriptor on the device (all front and back buffers will be the same size)

// drawing objects stuff //
ID3D12PipelineState* pipelineStateObject; //pso containing a pipeline state (in this case the vertex data for 1 object)

ID3D12RootSignature* rootSignature; //root signature defines data shaders will access

D3D12_VIEWPORT viewport; //area that the rasterizer will be streched to.

D3D12_RECT scissorRect; //the area of the window that can be drawn in. pixels outside the area will not be drawn

ID3D12Resource* vertexBuffer; //a default buffer in gpu memory that we will load the vertex data into

D3D12_VERTEX_BUFFER_VIEW vertexBufferView; //a structure containing a pointer to the vertex data in gpu memory (to be used by the driver), 
										   //the total size of the buffer, and the size of each element


/// functions

//create the window
bool InitializeWindow(HINSTANCE hInstance, int ShowWnd, bool fullscreen);

//main application loop
void MainLoop();

//Callback function for windows messages (e.g. window close)
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

//initializes direct3d 12
bool InitD3D(); 

//update the game logic
void Update();

//update teh direct3d pipeline (update the command list)
void UpdatePipeline();

//execute the command list
void Render();

//release com objects and clean up memory
void Cleanup();

//wait until gpu is finished with the command list
void WaitForPreviousFrame();


template <class C>
std::size_t countof(C const & c)
{
	return c.size();
}