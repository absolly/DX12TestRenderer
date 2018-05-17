#include "stdafx.h"

struct Vertex
{
	Vertex(float x, float y, float z, float u, float v) : pos(x,y,z), texCoord(u, v){}
	XMFLOAT3 pos;
	XMFLOAT2 texCoord;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR, int nShowCmd) {

	//create the window
	if (!InitializeWindow(hInstance, nShowCmd, FullScreen)) {
		MessageBox(0, L"Window initialization - Failed", L"Error", MB_OK);
		return 0;
	}

	//initialize direct3d
	if (!InitD3D()) {
		MessageBox(0, L"Failed to initialize direct3d 12", L"Error", MB_OK);
		Cleanup();
		return 1;
	}

	//start the main loop
	mainloop();

	//we want to wait for the gpu to finish executing the command list before we start releasing everything
	WaitForPreviousFrame();

	//close the fence event
	CloseHandle(fenceEvent);

	// clean up
	Cleanup();

	return 0;
}

bool InitializeWindow(HINSTANCE hInstance, int ShowWnd, bool fullscreen) {
	if (fullscreen) {
		HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFO mi = { sizeof(mi) };
		GetMonitorInfo(hmon, &mi);

		Width = mi.rcMonitor.right - mi.rcMonitor.left;
		Height = mi.rcMonitor.bottom - mi.rcMonitor.top;
	}

	WNDCLASSEX windowClass;

	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WndProc;
	windowClass.cbClsExtra = NULL;
	windowClass.cbWndExtra = NULL;
	windowClass.hInstance = hInstance;
	windowClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW);
	windowClass.lpszMenuName = NULL;
	windowClass.lpszClassName = WindowName;
	windowClass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassEx(&windowClass)) {
		MessageBox(NULL, L"Failed to register window class", L"Error", MB_OK | MB_ICONERROR);
		return false;
	}

	hwnd = CreateWindowEx(
		NULL,
		WindowName,
		WindowTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		Width, Height,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if (!hwnd) {
		MessageBox(NULL, L"Error creating window", L"Error", MB_OK | MB_ICONERROR);
		return false;
	}

	if (fullscreen) {
		SetWindowLong(hwnd, GWL_STYLE, 0);
	}

	ShowWindow(hwnd, ShowWnd);
	UpdateWindow(hwnd);

	return true;
}

void mainloop() {
	MSG msg;
	ZeroMemory(&msg, sizeof(MSG));

	while (Running) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				break;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			Update();
			Render();
		}
	}
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg)
	{
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE) {
			if (MessageBox(0, L"Are you sure you want to exit?", L"Really?", MB_YESNO | MB_ICONQUESTION) == IDYES)
				DestroyWindow(hwnd);
		}
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

}

