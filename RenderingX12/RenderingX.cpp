//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "RenderingX.h"

using namespace std;
using namespace XUSG;

RenderingX::RenderingX(uint32_t width, uint32_t height, std::wstring name) :
	DXFramework(width, height, name),
	m_frameParity(0),
	m_frameIndex(0),
	m_useIBL(true),
	m_isPaused(false),
	m_isTracking(false),
	m_sceneFile(L"Media/Scene.json")
{
#if defined (_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	AllocConsole();
	FILE* stream;
	freopen_s(&stream, "CONOUT$", "w+t", stdout);
	freopen_s(&stream, "CONIN$", "r+t", stdin);
#endif
}

RenderingX::~RenderingX()
{
#if defined (_DEBUG)
	FreeConsole();
#endif
}

void RenderingX::OnInit()
{
	LoadPipeline();
	LoadAssets();
}

// Load the rendering pipeline dependencies.
void RenderingX::LoadPipeline()
{
	auto dxgiFactoryFlags = 0u;

#if defined(_DEBUG)
	// Enable the debug layer (requires the Graphics Tools "optional feature").
	// NOTE: Enabling the debug layer after device creation will invalidate the active device.
	{
		com_ptr<ID3D12Debug1> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
			//debugController->SetEnableGPUBasedValidation(TRUE);

			// Enable additional debug layers.
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));

	DXGI_ADAPTER_DESC1 dxgiAdapterDesc;
	com_ptr<IDXGIAdapter1> dxgiAdapter;
	auto hr = DXGI_ERROR_UNSUPPORTED;
	for (auto i = 0u; hr == DXGI_ERROR_UNSUPPORTED; ++i)
	{
		dxgiAdapter = nullptr;
		ThrowIfFailed(m_factory->EnumAdapters1(i, &dxgiAdapter));
		hr = D3D12CreateDevice(dxgiAdapter.get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
	}

	dxgiAdapter->GetDesc1(&dxgiAdapterDesc);
	if (dxgiAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		m_title += dxgiAdapterDesc.VendorId == 0x1414 && dxgiAdapterDesc.DeviceId == 0x8c ? L" (WARP)" : L" (Software)";
	ThrowIfFailed(hr);

	// Create the command queue.
	N_RETURN(m_device->GetCommandQueue(m_commandQueue, CommandListType::DIRECT, CommandQueueFlags::NONE), ThrowIfFailed(E_FAIL));

	// Create the swap chain.
	CreateSwapchain();

	// Reset the index to the current back buffer.
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// Create a command allocator for each frame.
	for (auto n = 0u; n < FrameCount; ++n)
		N_RETURN(m_device->GetCommandAllocator(m_commandAllocators[n], CommandListType::DIRECT), ThrowIfFailed(E_FAIL));

	// Create descriptor table cache.
	m_descriptorTableCache = DescriptorTableCache::MakeShared(m_device, L"DescriptorTableCache");

}

// Load the sample assets.
void RenderingX::LoadAssets()
{
	m_shaderPool = ShaderPool::MakeShared();
	m_graphicsPipelineCache = Graphics::PipelineCache::MakeShared(m_device);
	m_computePipelineCache = Compute::PipelineCache::MakeShared(m_device);
	m_pipelineLayoutCache = PipelineLayoutCache::MakeShared(m_device);

	// Create the command list.
	m_commandList = CommandList::MakeUnique();
	N_RETURN(m_device->GetCommandList(m_commandList->GetCommandList(), 0, CommandListType::DIRECT,
		m_commandAllocators[m_frameIndex], nullptr), ThrowIfFailed(E_FAIL));

	// Load scene asset
	vector<Resource> uploaders;
	{
		Blob sceneFileBlob;
		D3DReadFileToBlob(m_sceneFile.c_str(), &sceneFileBlob);
		N_RETURN(sceneFileBlob, ThrowIfFailed(E_FAIL));

		const string sceneString = reinterpret_cast<char*>(sceneFileBlob->GetBufferPointer());

		tiny::TinyJson sceneReader;
		sceneReader.ReadJson(sceneString);

		// Create scene
		m_scene = Scene::MakeUnique(m_device);
		//m_scene->SetRenderTarget(m_rtHDR, m_depth);
		N_RETURN(m_scene->LoadAssets(&sceneReader, m_commandList.get(), m_shaderPool,
			m_graphicsPipelineCache, m_computePipelineCache, m_pipelineLayoutCache,
			m_descriptorTableCache, uploaders, FormatHDR, FormatDepth, m_useIBL),
			ThrowIfFailed(E_FAIL));
	}

	{
		m_postprocess = Postprocess::MakeUnique(m_device);
		N_RETURN(m_postprocess->Init(m_shaderPool, m_graphicsPipelineCache,
			m_computePipelineCache, m_pipelineLayoutCache, m_descriptorTableCache,
			FormatHDR, FormatLDR), ThrowIfFailed(E_FAIL));
	}

	// Close the command list and execute it to begin the initial GPU setup.
	ThrowIfFailed(m_commandList->Close());
	BaseCommandList* const ppCommandLists[] = { m_commandList->GetCommandList().get() };
	m_commandQueue->ExecuteCommandLists(static_cast<uint32_t>(size(ppCommandLists)), ppCommandLists);

	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	{
		N_RETURN(m_device->GetFence(m_fence, m_fenceValues[m_frameIndex]++, FenceFlag::NONE), ThrowIfFailed(E_FAIL));

		// Create an event handle to use for frame synchronization.
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		// Wait for the command list to execute; we are reusing the same command 
		// list in our main loop but for now, we just want to wait for setup to 
		// complete before continuing.
		WaitForGpu();
	}

	// Setup the camera's view parameters
	{
		const auto focus_dist = m_scene->GetFocusAndDistance();
		const auto viewDist = XMVectorGetW(focus_dist);
		const auto viewDisp = XMVectorSet(0.0f, 0.0f, viewDist, 0.0f);
		const auto eyePt = focus_dist - viewDisp;
		const auto view = XMMatrixLookAtLH(eyePt, focus_dist, XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f));
		XMStoreFloat3(&m_eyePt, eyePt);
		XMStoreFloat4x4(&m_view, view);
	}
}

