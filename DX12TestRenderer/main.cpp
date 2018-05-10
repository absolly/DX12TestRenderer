#include "stdafx.h"

using namespace DirectX;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nShowCmd) {

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
	MainLoop();

	//we want to wait for the gpu to finish executing the command list before we start releasing everything
	WaitForPreviousFrame();

	//close the fence event
	CloseHandle(fenceEvent);

	// clean up
	Cleanup();

	return 0;
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

	// create teh back buffers (rtv's) discriptor heap //

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
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator[0], NULL, IID_PPV_ARGS(&commandList));
	if (FAILED(hr))
		return false;

	//command lists are created in a recording state. our main loop will set it up for recording agains so close it for now.
	commandList->Close();

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

	return true;
}

void MainLoop() {
	MSG msg;
	ZeroMemory(&msg, sizeof(MSG));

	while (Running) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				break;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
			Update();
			Render();
		}
	}
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

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam){
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

void Update() {
	//update app logic
}

void Render() {
	HRESULT hr;

	UpdatePipeline(); //update teh pipeline by sending commands to the commandqueue

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
	//here you will pass an initial pipeline state object(pso) as the second paramter,
	//here we are only clearing the rtv, so we don't need anything but an initial default pipeline.
	//this is why we use null as teh second paramter
	hr = commandList->Reset(commandAllocator[frameIndex], NULL);
	if (FAILED(hr))
		Running = false;

	// this is where the commands are recorded into the command list //

	//transition the 'frameIndex' render target from the present state to the render target state so the command list drawas to it starting from here
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	//get the handle to our current rtv so we can set it as the render target view for the output merger
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);

	//set the render target for the output merger stage
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	//Clear the render target to the specified clear color
	const float clearColor[] = { 0.0f,0.2f,0.4f,1.0f };
	commandList->ClearRenderTargetView(rtvHandle,clearColor,0,nullptr);

	//transition the 'frameIndex' render target from the render target state to the present state.
	//if the debug layer is enabled you will receive an error if present is called on a render target that is not in present state
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	//close the command list. if there where errors in the command list the program will break here
	hr = commandList->Close();
	if (FAILED(hr))
		Running = false;
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

void Cleanup() {
	// wait for the gpu to finish all frames
	for (int i = 0; i < frameBufferCount; i++)
	{
		frameIndex = i;
		WaitForPreviousFrame();
	}

	//get teh swapchain out of full screen before exiting
	BOOL fs = false;
	if (swapChain->GetFullscreenState(&fs, NULL))
		swapChain->SetFullscreenState(false, NULL);

	SAFE_RELEASE(device);
	SAFE_RELEASE(swapChain);
	SAFE_RELEASE(commandQueue);
	SAFE_RELEASE(rtvDescriptorHeap);
	SAFE_RELEASE(commandList);
	
	for (int i = 0; i < frameBufferCount; i++)
	{
		SAFE_RELEASE(renderTargets[i]);
		SAFE_RELEASE(commandAllocator[i]);
		SAFE_RELEASE(fence[i]);
	}
}