bool InitD3D() {
	HRESULT hr;

	// create the device //

	IDXGIFactory4* dxgiFactory;
	hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
	if (FAILED(hr))
		return false;

	IDXGIAdapter1* adapter; //adapters are the graphics card (including embedded graphcis on the motherboard)

	int adapterIndex = 0; //start looking at the first device index

	bool adapterFound = false; //we will keep looking until this is true or there are no more adapters

	while (dxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND) {
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		//skip software devices
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			adapterIndex++;
			continue;
		}

		//check if the device is directx 12 compatible (feature level 11 or higher)
		hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr);
		if (SUCCEEDED(hr)) {
			adapterFound = true;
			break;
		}
		adapterIndex++;

	}

	if (!adapterFound)
		return false;

	//create the device
	hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
	if (FAILED(hr))
		return false;

	// create the command queue //
	D3D12_COMMAND_QUEUE_DESC cqDesc = {}; //using default command queue values;
	cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; //direct means the gpu can directly execute this command queue

	hr = device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&commandQueue)); // create the command queue
	if (FAILED(hr))
		return false;

	// create the swapchain (double/tripple buffering) //

	// display mode settings
	DXGI_MODE_DESC backBufferDesc = {};
	backBufferDesc.Width = Width;
	backBufferDesc.Height = Height;
	backBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; //color format of the buffer (rgba 32 bit)

	// multi-sampling settings (not using it currently)
	DXGI_SAMPLE_DESC sampleDesc = {};
	sampleDesc.Count = 1; // 1 sample count is disables multi-sampling

	// swap chain settings
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = frameBufferCount;
	swapChainDesc.BufferDesc = backBufferDesc;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; //tells dx pipeline that we will render to this swap chain
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; //dxgi will discard the buffer data after we call present
	swapChainDesc.OutputWindow = hwnd;
	swapChainDesc.SampleDesc = sampleDesc;
	swapChainDesc.Windowed = !FullScreen;

	IDXGISwapChain* tempSwapChain;

	dxgiFactory->CreateSwapChain(
		commandQueue, //the queue will be flushed once the swap chain is created
		&swapChainDesc, //pass it the swapchain we created above
		&tempSwapChain //store the swapchain in a temp IDXGISwapChain interface (cast to IDXGISwapChain3 later)
	);

	swapChain = static_cast<IDXGISwapChain3*>(tempSwapChain);

	frameIndex = swapChain->GetCurrentBackBufferIndex();

	// create the back buffers (rtv's) discriptor heap //

	// describe a rtv descriptor heap and create
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = frameBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; //this is a rtv heap

	//this heap will not be directly visible to shaders as it will store teh output from the pipeline
	// otherwise we would set teh heap's flag to D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap));
	if (FAILED(hr))
		return false;

	//get the size of teh descriptor in this heap (this is a rtv heap so only rtv descriptors should be stored in it)
	//descriptor sizes may vary from device to device. which is where there is no set size and we must ask the device to give us the size
	// we will use the given size to increment a discriptor handle offset
	rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	//get a handle to the first descriptor in the descriptor heap. a handle is basically a pointer,
	//but we cannot literally use it like a c++ pointer. it is for the driver to use.
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	//create a rtv for each buffer (3 for tripple buffering in this case)
	for (int i = 0; i < frameBufferCount; i++)
	{
		//first get the n'th buffer in the swap chain and store it in the n'th
		//position of out ID3D12Resource array
		hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
		if (FAILED(hr))
			return false;

		//then we "create" a rtv which binds the swap chain buffer (ID3D12Resource[n])
		device->CreateRenderTargetView(renderTargets[i], nullptr, rtvHandle);

		//increment the rtv handle by the rtv descriptor size
		rtvHandle.Offset(1, rtvDescriptorSize);
	}

	// create the command allocators //

	for (int i = 0; i < frameBufferCount; i++)
	{
		hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator[i]));
		if (FAILED(hr))
			return false;
	}

	// create the command list with the first allocator. we only need one since we only use one thread and can reset the cpu side list right after executing
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator[frameIndex], NULL, IID_PPV_ARGS(&commandList));
	if (FAILED(hr))
		return false;

	// create a fense and a fence event //

	//create fences
	for (int i = 0; i < frameBufferCount; i++)
	{
		hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence[i]));
		if (FAILED(hr))
			return false;

		fenceValue[i] = 0; // set initial fence value to 0
	}

	//create a handle to the fence event
	fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (fenceEvent == nullptr)
		return false;

	//create root signature
	
	D3D12_ROOT_DESCRIPTOR rootCBVDescriptor;
	rootCBVDescriptor.RegisterSpace = 0;
	rootCBVDescriptor.ShaderRegister = 0;

	//create the descriptor range and fill it out
	D3D12_DESCRIPTOR_RANGE descriptorTableRanges[1];
	descriptorTableRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; //this is the range of shader resource views
	descriptorTableRanges[0].NumDescriptors = 1; //we only have 1 texture right now
	descriptorTableRanges[0].BaseShaderRegister = 0; //start index of the shader registers in the range
	descriptorTableRanges[0].RegisterSpace = 0; //space can usually be 0 according to msdn. don't know why
	descriptorTableRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; //appends the range to the end of the root signature descriptor tables

	//create the descriptor table
	D3D12_ROOT_DESCRIPTOR_TABLE descriptorTable;
	descriptorTable.NumDescriptorRanges = _countof(descriptorTableRanges); //only one right now
	descriptorTable.pDescriptorRanges = &descriptorTableRanges[0]; //pointer to the start of the ranges array

	//create a root parameter
	D3D12_ROOT_PARAMETER rootParameters[2];
	//its a good idea to sort the root parameters by frequency of change.
	//the constant buffer will change multiple times per frame but the descriptor table won't change in this case

	//constant buffer view root descriptor
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; //this is a constant buffer view
	rootParameters[0].Descriptor = rootCBVDescriptor;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; //only the vertex shader will be able to access the parameter

	//descriptor table
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; 
	rootParameters[1].DescriptorTable = descriptorTable;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; //only visible to pixel since this should contain the texture.

	//create a static sampler
	D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	sampler.MipLODBias = 0;
	sampler.MaxAnisotropy = 0;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS; //used for more advanced effects like shadow mapping. leave on always for now.
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;



	//fill out the root signature
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init(
		_countof(rootParameters), //we have 1 root parameter for now
		rootParameters, //pointer to the start of the root parameters array
		1, //we have one static sampler
		&sampler, //pointer to our static sampler (array)
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | 
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |       // we can deny shader stages here for better performance
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
	);

	ID3DBlob* errorBuffer; //a buffer holding the error data if any
	ID3DBlob* signature;
	hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errorBuffer);
	if (FAILED(hr))
		return false;

	hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	if (FAILED(hr))
		return false;

	//Compile the vertex and pixel shaders

	//when debuging we can compile the shaders at runtime.
	//for release versions it is recommended to compile the hlsl shaders using fxc.exe
	//this creates .cso files that can be loaded in at runtime to get the shader bytecode
	//this is of course faster then compiling them at runtime

	//compile vertex shader
	ID3DBlob* vertexShader; //d3d blob for holding shader bytecode
							//shader file,		  defines  includes, entry,	sm		  compile flags,							efect flags, shader blob, error blob
	hr = D3DCompileFromFile(L"VertexShader.hlsl", nullptr, nullptr, "main", "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &vertexShader, &errorBuffer);
	if (FAILED(hr)) {
		OutputDebugStringA((char*)errorBuffer->GetBufferPointer());
		return false;
	}

	//fill out a shader bytecode structure, which is bascially a pointer
	//to the shader bytecode and syze of shader bytecode
	D3D12_SHADER_BYTECODE vertexShaderBytecode = {};
	vertexShaderBytecode.BytecodeLength = vertexShader->GetBufferSize();
	vertexShaderBytecode.pShaderBytecode = vertexShader->GetBufferPointer();

	// compile pixel shader
	ID3DBlob* pixelShader;
	//shader file,		  defines  includes, entry,	sm		  compile flags,							efect flags, shader blob, error blob
	hr = D3DCompileFromFile(L"PixelShader.hlsl", nullptr, nullptr, "main", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &pixelShader, &errorBuffer);
	if (FAILED(hr))
	{
		OutputDebugStringA((char*)errorBuffer->GetBufferPointer());
		return false;
	}

	// fill out shader bytecode structure for pixel shader
	D3D12_SHADER_BYTECODE pixelShaderBytecode = {};
	pixelShaderBytecode.BytecodeLength = pixelShader->GetBufferSize();
	pixelShaderBytecode.pShaderBytecode = pixelShader->GetBufferPointer();

	//create input layout

	//the input layout is used by the ia so it knows
	//how to read teh vertex data bound to it.

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	//fill out an input layout description struct
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};

	//we can get teh number of elements in an array by "sizeof(array)/sizeof(arrayElementType)"
	inputLayoutDesc.NumElements = sizeof(inputLayout) / sizeof(D3D12_INPUT_ELEMENT_DESC);
	inputLayoutDesc.pInputElementDescs = inputLayout;

	//create a pipeline state object
	//in real applications you will have many pso's. for each different shader
	//or different combinations of shaders, different blend states or different rasterizer states,
	//different topology types (point, line, triangle, patch), or a different number of render targets.

	// the vertex shader is the only required shader for a pso

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {}; //pso description struct
	psoDesc.InputLayout = inputLayoutDesc;
	psoDesc.pRootSignature = rootSignature;
	psoDesc.VS = vertexShaderBytecode;
	psoDesc.PS = pixelShaderBytecode;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; //format of the rtv
	psoDesc.SampleDesc = sampleDesc;
	psoDesc.SampleMask = 0xffffffff; //sample mask has to do with multi - sampling. 0xffffffff means point sampling
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); //default rasterizer state
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); //default blend state
	psoDesc.NumRenderTargets = 1;
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); //default values for depth and stencil settings are alright for now.
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	// create the pso
	hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStateObject));
	if (FAILED(hr))
		return false;

	//


	// Create the vertex buffer // ----------------------------------------------------------------------

	//triangle vertex data

	//triangle
	//Vertex vList[] = {
	//	{ 0.0f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
	//	{ 0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
	//	{ -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
	//};

	//quad with duplicate points
	//Vertex vList[] = {
	//	{ -0.5f, -0.5f, 0.5f,	1.0f, 0.0f, 0.0f, 1.0f }, //0
	//	{ -0.5f, 0.5f, 0.5f,	0.0f, 1.0f, 0.0f, 1.0f }, //1
	//	{ 0.5f, 0.5f, 0.5f,		0.0f, 0.0f, 1.0f, 1.0f }, //2
	//	{ -0.5f, -0.5f, 0.5f,	1.0f, 0.0f, 0.0f, 1.0f }, //0
	//	{ 0.5f, 0.5f, 0.5f,		0.0f, 0.0f, 1.0f, 1.0f }, //2
	//	{ 0.5f, -0.5f, 0.5f,	1.0f, 0.0f, 1.0f, 1.0f }, //3
	//};

	//quad with indexed vertices
	//Vertex vList[] = {
	//	{ -0.5f,  0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
	//	{ 0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
	//	{ -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
	//	{ 0.5f,  0.5f, 0.5f, 1.0f, 0.0f, 1.0f, 1.0f }
	//};

	//Vertex vList[] = {
	//	// first quad (closer to camera, blue)
	//	{ -0.5f,  0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
	//	{ 0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
	//	{ -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
	//	{ 0.5f,  0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },

	//	// second quad (further from camera, green)
	//	{ -0.75f,  0.75f,  0.7f, 0.0f, 1.0f, 0.0f, 1.0f },
	//	{ 0.0f,  0.0f, 0.7f, 0.0f, 1.0f, 0.0f, 1.0f },
	//	{ -0.75f,  0.0f, 0.7f, 0.0f, 1.0f, 0.0f, 1.0f },
	//	{ 0.0f,  0.75f,  0.7f, 0.0f, 1.0f, 0.0f, 1.0f }
	//};
	// Create vertex buffer

	// a cube vertex colors
	//Vertex vList[] = {
	//	// front face
	//	{ -0.5f,  0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
	//	{ 0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f },
	//	{ -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
	//	{ 0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f },

	//	// right side face
	//	{ 0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
	//	{ 0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 1.0f, 1.0f },
	//	{ 0.5f, -0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
	//	{ 0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f },

	//	// left side face
	//	{ -0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
	//	{ -0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f },
	//	{ -0.5f, -0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
	//	{ -0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f },

	//	// back face
	//	{ 0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
	//	{ -0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 1.0f, 1.0f },
	//	{ 0.5f, -0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
	//	{ -0.5f,  0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f },

	//	// top face
	//	{ -0.5f,  0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
	//	{ 0.5f,  0.5f,  0.5f, 1.0f, 0.0f, 1.0f, 1.0f },
	//	{ 0.5f,  0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
	//	{ -0.5f,  0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f },

	//	// bottom face
	//	{ 0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 0.0f, 1.0f },
	//	{ -0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f },
	//	{ 0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
	//	{ -0.5f, -0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f },
	//};

	//a cube with UVs
	Vertex vList[] = {
		// front face
		{ -0.5f,  0.5f, -0.5f, 0.0f, 0.0f },
		{ 0.5f, -0.5f, -0.5f, 1.0f, 1.0f },
		{ -0.5f, -0.5f, -0.5f, 0.0f, 1.0f },
		{ 0.5f,  0.5f, -0.5f, 1.0f, 0.0f },

		// right side face
		{ 0.5f, -0.5f, -0.5f, 0.0f, 1.0f },
		{ 0.5f,  0.5f,  0.5f, 1.0f, 0.0f },
		{ 0.5f, -0.5f,  0.5f, 1.0f, 1.0f },
		{ 0.5f,  0.5f, -0.5f, 0.0f, 0.0f },

		// left side face
		{ -0.5f,  0.5f,  0.5f, 0.0f, 0.0f },
		{ -0.5f, -0.5f, -0.5f, 1.0f, 1.0f },
		{ -0.5f, -0.5f,  0.5f, 0.0f, 1.0f },
		{ -0.5f,  0.5f, -0.5f, 1.0f, 0.0f },

		// back face
		{ 0.5f,  0.5f,  0.5f, 0.0f, 0.0f },
		{ -0.5f, -0.5f,  0.5f, 1.0f, 1.0f },
		{ 0.5f, -0.5f,  0.5f, 0.0f, 1.0f },
		{ -0.5f,  0.5f,  0.5f, 1.0f, 0.0f },

		// top face
		{ -0.5f,  0.5f, -0.5f, 0.0f, 1.0f },
		{ 0.5f,  0.5f,  0.5f, 1.0f, 0.0f },
		{ 0.5f,  0.5f, -0.5f, 1.0f, 1.0f },
		{ -0.5f,  0.5f,  0.5f, 0.0f, 0.0f },

		// bottom face
		{ 0.5f, -0.5f,  0.5f, 0.0f, 0.0f },
		{ -0.5f, -0.5f, -0.5f, 1.0f, 1.0f },
		{ 0.5f, -0.5f, -0.5f, 0.0f, 1.0f },
		{ -0.5f, -0.5f,  0.5f, 1.0f, 0.0f },
	};

	int vBufferSize = sizeof(vList);

	// create default heap
	// default heap is memory on the GPU. it can only be accessed directly by the gpu
	// to get data into this heap, we will have to upload the data using an upload heap
	hr = device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vBufferSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&vertexBuffer)
	);
	if (FAILED(hr)) {
		Running = false;
		return false;
	}

	//set a name for the resource heap so it is identifiable when debuging
	vertexBuffer->SetName(L"Vertex Buffer Resource Heap");

	//create upload heap
	//upload heaps are used to upload data from cpu memory to gpu memory. the cpu can write to it and the gpu can read from it
	//we will use this to copy the data from the cpu memory to the gpu default heap we created above.
	ID3D12Resource* vBufferUploadHeap;
	hr =device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vBufferUploadHeap)
	);
	if (FAILED(hr)) {
		Running = false;
		return false;
	}

	vBufferUploadHeap->SetName(L"Vertex Buffer Upload Resource Heap");

	//store vertex buffer in upload heap
	D3D12_SUBRESOURCE_DATA vertexData = {};
	vertexData.pData = reinterpret_cast<BYTE*>(vList);
	vertexData.RowPitch = vBufferSize;		//size of all out triangle vertex data
	vertexData.SlicePitch = vBufferSize;	//

	//add the command to the commandlist to copy the date from the upload to the default heap
	UpdateSubresources(commandList, vertexBuffer, vBufferUploadHeap, 0, 0, 1, &vertexData);

	//transition the vertex buffer data from copy destination state to vertex buffer state
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	//create index buffer

	// a quad (2 triangles)
	//DWORD iList[] = {
	//	0, 1, 2, //first triangle
	//	0, 3, 1  //second triangle
	//};

	
	// a cube (12 triangles)
	DWORD iList[] = {
		// front face
		0, 1, 2, // first triangle
		0, 3, 1, // second triangle

		// left face
		4, 5, 6, // first triangle
		4, 7, 5, // second triangle

		// right face
		8, 9, 10, // first triangle
		8, 11, 9, // second triangle

		// back face
		12, 13, 14, // first triangle
		12, 15, 13, // second triangle

		// top face
		16, 17, 18, // first triangle
		16, 19, 17, // second triangle

		// bottom face
		20, 21, 22, // first triangle
		20, 23, 21, // second triangle
	};

	int iBufferSize = sizeof(iList);

	numCubeIndices = sizeof(iList) / sizeof(DWORD); //the number of indeces we want to draw (size of the (iList)/(size of one float3) i think)

	//create default heap to hold index buffer
	hr = device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(iBufferSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&indexBuffer)
	);
	if (FAILED(hr)) {
		Running = false;
		return false;
	}

	//resource heap name for easier debugging
	indexBuffer->SetName(L"Index Buffer Resource Heap");

	//create upload heap to upload index buffer
	ID3D12Resource* iBufferUploadHeap;
	hr = device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(iBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&iBufferUploadHeap)
	);
	if (FAILED(hr)) {
		Running = false;
		return false;
	}

	iBufferUploadHeap->SetName(L"Index Buffer Upload Resource Heap");

	//store index buffer into upload heap
	D3D12_SUBRESOURCE_DATA indexData = {};
	indexData.pData = reinterpret_cast<BYTE*>(iList);
	indexData.RowPitch = iBufferSize;
	indexData.SlicePitch = iBufferSize;

	//create command to copy data from upload heap to default heap
	UpdateSubresources(commandList, indexBuffer, iBufferUploadHeap, 0, 0, 1, &indexData);

	//transition index buffer data from copy to index buffer state
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));

	//create a depth stencil descriptor heap so we can get a pointer to the depth stencil buffer
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsDescriptorHeap));
	if (FAILED(hr))
		Running = false;

	D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
	depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

	D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
	depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
	depthOptimizedClearValue.DepthStencil.Stencil = 0;

	hr = device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, Width, Height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthOptimizedClearValue,
		IID_PPV_ARGS(&depthStencilBuffer)
	);
	if (FAILED(hr)) {
		Running = false;
		return false;
	}

	dsDescriptorHeap->SetName(L"Depth/Stencil Resource Heap");

	device->CreateDepthStencilView(depthStencilBuffer, &depthStencilDesc, dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	//create a constant buffer resource heap
	//unlike the other upload buffers this one is not temporary
	//since the data in this buffer will likely be updated every frame there is no point in copying the data to a default heap
	
	//create a resource heap, descriptor heap, and pointer to cbv for every framebuffer
	for (int i = 0; i < frameBufferCount; i++)
	{
		hr = device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(1024*64), //must be a multiple of 64kb thus 64 bytes * 1024 (4mb multiple for multi-sampled textures)
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&constantBufferUploadHeaps[i])
		);
		if (FAILED(hr)) {
			Running = false;
			return false;
		}

		constantBufferUploadHeaps[i]->SetName(L"Constant Buffer Upload Resource Heap");
		
		//this shouldn't need te be done inside the loop
		ZeroMemory(&cbPerObject, sizeof(cbPerObject));

		CD3DX12_RANGE readRange(0, 0); //read range is less then 0, indicates that we will not be reading this resource from the cpu
		
		//map the resource heap to get a gpu virtual address to the beginning of the heap
		hr = constantBufferUploadHeaps[i]->Map(0, &readRange, reinterpret_cast<void**>(&cbvGPUAddress[i]));

		//because of the constant read alignment requirements, constant buffer views must be 256 bit aligned. since our buffers are smaller than 256 bits
		//we just need to add the spacing between the two buffers, so the second buffer starts 256 bits from the beginning of the heap
		memcpy(cbvGPUAddress[i], &cbPerObject, sizeof(cbPerObject)); //cube1's constant buffer data
		memcpy(cbvGPUAddress[i] + ConstantBufferPerObjectAlignedSize, &cbPerObject, sizeof(cbPerObject)); //cube2's constant buffer data
	}

	//load the image, create a texture resource and descriptor heap

	//create the descriptor heap that will store our shader resource view
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = 1;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mainDescriptorHeap));
	if (FAILED(hr))
		Running = false;

	//load the image from file
	D3D12_RESOURCE_DESC textureDesc;
	int imageBytesPerRow;
	int imageSize = LoadImageDataFromFile(&imageData, textureDesc, L"absolly.png", imageBytesPerRow);

	//make sure we have data
	if (imageSize <= 0) {
		Running = false;
		return false;
	}

	//create a default heap where the upload heap will copy the textures into
	hr = device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr, //clear value used for render targets and depth/stencil buffers
		IID_PPV_ARGS(&textureBuffer)
	);
	if (FAILED(hr))
	{
		Running = false;
		return false;
	}
	textureBuffer->SetName(L"Texture Buffer Resource Heap");

	UINT64 textureUploadBufferSize;
	//this function gets the size an upload buffer needs to be to upload a texture to the gpu.
	//each row must be 256 byte aligned except the last row, which can just be the size in bytes of the row
	//the function below does the following calculation: 
	//int textureHeapSize = ((((width * numBytesPerPixel) + 255) & ~255) * (height - 1)) + (width * numBytesPerPixel);
	device->GetCopyableFootprints(&textureDesc, 0, 1, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);

	//now create an upload heap to upload the texture to the gpu
	hr = device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(textureUploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&textureBufferUploadHeap)
	);
	if (FAILED(hr))
	{
		Running = false;
		return false;
	}
	textureBufferUploadHeap->SetName(L"Texture Buffer Upload Resource Heap");

	//store texture data in texture heap
	D3D12_SUBRESOURCE_DATA textureData = {};
	textureData.pData = &imageData[0]; //pointer to the image data
	textureData.RowPitch = imageBytesPerRow;
	textureData.SlicePitch = imageBytesPerRow * textureDesc.Height;

	//now we copy the upload buffer contents to the default heap
	UpdateSubresources(commandList, textureBuffer, textureBufferUploadHeap, 0, 0, 1, &textureData);

	//transition the texture default heap to a pixel shader resource
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(textureBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

	//now we create a shader resource view descriptor (points to the texture and describes it)
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	device->CreateShaderResourceView(textureBuffer, &srvDesc, mainDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	//new we execute the command list and upload the initial assets (triangle data)
	commandList->Close();
	ID3D12CommandList* ppCommandLists[] = { commandList };
	commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	//increment the fence value now, otherwise the buffer might not be uploaded by the time we start drawing
	fenceValue[frameIndex]++;
	hr = commandQueue->Signal(fence[frameIndex], fenceValue[frameIndex]);
	if (FAILED(hr)) {
		Running = false;
		return false;
	}

	//we are done with the image data. it's uploaded to the gpu now. we can free up the (ram) memory
	delete imageData;

	//create a vertex buffer view for the triangle. we get the gpu memory address to the vertex pointer using the GetGPUVirtualAddress() method
	vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vertexBufferView.StrideInBytes = sizeof(Vertex);
	vertexBufferView.SizeInBytes = vBufferSize;

	//create a index buffer view for the triangle. gets the gpu memory address to the pointer.
	indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	indexBufferView.SizeInBytes = iBufferSize;

	//fill out the viewport
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = Width;
	viewport.Height = Height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	//fill out the scissor rect
	scissorRect.left = 0;
	scissorRect.right = Width;
	scissorRect.top = 0;
	scissorRect.bottom = Height;

	//build projection and view matrix
	XMMATRIX tmpMat = XMMatrixPerspectiveFovLH(45.0f*(3.14f / 180.0f), (float)Width / (float)Height, 0.1f, 1000.0f);
	XMStoreFloat4x4(&cameraProjMat, tmpMat);

	//set starting camera state
	cameraPosition = XMFLOAT4(0.0f, 2.0f, -4.0f, 0.0f);
	cameraTarget = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	cameraUp = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);

	//build view matrix
	XMVECTOR cPos = XMLoadFloat4(&cameraPosition);
	XMVECTOR cTarg = XMLoadFloat4(&cameraTarget);
	XMVECTOR cUp = XMLoadFloat4(&cameraUp);
	tmpMat = XMMatrixLookAtLH(cPos, cTarg, cUp);
	XMStoreFloat4x4(&cameraViewMat, tmpMat);

	//set cube starting positions
	//first cube
	cube1Position = XMFLOAT4(0, 0, 0, 0);
	XMVECTOR posVec = XMLoadFloat4(&cube1Position);
	
	tmpMat = XMMatrixTranslationFromVector(posVec); //create translation matrix from cube1's position vector
	XMStoreFloat4x4(&cube1RotMat, XMMatrixIdentity()); //initialize cube1's rotation matrix to identity matrix
	XMStoreFloat4x4(&cube1WorldMat, tmpMat); //store world matrix

	//second cube
	cube2PositionOffset = XMFLOAT4(1.5f, 0, 0, 0);
	posVec = XMLoadFloat4(&cube2PositionOffset) + XMLoadFloat4(&cube1Position); //we are rotating cube 2 around cube one so add positions

	tmpMat = XMMatrixTranslationFromVector(posVec); //create translation matrix from cube2's position offset vector
	XMStoreFloat4x4(&cube2RotMat, XMMatrixIdentity()); //initialize cube2's rotation matrix to identity matrix
	XMStoreFloat4x4(&cube2WorldMat, tmpMat); //store world matrix



	return true;
}