void RenderingX::CreateSwapchain()
{
	// Describe and create the swap chain.
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = m_width;
	swapChainDesc.Height = m_height;
	swapChainDesc.Format = static_cast<DXGI_FORMAT>(FormatLDR);;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	com_ptr<IDXGISwapChain1> swapChain;
	ThrowIfFailed(m_factory->CreateSwapChainForHwnd(
		m_commandQueue.get(),		// Swap chain needs the queue so that it can force a flush on it.
		Win32Application::GetHwnd(),
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain
	));

	// Store the swap chain.
	ThrowIfFailed(swapChain.As(&m_swapChain));

	// This class does not support exclusive full-screen mode and prevents DXGI from responding to the ALT+ENTER shortcut.
	ThrowIfFailed(m_factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));
}

void RenderingX::CreateResources()
{
	// Obtain the back buffers for this window which will be the final render targets
	// and create render target views for each of them.
	for (auto n = 0u; n < FrameCount; ++n)
	{
		m_renderTargets[n] = RenderTarget::MakeUnique();
		N_RETURN(m_renderTargets[n]->CreateFromSwapChain(m_device, m_swapChain, n), ThrowIfFailed(E_FAIL));
	}

	// Create TAA RTs
	for (auto n = 0u; n < 2; ++n)
	{
		m_rtTAAs[n] = RenderTarget::MakeUnique();
		N_RETURN(m_rtTAAs[n]->Create(m_device, m_width, m_height, FormatLDR, 1, ResourceFlag::NONE,
			1, 1, nullptr, false, (L"TemporalAA_RT" + to_wstring(n)).c_str()), ThrowIfFailed(E_FAIL));
	}

	// Create HDR RT
	m_rtHDR = RenderTarget::MakeUnique();
	N_RETURN(m_rtHDR->Create(m_device, m_width, m_height, FormatHDR, 1, ResourceFlag::NONE,
		1, 1, nullptr, false, L"HDR_RT"), ThrowIfFailed(E_FAIL));

	// Create LDR RT
	m_rtLDR = RenderTarget::MakeUnique();
	N_RETURN(m_rtLDR->Create(m_device, m_width, m_height, FormatLDR, 1, ResourceFlag::NONE,
		1, 1, nullptr, false, L"LDR_RT"),
		ThrowIfFailed(E_FAIL));

	// Create a DSV
	m_depth = DepthStencil::MakeUnique();
	N_RETURN(m_depth->Create(m_device, m_width, m_height, Format::UNKNOWN, ResourceFlag::NONE,
		1, 1, 1, 1.0f, 0, false, L"Depth"), ThrowIfFailed(E_FAIL));

	// Set the 3D rendering viewport and scissor rectangle to target the entire window.
	m_viewport = Viewport(0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height));
	m_scissorRect = RectRange(0, 0, m_width, m_height);
}

