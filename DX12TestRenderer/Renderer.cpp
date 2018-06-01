#include "Renderer.h"

ID3D12Device* Renderer::device = nullptr;
ID3D12GraphicsCommandList*  Renderer::commandList = nullptr;

Renderer::Renderer(HINSTANCE hInstance, HINSTANCE hPrevInstance, int nShowCmd)
{
	//create the window
	if (!InitializeWindow(hInstance, nShowCmd, FullScreen)) {
		MessageBox(0, L"Window initialization - Failed", L"Error", MB_OK);
		return;
	}

	//initialize direct3d
	if (!InitD3D()) {
		MessageBox(0, L"Failed to initialize direct3d 12", L"Error", MB_OK);
		Cleanup();
		return;
	}

	//start the main loop
	mainloop();

	//we want to wait for the gpu to finish executing the command list before we start releasing everything
	WaitForPreviousFrame();

	//close the fence event
	CloseHandle(fenceEvent);

	// clean up
	Cleanup();

}


Renderer::~Renderer()
{
}

bool Renderer::InitializeWindow(HINSTANCE hInstance, int ShowWnd, bool fullscreen) {
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

void Renderer::mainloop() {
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

//TODO: split into functions
bool Renderer::InitD3D() {
	HRESULT hr;
	IDXGIFactory4* dxgiFactory;

	// create the device //
	{
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
		//TODO: Add debug interface for better errors
	}

	// create the command queue //
	{
		D3D12_COMMAND_QUEUE_DESC cqDesc = {}; //using default command queue values;
		cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; //direct means the gpu can directly execute this command queue

		hr = device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&commandQueue)); // create the command queue
		if (FAILED(hr))
			return false;
	}

	// display mode settings
	DXGI_MODE_DESC backBufferDesc = {};
	backBufferDesc.Width = Width;
	backBufferDesc.Height = Height;
	backBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; //color format of the buffer (rgba 32 bit)

														// multi-sampling settings (not using it currently)
	DXGI_SAMPLE_DESC sampleDesc = {};
	sampleDesc.Count = 1; // 1 sample count is disables multi-sampling
						  //TODO: msaa

						  // create the swapchain (double/tripple buffering) //
	{
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
	}

	// create the back buffers (rtv's) discriptor heap //
	{
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
	}

	// create the command allocators //
	{
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
	}

	// create a fence and a fence event //
	{
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
	}

	//create root signature
	mat1 = new TextureMaterial(device, commandList, L"dive_scooter_Base1k.png");
	mat2 = new TextureMaterial(device, commandList, L"MantaRay_Base.png");

	///////////

	// Load the mesh data //
	{
		diveScooterMesh = Mesh::load("dive_scooter.obj", device, commandList);
		mantaMesh = Mesh::load("MantaRay.obj", device, commandList);
	}

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
	go1 = new GameObject("", vec3(0,0,0));
	go1->SetMesh(diveScooterMesh);
	go1->SetMaterial(mat1);

	go2 = new GameObject("", vec3(1.5f, 0, 0));
	go2->scale(vec3(0.02f));
	go2->SetMesh(mantaMesh);
	go2->SetMaterial(mat2);

	//create a resource heap, descriptor heap, and pointer to cbv for every framebuffer
	for (int i = 0; i < frameBufferCount; i++)
	{
		hr = device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(1024 * 64), //must be a multiple of 64kb thus 64 bytes * 1024 (4mb multiple for multi-sampled textures)
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

		//because of the constant read alignment requirements, constant buffer views must be 256 byte aligned. since our buffers are smaller than 256 bits
		//we just need to add the spacing between the two buffers, so the second buffer starts 256 byte from the beginning of the heap
		//
		//memcpy(cbvGPUAddress[i], &cbPerObject, sizeof(cbPerObject)); //cube1's constant buffer data
		//memcpy(cbvGPUAddress[i] + ConstantBufferPerObjectAlignedSize, &cbPerObject, sizeof(cbPerObject)); //cube2's constant buffer data
	}

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

	////we are done with the image data. it's uploaded to the gpu now. we can free up the (ram) memory
	//delete imageData; TODO

	//setup viewport and scene objects //
	{
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
		cameraProjMat = glm::perspective(glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 1000.0f);

		//set starting camera state
		camPos = glm::vec3(0, 2, -4);
		camTarget = glm::vec3(0);
		camUp = glm::vec3(0, 1, 0);

		//build view matrix
		cameraViewMat = glm::lookAt(camPos, camTarget, camUp);


	}

	return true;
}

void Renderer::Update() {
	//update app logic

	// update constant buffer for cube1
	// create the wvp matrix and store in constant buffer
	go1->SetTransform(glm::rotate(go1->GetTransform(), .0001f, glm::vec3(1, 2, 3)));

	glm::mat4 cb = glm::transpose(cameraProjMat * cameraViewMat * go1->GetTransform());
													  // copy our ConstantBuffer instance to the mapped constant buffer resource
	memcpy(cbvGPUAddress[frameIndex] + ConstantBufferPerObjectAlignedSize * go1->_constantBufferID, &cb, sizeof(cb));

	// now do cube2's world matrix
	// create rotation matrices for cube2
	go2->SetTransform(glm::rotate(go2->GetTransform(), .0001f, glm::vec3(3, 2, 1)));
	cb = glm::transpose(cameraProjMat * cameraViewMat * go1->GetTransform() * go2->GetTransform());

	// copy our ConstantBuffer instance to the mapped constant buffer resource
	memcpy(cbvGPUAddress[frameIndex] + ConstantBufferPerObjectAlignedSize * go2->_constantBufferID, &cb, sizeof(cb));
}

void Renderer::UpdatePipeline() {
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
	hr = commandList->Reset(commandAllocator[frameIndex], NULL);
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
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

	//Clear the render target to the specified clear color
	const float clearColor[] = { 0.0f,0.2f,0.4f,1.0f };
	commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	//set the root signature

	//draw!
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//first cube
	mat1->Render(diveScooterMesh, constantBufferUploadHeaps[frameIndex]->GetGPUVirtualAddress() + ConstantBufferPerObjectAlignedSize * go1->_constantBufferID);

	//second cube

	//set cube2's constant buffer. we add the size of the ConstantBufferPerObject (256 bits) to the constatn buffer address.
	//this is because cube2's constant buffer data is stored after the first one(256 bits from the start of the heap)	
	mat2->Render(mantaMesh, constantBufferUploadHeaps[frameIndex]->GetGPUVirtualAddress() + ConstantBufferPerObjectAlignedSize * go2->_constantBufferID);

	//transition the 'frameIndex' render target from the render target state to the present state.
	//if the debug layer is enabled you will receive an error if present is called on a render target that is not in present state
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	//close the command list. if there where errors in the command list the program will break here
	hr = commandList->Close();
	if (FAILED(hr))
		Running = false;
}

void Renderer::Render() {
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

void Renderer::Cleanup() {
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
	//SAFE_RELEASE(pipelineStateObject);
	//SAFE_RELEASE(rootSignature);
	SAFE_RELEASE(depthStencilBuffer);
	SAFE_RELEASE(dsDescriptorHeap);

	for (int i = 0; i < frameBufferCount; i++)
	{
		SAFE_RELEASE(renderTargets[i]);
		SAFE_RELEASE(commandAllocator[i]);
		SAFE_RELEASE(fence[i]);
		//SAFE_RELEASE(constantBufferUploadHeaps[i]);
	}
}

void Renderer::WaitForPreviousFrame() {
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