void Update() {
	//update app logic
	
	//create rotation matrices (you wouldn't normally do this every frame but alright)
	XMMATRIX rotXMat = XMMatrixRotationX(0.0001f);
	XMMATRIX rotYMat = XMMatrixRotationY(0.0002f);
	XMMATRIX rotZMat = XMMatrixRotationZ(0.0003f);

	//add rotation to cube1's rotation matrix and store it
	XMMATRIX rotMat = XMLoadFloat4x4(&cube1RotMat) * rotXMat * rotYMat * rotZMat;
	XMStoreFloat4x4(&cube1RotMat, rotMat);

	//create translatiom matrix for cube1 from position vector
	XMMATRIX translationMat = XMMatrixTranslationFromVector(XMLoadFloat4(&cube1Position));

	//create cube1's world matrix by rotating and then positioning the rotated cube
	XMMATRIX worldMat = rotMat * translationMat;

	//store cube1's world matrix
	XMStoreFloat4x4(&cube1WorldMat, worldMat);

	// update constant buffer for cube1
	// create the wvp matrix and store in constant buffer
	XMMATRIX viewMat = XMLoadFloat4x4(&cameraViewMat); // load view matrix
	XMMATRIX projMat = XMLoadFloat4x4(&cameraProjMat); // load projection matrix
	XMMATRIX wvpMat = XMLoadFloat4x4(&cube1WorldMat) * viewMat * projMat; // create wvp matrix
	XMMATRIX transposed = XMMatrixTranspose(wvpMat); // must transpose wvp matrix for the gpu
	XMStoreFloat4x4(&cbPerObject.wvpMat, transposed); // store transposed wvp matrix in constant buffer

													  // copy our ConstantBuffer instance to the mapped constant buffer resource
	memcpy(cbvGPUAddress[frameIndex], &cbPerObject, sizeof(cbPerObject));

	// now do cube2's world matrix
	// create rotation matrices for cube2
	rotXMat = XMMatrixRotationX(0.0003f);
	rotYMat = XMMatrixRotationY(0.0002f);
	rotZMat = XMMatrixRotationZ(0.0001f);

	// add rotation to cube2's rotation matrix and store it
	rotMat = rotZMat * (XMLoadFloat4x4(&cube2RotMat) * (rotXMat * rotYMat));
	XMStoreFloat4x4(&cube2RotMat, rotMat);

	// create translation matrix for cube 2 to offset it from cube 1 (its position relative to cube1
	XMMATRIX translationOffsetMat = XMMatrixTranslationFromVector(XMLoadFloat4(&cube2PositionOffset));

	// we want cube 2 to be half the size of cube 1, so we scale it by .5 in all dimensions
	XMMATRIX scaleMat = XMMatrixScaling(0.5f, 0.5f, 0.5f);

	// reuse worldMat. 
	// first we scale cube2. scaling happens relative to point 0,0,0, so you will almost always want to scale first
	// then we translate it. 
	// then we rotate it. rotation always rotates around point 0,0,0
	// finally we move it to cube 1's position, which will cause it to rotate around cube 1
	worldMat = scaleMat * translationOffsetMat * rotMat * translationMat;

	wvpMat = XMLoadFloat4x4(&cube2WorldMat) * viewMat * projMat; // create wvp matrix
	transposed = XMMatrixTranspose(wvpMat); // must transpose wvp matrix for the gpu
	XMStoreFloat4x4(&cbPerObject.wvpMat, transposed); // store transposed wvp matrix in constant buffer

	// copy our ConstantBuffer instance to the mapped constant buffer resource
	memcpy(cbvGPUAddress[frameIndex] + ConstantBufferPerObjectAlignedSize, &cbPerObject, sizeof(cbPerObject));

	// store cube2's world matrix
	XMStoreFloat4x4(&cube2WorldMat, worldMat);
}