void RenderingX::ResizeAssets()
{
	ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());
	ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex], nullptr));

	// Scene
	vector<Resource> uploaders;
	N_RETURN(m_scene->ChangeWindowSize(m_commandList.get(), uploaders, *m_rtHDR, *m_depth), ThrowIfFailed(E_FAIL));

	// Post process
	{
		N_RETURN(m_postprocess->ChangeWindowSize(*m_rtHDR, m_scene->GetGBufferSRV(Scene::ALBEDO_IDX)), ThrowIfFailed(E_FAIL));

		// Create Descriptor tables
		const auto srvTable = Util::DescriptorTable::MakeUnique();
		const Descriptor srvs[] = { m_rtHDR->GetSRV(), m_depth->GetSRV() };
		srvTable->SetDescriptors(0, static_cast<uint32_t>(size(srvs)), srvs, Postprocess::RESIZABLE_POOL);
		X_RETURN(m_srvTables[SRV_HDR], srvTable->GetCbvSrvUavTable(*m_descriptorTableCache), ThrowIfFailed(E_FAIL));

		for (auto n = 0u; n < 2; ++n)
		{
			X_RETURN(m_srvTables[SRV_AA_INPUT + n], m_postprocess->CreateTemporalAASRVTable(
				m_rtLDR->GetSRV(), m_rtTAAs[!n]->GetSRV(), m_scene->GetGBufferSRV(Scene::MOTION_IDX),
				m_scene->GetGBufferSRV(Scene::MATENC_IDX)), ThrowIfFailed(E_FAIL));

			const auto srvTable = Util::DescriptorTable::MakeUnique();
			srvTable->SetDescriptors(0, 1, &m_rtTAAs[n]->GetSRV(), Postprocess::RESIZABLE_POOL);
			X_RETURN(m_srvTables[SRV_ANTIALIASED + n], srvTable->GetCbvSrvUavTable(*m_descriptorTableCache), ThrowIfFailed(E_FAIL));
		}
	}

	// Close the command list and execute it to begin the initial GPU setup.
	ThrowIfFailed(m_commandList->Close());
	BaseCommandList* const ppCommandLists[] = { m_commandList->GetCommandList().get() };
	m_commandQueue->ExecuteCommandLists(static_cast<uint32_t>(size(ppCommandLists)), ppCommandLists);

	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	{
		N_RETURN(m_device->GetFence(m_fence, m_fenceValues[m_frameIndex]++, FenceFlag::NONE), ThrowIfFailed(E_FAIL));

		// Create an event handle to use for frame synchronization.
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (m_fenceEvent == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		// Wait for the command list to execute; we are reusing the same command 
		// list in our main loop but for now, we just want to wait for setup to 
		// complete before continuing.
		WaitForGpu();
	}
}