void UpdatePipeline() {
	HRESULT hr;

	//wait for the gpu to finish with the command allocator before we reset it
	WaitForPreviousFrame();

	//resetting an allocator frees the memory that the command list was stored in
	hr = commandAllocator[frameIndex]->Reset();
	if (FAILED(hr))
		Running = false;

	//reset the command list. by resetting it it will be put into the recording state so we can start recording commands.
	//the command allocator can have multiple command lists associated with it but only one command list can recording at the time.
	//so make sure any other command list is in closed state
	//here you will pass an initial pipeline state object(pso) as the second paramter
	hr = commandList->Reset(commandAllocator[frameIndex], pipelineStateObject);
	if (FAILED(hr))
		Running = false;

	// this is where the commands are recorded into the command list //

	//transition the 'frameIndex' render target from the present state to the render target state so the command list drawas to it starting from here
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	//get the handle to our current rtv so we can set it as the render target view for the output merger
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);

	//get handle for the depth/stencil buffer
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	//set the render target for the output merger stage
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	//set the render target for the output merger stage
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	//Clear the render target to the specified clear color
	const float clearColor[] = { 0.0f,0.2f,0.4f,1.0f };
	commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	//set the root signature
	commandList->SetGraphicsRootSignature(rootSignature); 

	//set the descriptor heap
	ID3D12DescriptorHeap* descriptorHeaps[] = { mainDescriptorHeap };
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	//set the descriptor table to the descriptor heap (parameter 1, as constant buffer root descriptor is parameter index 0)
	commandList->SetGraphicsRootDescriptorTable(1, mainDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	
	//draw!
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
	commandList->IASetIndexBuffer(&indexBufferView);
	
	//first cube
	
	//set cube1's constant buffer
	commandList->SetGraphicsRootConstantBufferView(0, constantBufferUploadHeaps[frameIndex]->GetGPUVirtualAddress());

	//draw cube1
	commandList->DrawIndexedInstanced(numCubeIndices, 1, 0, 0, 0);

	//second cube

	//set cube2's constant buffer. we add the size of the ConstantBufferPerObject (256 bits) to the constatn buffer address.
	//this is because cube2's constant buffer data is stored after the first one(256 bits from the start of the heap)
	commandList->SetGraphicsRootConstantBufferView(0, constantBufferUploadHeaps[frameIndex]->GetGPUVirtualAddress() + ConstantBufferPerObjectAlignedSize);

	//draw second cube
	commandList->DrawIndexedInstanced(numCubeIndices, 1, 0, 0, 0);

	//transition the 'frameIndex' render target from the render target state to the present state.
	//if the debug layer is enabled you will receive an error if present is called on a render target that is not in present state
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	//close the command list. if there where errors in the command list the program will break here
	hr = commandList->Close();
	if (FAILED(hr))
		Running = false;
}