// Update frame-based values.
void RenderingX::OnUpdate()
{
	// Timer
	static auto time = 0.0, pauseTime = 0.0;

	m_timer.Tick();
	float timeStep;
	const auto totalTime = CalculateFrameStats(&timeStep);
	pauseTime = m_isPaused ? totalTime - time : pauseTime;
	timeStep = m_isPaused ? 0.0f : timeStep;
	time = totalTime - pauseTime;

	// Camera update for scene
	const auto eyePt = XMLoadFloat3(&m_eyePt);
	const auto view = XMLoadFloat4x4(&m_view);
	const auto proj = XMLoadFloat4x4(&m_proj);
	m_scene->Update(m_frameIndex, time, timeStep, view, proj, eyePt);
	m_postprocess->Update(m_scene->GetCBVTable(Scene::CBV_IMMUTABLE),
		m_scene->GetCBVTable(Scene::CBV_PER_FRAME_PS + m_frameIndex), timeStep);
}

// Render the scene.
void RenderingX::OnRender()
{
	// Record all the commands we need to render the scene into the command list.
	PopulateCommandList();

	// Execute the command list.
	BaseCommandList* const ppCommandLists[] = { m_commandList->GetCommandList().get() };
	m_commandQueue->ExecuteCommandLists(static_cast<uint32_t>(size(ppCommandLists)), ppCommandLists);

	// Present the frame.
	ThrowIfFailed(m_swapChain->Present(0, 0));

	MoveToNextFrame();
}

void RenderingX::OnDestroy()
{
	// Ensure that the GPU is no longer referencing resources that are about to be
	// cleaned up by the destructor.
	WaitForGpu();

	CloseHandle(m_fenceEvent);
}

void RenderingX::OnWindowSizeChanged(int width, int height)
{
	if (!Win32Application::GetHwnd())
	{
		throw std::exception("Call SetWindow with a valid Win32 window handle");
	}

	// Wait until all previous GPU work is complete.
	WaitForGpu();

	// Release resources that are tied to the swap chain and update fence values.
	for (auto n = 0u; n < FrameCount; ++n)
	{
		m_renderTargets[n].reset();
		m_fenceValues[n] = m_fenceValues[m_frameIndex];
	}
	m_descriptorTableCache->ResetDescriptorPool(CBV_SRV_UAV_POOL, Postprocess::RESIZABLE_POOL);
	m_descriptorTableCache->ResetDescriptorPool(RTV_POOL, Postprocess::RESIZABLE_POOL);

	// Determine the render target size in pixels.
	m_width = (max)(width, 1);
	m_height = (max)(height, 1);

	// If the swap chain already exists, resize it, otherwise create one.
	if (m_swapChain)
	{
		// If the swap chain already exists, resize it.
		const auto hr = m_swapChain->ResizeBuffers(FrameCount, m_width, m_height, DXGI_FORMAT_B8G8R8A8_UNORM, 0);

		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
#ifdef _DEBUG
			char buff[64] = {};
			sprintf_s(buff, "Device Lost on ResizeBuffers: Reason code 0x%08X\n", (hr == DXGI_ERROR_DEVICE_REMOVED) ? m_device->GetDeviceRemovedReason() : hr);
			OutputDebugStringA(buff);
#endif
			// If the device was removed for any reason, a new device and swap chain will need to be created.
			//HandleDeviceLost();

			// Everything is set up now. Do not continue execution of this method. HandleDeviceLost will reenter this method 
			// and correctly set up the new device.
			return;
	}
		else
		{
			ThrowIfFailed(hr);
		}
}
	else CreateSwapchain();

	// Reset the index to the current back buffer.
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// Create window size dependent resources.
	CreateResources();
	ResizeAssets();

	// Projection
	{
		const auto aspectRatio = m_width / static_cast<float>(m_height);
		const auto proj = XMMatrixPerspectiveFovLH(g_FOVAngleY, aspectRatio, g_zNear, g_zFar);
		XMStoreFloat4x4(&m_proj, proj);
	}
}

// User hot-key interactions.
void RenderingX::OnKeyUp(uint8_t key)
{
	switch (key)
	{
	case 0x20:	// case VK_SPACE:
		m_isPaused = !m_isPaused;
		break;
	}
}

// User camera interactions.
void RenderingX::OnLButtonDown(float posX, float posY)
{
	m_isTracking = true;
	m_mousePt = XMFLOAT2(posX, posY);
}

void RenderingX::OnLButtonUp(float posX, float posY)
{
	m_isTracking = false;
}

void RenderingX::OnMouseMove(float posX, float posY)
{
	if (m_isTracking)
	{
		const XMFLOAT2 dPos(m_mousePt.x - posX, m_mousePt.y - posY);
		const XMFLOAT2 radians(XM_2PI * dPos.y / m_height, XM_2PI * dPos.x / m_width);

		const auto focusPt = m_scene->GetFocusAndDistance();
		auto eyePt = XMLoadFloat3(&m_eyePt);

		const auto len = XMVectorGetX(XMVector3Length(focusPt - eyePt));
		auto transform = XMMatrixTranslation(0.0f, 0.0f, -len);
		transform *= XMMatrixRotationRollPitchYaw(radians.x, radians.y, 0.0f);
		transform *= XMMatrixTranslation(0.0f, 0.0f, len);

		const auto view = XMLoadFloat4x4(&m_view) * transform;
		const auto viewInv = XMMatrixInverse(nullptr, view);
		eyePt = viewInv.r[3];

		XMStoreFloat3(&m_eyePt, eyePt);
		XMStoreFloat4x4(&m_view, view);

		m_mousePt = XMFLOAT2(posX, posY);
	}
}

void RenderingX::OnMouseWheel(float deltaZ, float posX, float posY)
{
	const auto focusPt = m_scene->GetFocusAndDistance();
	auto eyePt = XMLoadFloat3(&m_eyePt);

	const auto len = XMVectorGetX(XMVector3Length(focusPt - eyePt));
	const auto transform = XMMatrixTranslation(0.0f, 0.0f, -len * deltaZ / 16.0f);

	const auto view = XMLoadFloat4x4(&m_view) * transform;
	const auto viewInv = XMMatrixInverse(nullptr, view);
	eyePt = viewInv.r[3];

	XMStoreFloat3(&m_eyePt, eyePt);
	XMStoreFloat4x4(&m_view, view);
}

void RenderingX::OnMouseLeave()
{
	m_isTracking = false;
}

void RenderingX::ParseCommandLineArgs(wchar_t* argv[], int argc)
{
	DXFramework::ParseCommandLineArgs(argv, argc);

	auto specifyWindowSize = 0;

	for (auto i = 1; i < argc; ++i)
	{
		if (_wcsnicmp(argv[i], L"-scene", wcslen(argv[i])) == 0 ||
			_wcsnicmp(argv[i], L"/scene", wcslen(argv[i])) == 0 && i + 1 < argc)
			m_sceneFile = argv[i + 1];
		else if ((_wcsnicmp(argv[i], L"-width", wcslen(argv[i])) == 0 ||
			_wcsnicmp(argv[i], L"/width", wcslen(argv[i])) == 0 ||
			_wcsnicmp(argv[i], L"-w", wcslen(argv[i])) == 0 ||
			_wcsnicmp(argv[i], L"/w", wcslen(argv[i])) == 0) && i + 1 < argc)
			specifyWindowSize = swscanf_s(argv[i + 1], L"%u", &m_width);
		else if ((_wcsnicmp(argv[i], L"-height", wcslen(argv[i])) == 0 ||
			_wcsnicmp(argv[i], L"/height", wcslen(argv[i])) == 0 ||
			_wcsnicmp(argv[i], L"-h", wcslen(argv[i])) == 0 ||
			_wcsnicmp(argv[i], L"/h", wcslen(argv[i])) == 0) && i + 1 < argc)
			specifyWindowSize = swscanf_s(argv[i + 1], L"%u", &m_height);
		else if (_wcsnicmp(argv[i], L"-noIBL", wcslen(argv[i])) == 0 ||
			_wcsnicmp(argv[i], L"/noIBL", wcslen(argv[i])) == 0)
			m_useIBL = false;
	}
}