void Render() {
	HRESULT hr;

	UpdatePipeline(); //update the pipeline by sending commands to the commandqueue

	//create an array of command lists (only one command list right now)
	ID3D12CommandList* ppCommandLists[] = { commandList };

	//execute the array of command lists
	commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	//add the fence command at the end of the command queue so we know when the command queue has finished executing
	hr = commandQueue->Signal(fence[frameIndex], fenceValue[frameIndex]);
	if (FAILED(hr))
		Running = false;

	//present the current backbuffer
	hr = swapChain->Present(0, 0);
	if (FAILED(hr))
		Running = false;
}

void Cleanup() {
	// wait for the gpu to finish all frames
	for (int i = 0; i < frameBufferCount; i++)
	{
		frameIndex = i;
		WaitForPreviousFrame();
	}

	//get the swapchain out of full screen before exiting
	BOOL fs = false;
	if (swapChain->GetFullscreenState(&fs, NULL))
		swapChain->SetFullscreenState(false, NULL);

	SAFE_RELEASE(device);
	SAFE_RELEASE(swapChain);
	SAFE_RELEASE(commandQueue);
	SAFE_RELEASE(rtvDescriptorHeap);
	SAFE_RELEASE(commandList);
	SAFE_RELEASE(pipelineStateObject);
	SAFE_RELEASE(rootSignature);
	SAFE_RELEASE(vertexBuffer);
	SAFE_RELEASE(indexBuffer);
	SAFE_RELEASE(depthStencilBuffer);
	SAFE_RELEASE(dsDescriptorHeap);

	for (int i = 0; i < frameBufferCount; i++)
	{
		SAFE_RELEASE(renderTargets[i]);
		SAFE_RELEASE(commandAllocator[i]);
		SAFE_RELEASE(fence[i]);
		SAFE_RELEASE(constantBufferUploadHeaps[i]);
	}
}