void RenderingX::PopulateCommandList()
{
	// Command list allocators can only be reset when the associated 
	// command lists have finished execution on the GPU; apps should use 
	// fences to determine GPU execution progress.
	ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before 
	// re-recording.
	const auto pCommandList = m_commandList.get();
	ThrowIfFailed(pCommandList->Reset(m_commandAllocators[m_frameIndex], nullptr));

	// Record commands.
	//const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	//m_commandList.ClearRenderTargetView(m_renderTargets[m_frameIndex].GetRTV(), clearColor);
	m_scene->Render(pCommandList);

	// Postprocessing
	ResourceBarrier barriers[2];
	m_postprocess->Render(pCommandList, *m_rtLDR, *m_rtHDR, m_rtLDR->GetRTV(), m_srvTables[SRV_HDR]);

	// Temporal AA
	auto numBarriers = m_rtTAAs[m_frameParity]->SetBarrier(barriers, ResourceState::RENDER_TARGET);
	numBarriers = m_rtLDR->SetBarrier(barriers, ResourceState::PIXEL_SHADER_RESOURCE, numBarriers);
	pCommandList->Barrier(numBarriers, barriers);
	m_postprocess->Antialias(pCommandList, &m_rtTAAs[m_frameParity]->GetRTV(), m_srvTables[SRV_AA_INPUT + m_frameParity]);

	// Unsharp
	numBarriers = m_renderTargets[m_frameIndex]->SetBarrier(barriers, ResourceState::RENDER_TARGET);
	numBarriers = m_rtTAAs[m_frameParity]->SetBarrier(barriers, ResourceState::PIXEL_SHADER_RESOURCE, numBarriers);
	pCommandList->Barrier(numBarriers, barriers);
	m_postprocess->Unsharp(pCommandList, &m_renderTargets[m_frameIndex]->GetRTV(), m_srvTables[SRV_ANTIALIASED + m_frameParity]);
	m_frameParity = !m_frameParity;

	// Indicate that the back buffer will now be used to present.
	numBarriers = m_renderTargets[m_frameIndex]->SetBarrier(barriers, ResourceState::PRESENT);
	pCommandList->Barrier(numBarriers, barriers);

	ThrowIfFailed(pCommandList->Close());
}

// Wait for pending GPU work to complete.
void RenderingX::WaitForGpu()
{
	// Schedule a Signal command in the queue.
	ThrowIfFailed(m_commandQueue->Signal(m_fence.get(), m_fenceValues[m_frameIndex]));

	// Wait until the fence has been processed, and increment the fence value for the current frame.
	ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex]++, m_fenceEvent));
	WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
}

// Prepare to render the next frame.
void RenderingX::MoveToNextFrame()
{
	// Schedule a Signal command in the queue.
	const auto currentFenceValue = m_fenceValues[m_frameIndex];
	ThrowIfFailed(m_commandQueue->Signal(m_fence.get(), currentFenceValue));

	// Update the frame index.
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// If the next frame is not ready to be rendered yet, wait until it is ready.
	if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
	{
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	// Set the fence value for the next frame.
	m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}

double RenderingX::CalculateFrameStats(float* pTimeStep)
{
	static int frameCnt = 0;
	static double elapsedTime = 0.0;
	static double previousTime = 0.0;
	const auto totalTime = m_timer.GetTotalSeconds();
	++frameCnt;

	const auto timeStep = static_cast<float>(totalTime - elapsedTime);

	// Compute averages over one second period.
	if ((totalTime - elapsedTime) >= 1.0f)
	{
		float fps = static_cast<float>(frameCnt) / timeStep;	// Normalize to an exact second.

		frameCnt = 0;
		elapsedTime = totalTime;

		wstringstream windowText;
		windowText << setprecision(2) << fixed << L"    fps: " << fps;
		SetCustomWindowText(windowText.str().c_str());
	}

	if (pTimeStep)* pTimeStep = static_cast<float>(totalTime - previousTime);
	previousTime = totalTime;

	return totalTime;
}