void WaitForPreviousFrame() {
	HRESULT hr;

	//if the current fence value is still less than 'fenceValue' then we know the gpu has not finished executing
	//the command queue since it has not reached the 'commandQueue->Signal()' command
	if (fence[frameIndex]->GetCompletedValue() < fenceValue[frameIndex]) {
		//we have the fence create an event which is signaled once the fence's value is 'fenceValue'
		hr = fence[frameIndex]->SetEventOnCompletion(fenceValue[frameIndex], fenceEvent);
		if (FAILED(hr))
			Running = false;

		//we will wait untill the fence has triggered the event once the fence has reached 'fenceValue'
		WaitForSingleObject(fenceEvent, INFINITE);

	}
	//increment fenceValue for next frame
	fenceValue[frameIndex]++;

	//swap the current rtv buffer index so we draw on the correct buffer
	frameIndex = swapChain->GetCurrentBackBufferIndex();
}

int LoadImageDataFromFile(BYTE** imageData, D3D12_RESOURCE_DESC& resourceDescription, LPCWSTR filename, int &bytesPerRow) {
	HRESULT hr;

	//we only need one instance of the imaging factory to create decoders and frames
	static IWICImagingFactory *wicFactory;

	//reset decoder, frame, and converter since these will be different for each image we load
	IWICBitmapDecoder *wicDecoder = NULL;
	IWICBitmapFrameDecode *wicFrame = NULL;
	IWICFormatConverter *wicConverter = NULL;

	bool imageConverted = false;

	if (wicFactory == NULL) {
		//initialize the COM library
		CoInitialize(NULL);

		//create the WIC factory
		hr = CoCreateInstance(
			CLSID_WICImagingFactory,
			NULL,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&wicFactory)
		);
		if (FAILED(hr))
			return 0;
	}

	//load a decoder for the image
	hr = wicFactory->CreateDecoderFromFilename(
		filename,								//image we want to load
		NULL,									//this is a vendor id, we have no preference so set it to null
		GENERIC_READ,							//we want to read this file
		WICDecodeMetadataCacheOnLoad,			//we will cache the metadata right away, rather than when needed
		&wicDecoder								//the wic decoder we created
	);

	if (FAILED(hr))
		return 0;

	//decode the first frame
	hr = wicDecoder->GetFrame(0, &wicFrame);
	if (FAILED(hr))
		return 0;

	//get the wic pixel format
	WICPixelFormatGUID pixelFormat;
	hr = wicFrame->GetPixelFormat(&pixelFormat);
	if (FAILED(hr))
		return 0;

	//get the size of the image
	UINT textureWidth, textureHeight;
	hr = wicFrame->GetSize(&textureWidth, &textureHeight);
	if (FAILED(hr))
		return 0;

	//convert wic pixel format to dxgi pixel format
	DXGI_FORMAT dxgiFormat = GetDXGIFormatFromWICFormat(pixelFormat);

	//try to convert image if the format is not suported dxgi format
	if (dxgiFormat == DXGI_FORMAT_UNKNOWN) {
		//get a dxgi compatible wic format from the current image format
		WICPixelFormatGUID convertToPixelFormat = GetConvertToWICFormat(pixelFormat);

		//return if no dxgi compatible format was found
		if (convertToPixelFormat == GUID_WICPixelFormatDontCare)
			return 0;

		//set the dxgi format
		dxgiFormat = GetDXGIFormatFromWICFormat(convertToPixelFormat);

		//create the format converter
		hr = wicFactory->CreateFormatConverter(&wicConverter);
		if (FAILED(hr))
			return 0;

		//make sure we can convert to the dxgi compatible format
		BOOL canConvert = FALSE;
		hr = wicConverter->CanConvert(pixelFormat, convertToPixelFormat, &canConvert);
		if (FAILED(hr) || !canConvert)
			return 0;

		//this is so we know to get the image data from wicConverter instead of from wicFrame
		imageConverted = true;
	}

	int bitsPerPixel = GetDXGIFormatBitsPerPixel(dxgiFormat); //number of bits per pixel
	bytesPerRow = (textureWidth * bitsPerPixel) / 8; //number of bytes in each row of the image data
	int imageSize = bytesPerRow * textureHeight; //total image size in bytes

	//allocate enough memory for the raw image data, and set imageData to point to that memory
	*imageData = (BYTE*)malloc(imageSize);

	//copy (decoded) raw image data into the newly allocated memory
	if (imageConverted) {
		//if the imaged needed to be converted the wic converter will contain the converted image
		hr = wicConverter->CopyPixels(0, bytesPerRow, imageSize, *imageData);
		if (FAILED(hr))
			return 0;
		std::cout << "converted image loaded into memory" << std::endl;

	}
	else {
		//no need to convert, just copy the data from the wic frame
		hr = wicFrame->CopyPixels(0, bytesPerRow, imageSize, *imageData);
		if (FAILED(hr))
			return 0;
	}

	//now describe the texture with the information we have obtained from the image
	resourceDescription = {};
	resourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDescription.Alignment = 0; //let the driver decide the alignment based on the size of the image and the number of mips. could set manually for more control
	resourceDescription.Width = textureWidth;
	resourceDescription.Height = textureHeight;
	resourceDescription.DepthOrArraySize = 1; //if 3d image, depth of the 3d image. otherwise size of the array of textures (we only have 1 texture in this case)
	resourceDescription.MipLevels = 1; //not generating any mips for now
	resourceDescription.Format = dxgiFormat;
	resourceDescription.SampleDesc.Count = 1; //no msaa
	resourceDescription.SampleDesc.Quality = 0; //no msaa
	resourceDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; //the arrangement of the pixels. setting to unknown lets the driver choose the most efficient one
	resourceDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

	//return the image size of the image. remember to delete the image once you're done with it (in this case, once it's uploaded to the gpu)
	return imageSize;
}

// get the dxgi format equivilent of a wic format
DXGI_FORMAT GetDXGIFormatFromWICFormat(WICPixelFormatGUID& wicFormatGUID)

{

	if (wicFormatGUID == GUID_WICPixelFormat128bppRGBAFloat) return DXGI_FORMAT_R32G32B32A32_FLOAT;

	else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBAHalf) return DXGI_FORMAT_R16G16B16A16_FLOAT;

	else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBA) return DXGI_FORMAT_R16G16B16A16_UNORM;

	else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA) return DXGI_FORMAT_R8G8B8A8_UNORM;

	else if (wicFormatGUID == GUID_WICPixelFormat32bppBGRA) return DXGI_FORMAT_B8G8R8A8_UNORM;

	else if (wicFormatGUID == GUID_WICPixelFormat32bppBGR) return DXGI_FORMAT_B8G8R8X8_UNORM;

	else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA1010102XR) return DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM;



	else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBA1010102) return DXGI_FORMAT_R10G10B10A2_UNORM;

	else if (wicFormatGUID == GUID_WICPixelFormat16bppBGRA5551) return DXGI_FORMAT_B5G5R5A1_UNORM;

	else if (wicFormatGUID == GUID_WICPixelFormat16bppBGR565) return DXGI_FORMAT_B5G6R5_UNORM;

	else if (wicFormatGUID == GUID_WICPixelFormat32bppGrayFloat) return DXGI_FORMAT_R32_FLOAT;

	else if (wicFormatGUID == GUID_WICPixelFormat16bppGrayHalf) return DXGI_FORMAT_R16_FLOAT;

	else if (wicFormatGUID == GUID_WICPixelFormat16bppGray) return DXGI_FORMAT_R16_UNORM;

	else if (wicFormatGUID == GUID_WICPixelFormat8bppGray) return DXGI_FORMAT_R8_UNORM;

	else if (wicFormatGUID == GUID_WICPixelFormat8bppAlpha) return DXGI_FORMAT_A8_UNORM;



	else return DXGI_FORMAT_UNKNOWN;

}

// get a dxgi compatible wic format from another wic format
WICPixelFormatGUID GetConvertToWICFormat(WICPixelFormatGUID& wicFormatGUID)

{

	if (wicFormatGUID == GUID_WICPixelFormatBlackWhite) return GUID_WICPixelFormat8bppGray;

	else if (wicFormatGUID == GUID_WICPixelFormat1bppIndexed) return GUID_WICPixelFormat32bppRGBA;

	else if (wicFormatGUID == GUID_WICPixelFormat2bppIndexed) return GUID_WICPixelFormat32bppRGBA;

	else if (wicFormatGUID == GUID_WICPixelFormat4bppIndexed) return GUID_WICPixelFormat32bppRGBA;

	else if (wicFormatGUID == GUID_WICPixelFormat8bppIndexed) return GUID_WICPixelFormat32bppRGBA;

	else if (wicFormatGUID == GUID_WICPixelFormat2bppGray) return GUID_WICPixelFormat8bppGray;

	else if (wicFormatGUID == GUID_WICPixelFormat4bppGray) return GUID_WICPixelFormat8bppGray;

	else if (wicFormatGUID == GUID_WICPixelFormat16bppGrayFixedPoint) return GUID_WICPixelFormat16bppGrayHalf;

	else if (wicFormatGUID == GUID_WICPixelFormat32bppGrayFixedPoint) return GUID_WICPixelFormat32bppGrayFloat;

	else if (wicFormatGUID == GUID_WICPixelFormat16bppBGR555) return GUID_WICPixelFormat16bppBGRA5551;

	else if (wicFormatGUID == GUID_WICPixelFormat32bppBGR101010) return GUID_WICPixelFormat32bppRGBA1010102;

	else if (wicFormatGUID == GUID_WICPixelFormat24bppBGR) return GUID_WICPixelFormat32bppRGBA;

	else if (wicFormatGUID == GUID_WICPixelFormat24bppRGB) return GUID_WICPixelFormat32bppRGBA;

	else if (wicFormatGUID == GUID_WICPixelFormat32bppPBGRA) return GUID_WICPixelFormat32bppRGBA;

	else if (wicFormatGUID == GUID_WICPixelFormat32bppPRGBA) return GUID_WICPixelFormat32bppRGBA;

	else if (wicFormatGUID == GUID_WICPixelFormat48bppRGB) return GUID_WICPixelFormat64bppRGBA;

	else if (wicFormatGUID == GUID_WICPixelFormat48bppBGR) return GUID_WICPixelFormat64bppRGBA;

	else if (wicFormatGUID == GUID_WICPixelFormat64bppBGRA) return GUID_WICPixelFormat64bppRGBA;

	else if (wicFormatGUID == GUID_WICPixelFormat64bppPRGBA) return GUID_WICPixelFormat64bppRGBA;

	else if (wicFormatGUID == GUID_WICPixelFormat64bppPBGRA) return GUID_WICPixelFormat64bppRGBA;

	else if (wicFormatGUID == GUID_WICPixelFormat48bppRGBFixedPoint) return GUID_WICPixelFormat64bppRGBAHalf;

	else if (wicFormatGUID == GUID_WICPixelFormat48bppBGRFixedPoint) return GUID_WICPixelFormat64bppRGBAHalf;

	else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBAFixedPoint) return GUID_WICPixelFormat64bppRGBAHalf;

	else if (wicFormatGUID == GUID_WICPixelFormat64bppBGRAFixedPoint) return GUID_WICPixelFormat64bppRGBAHalf;

	else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBFixedPoint) return GUID_WICPixelFormat64bppRGBAHalf;

	else if (wicFormatGUID == GUID_WICPixelFormat64bppRGBHalf) return GUID_WICPixelFormat64bppRGBAHalf;

	else if (wicFormatGUID == GUID_WICPixelFormat48bppRGBHalf) return GUID_WICPixelFormat64bppRGBAHalf;

	else if (wicFormatGUID == GUID_WICPixelFormat128bppPRGBAFloat) return GUID_WICPixelFormat128bppRGBAFloat;

	else if (wicFormatGUID == GUID_WICPixelFormat128bppRGBFloat) return GUID_WICPixelFormat128bppRGBAFloat;

	else if (wicFormatGUID == GUID_WICPixelFormat128bppRGBAFixedPoint) return GUID_WICPixelFormat128bppRGBAFloat;

	else if (wicFormatGUID == GUID_WICPixelFormat128bppRGBFixedPoint) return GUID_WICPixelFormat128bppRGBAFloat;

	else if (wicFormatGUID == GUID_WICPixelFormat32bppRGBE) return GUID_WICPixelFormat128bppRGBAFloat;

	else if (wicFormatGUID == GUID_WICPixelFormat32bppCMYK) return GUID_WICPixelFormat32bppRGBA;

	else if (wicFormatGUID == GUID_WICPixelFormat64bppCMYK) return GUID_WICPixelFormat64bppRGBA;

	else if (wicFormatGUID == GUID_WICPixelFormat40bppCMYKAlpha) return GUID_WICPixelFormat64bppRGBA;

	else if (wicFormatGUID == GUID_WICPixelFormat80bppCMYKAlpha) return GUID_WICPixelFormat64bppRGBA;



#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8) || defined(_WIN7_PLATFORM_UPDATE)

	else if (wicFormatGUID == GUID_WICPixelFormat32bppRGB) return GUID_WICPixelFormat32bppRGBA;

	else if (wicFormatGUID == GUID_WICPixelFormat64bppRGB) return GUID_WICPixelFormat64bppRGBA;

	else if (wicFormatGUID == GUID_WICPixelFormat64bppPRGBAHalf) return GUID_WICPixelFormat64bppRGBAHalf;

#endif



	else return GUID_WICPixelFormatDontCare;

}

// get the number of bits per pixel for a dxgi format
int GetDXGIFormatBitsPerPixel(DXGI_FORMAT& dxgiFormat)

{

	if (dxgiFormat == DXGI_FORMAT_R32G32B32A32_FLOAT) return 128;

	else if (dxgiFormat == DXGI_FORMAT_R16G16B16A16_FLOAT) return 64;

	else if (dxgiFormat == DXGI_FORMAT_R16G16B16A16_UNORM) return 64;

	else if (dxgiFormat == DXGI_FORMAT_R8G8B8A8_UNORM) return 32;

	else if (dxgiFormat == DXGI_FORMAT_B8G8R8A8_UNORM) return 32;

	else if (dxgiFormat == DXGI_FORMAT_B8G8R8X8_UNORM) return 32;

	else if (dxgiFormat == DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM) return 32;



	else if (dxgiFormat == DXGI_FORMAT_R10G10B10A2_UNORM) return 32;

	else if (dxgiFormat == DXGI_FORMAT_B5G5R5A1_UNORM) return 16;

	else if (dxgiFormat == DXGI_FORMAT_B5G6R5_UNORM) return 16;

	else if (dxgiFormat == DXGI_FORMAT_R32_FLOAT) return 32;

	else if (dxgiFormat == DXGI_FORMAT_R16_FLOAT) return 16;

	else if (dxgiFormat == DXGI_FORMAT_R16_UNORM) return 16;

	else if (dxgiFormat == DXGI_FORMAT_R8_UNORM) return 8;

	else if (dxgiFormat == DXGI_FORMAT_A8_UNORM) return 8;

}