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
#include "stb_image_write.h"

using namespace std;
using namespace XUSG;

RenderingX::RenderingX(uint32_t width, uint32_t height, wstring name) :
	DXFramework(width, height, name),
	m_readBuffer(nullptr),
	m_frameParity(0),
	m_frameIndex(0),
	m_fence(nullptr),
	m_useIBL(true),
	m_showFPS(true),
	m_isPaused(false),
	m_useWarpDevice(false),
	m_isTracking(false),
	m_sceneFile(L"Assets/Scene.json"),
	m_screenShot(0)
{
#if defined (_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	AllocConsole();
	FILE* stream;
	freopen_s(&stream, "CONIN$", "r+t", stdin);
	freopen_s(&stream, "CONOUT$", "w+t", stdout);
	freopen_s(&stream, "CONOUT$", "w+t", stderr);
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
		com_ptr<ID3D12Debug1> debugController = nullptr;
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
	com_ptr<IDXGIAdapter1> dxgiAdapter = nullptr;
	com_ptr<ID3D12Device> device;
	auto hr = DXGI_ERROR_UNSUPPORTED;
	for (auto i = 0u; hr == DXGI_ERROR_UNSUPPORTED; ++i)
	{
		dxgiAdapter = nullptr;
		ThrowIfFailed(m_factory->EnumAdapters1(i, &dxgiAdapter));

		dxgiAdapter->GetDesc1(&dxgiAdapterDesc);
		if (m_useWarpDevice && dxgiAdapterDesc.DeviceId != 0x8c) continue;

		m_device = Device::MakeUnique(Api);
		hr = m_device->Create(dxgiAdapter.get(), D3D_FEATURE_LEVEL_11_0);
	}

	if (dxgiAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		m_title += dxgiAdapterDesc.VendorId == 0x1414 && dxgiAdapterDesc.DeviceId == 0x8c ? L" (WARP)" : L" (Software)";
	ThrowIfFailed(hr);

	// Create the command queue.
	m_commandQueue = CommandQueue::MakeUnique(Api);
	XUSG_N_RETURN(m_commandQueue->Create(m_device.get(), CommandListType::DIRECT, CommandQueueFlag::NONE,
		0, 0, L"CommandQueue"), ThrowIfFailed(E_FAIL));

	// Create the swap chain.
	CreateSwapchain();

	// Reset the index to the current back buffer.
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// Create a command allocator for each frame.
	for (uint8_t n = 0; n < FrameCount; ++n)
	{
		m_commandAllocators[n] = CommandAllocator::MakeUnique(Api);
		XUSG_N_RETURN(m_commandAllocators[n]->Create(m_device.get(), CommandListType::DIRECT,
			(L"CommandAllocator" + to_wstring(n)).c_str()), ThrowIfFailed(E_FAIL));
	}

	// Create descriptor-table lib.
	m_descriptorTableLib = DescriptorTableLib::MakeShared(m_device.get(), L"DescriptorTableLib", Api);
}

// Load the sample assets.
void RenderingX::LoadAssets()
{
	m_shaderLib = ShaderLib::MakeShared(Api);
	m_graphicsPipelineLib = Graphics::PipelineLib::MakeShared(m_device.get(), Api);
	m_computePipelineLib = Compute::PipelineLib::MakeShared(m_device.get(), Api);
	m_pipelineLayoutLib = PipelineLayoutLib::MakeShared(m_device.get(), Api);

	// Create the command list.
	m_commandList = CommandList::MakeUnique(Api);
	const auto pCommandList = m_commandList.get();
	XUSG_N_RETURN(pCommandList->Create(m_device.get(), 0, CommandListType::DIRECT,
		m_commandAllocators[m_frameIndex].get(), nullptr), ThrowIfFailed(E_FAIL));

	// Load scene asset
	vector<Resource::uptr> uploaders;
	{
		com_ptr<ID3DBlob> sceneFileBlob;
		D3DReadFileToBlob(m_sceneFile.c_str(), &sceneFileBlob);
		XUSG_N_RETURN(sceneFileBlob, ThrowIfFailed(E_FAIL));

		const string sceneString = static_cast<char*>(sceneFileBlob->GetBufferPointer());

		tiny::TinyJson sceneReader;
		sceneReader.ReadJson(sceneString);

		// Create scene
		m_scene = Scene::MakeUnique(Api);
		//m_scene->SetRenderTarget(m_rtHDR, m_depth);
		XUSG_N_RETURN(m_scene->LoadAssets(&sceneReader, pCommandList, m_shaderLib,
			m_graphicsPipelineLib, m_computePipelineLib, m_pipelineLayoutLib,
			m_descriptorTableLib, uploaders, FormatHDR, FormatDepth,
			Format::D24_UNORM_S8_UINT, m_useIBL), ThrowIfFailed(E_FAIL));
	}

	{
		m_postprocess = Postprocess::MakeUnique(Api);
		XUSG_N_RETURN(m_postprocess->Init(m_device.get(), m_shaderLib, m_graphicsPipelineLib,
			m_computePipelineLib, m_pipelineLayoutLib, m_descriptorTableLib,
			FormatHDR, FormatLDR), ThrowIfFailed(E_FAIL));
	}

	// Close the command list and execute it to begin the initial GPU setup.
	XUSG_N_RETURN(pCommandList->Close(), ThrowIfFailed(E_FAIL));
	m_commandQueue->ExecuteCommandList(pCommandList);

	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	{
		if (!m_fence)
		{
			m_fence = Fence::MakeUnique(Api);
			XUSG_N_RETURN(m_fence->Create(m_device.get(), m_fenceValues[m_frameIndex]++, FenceFlag::NONE, L"Fence"), ThrowIfFailed(E_FAIL));
		}

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

	// Create window size dependent resources.
	CreateResources();
	ResizeAssets();

	// Projection
	{
		const auto aspectRatio = m_width / static_cast<float>(m_height);
		const auto proj = XMMatrixPerspectiveFovLH(XUSG_FOVAngleY, aspectRatio, XUSG_zNear, XUSG_zFar);
		XMStoreFloat4x4(&m_proj, proj);
	}

	// Setup the camera's view parameters
	{
		const auto focusDist = m_scene->GetFocusAndDistance();
		const auto viewDist = XMVectorGetW(focusDist);
		const auto viewDisp = XMVectorSet(0.0f, 0.0f, viewDist, 0.0f);
		const auto eyePt = focusDist - viewDisp;
		const auto view = XMMatrixLookAtLH(eyePt, focusDist, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
		XMStoreFloat3(&m_eyePt, eyePt);
		XMStoreFloat4x4(&m_view, view);
	}
}

void RenderingX::CreateSwapchain()
{
	// Describe and create the swap chain.
	m_swapChain = SwapChain::MakeUnique(Api);
	XUSG_N_RETURN(m_swapChain->Create(m_factory.get(), Win32Application::GetHwnd(), m_commandQueue->GetHandle(),
		FrameCount, m_width, m_height, FormatLDR, SwapChainFlag::ALLOW_TEARING), ThrowIfFailed(E_FAIL));

	// This class does not support exclusive full-screen mode and prevents DXGI from responding to the ALT+ENTER shortcut.
	ThrowIfFailed(m_factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));
}

void RenderingX::CreateResources()
{
	// Obtain the back buffers for this window which will be the final render targets
	// and create render target views for each of them.
	for (uint8_t n = 0; n < FrameCount; ++n)
	{
		m_renderTargets[n] = RenderTarget::MakeUnique(Api);
		XUSG_N_RETURN(m_renderTargets[n]->CreateFromSwapChain(m_device.get(), m_swapChain.get(), n), ThrowIfFailed(E_FAIL));
	}

	// Create TAA RTs
	for (uint8_t n = 0; n < 2; ++n)
	{
		m_temporalColors[n] = RenderTarget::MakeUnique(Api);
		XUSG_N_RETURN(m_temporalColors[n]->Create(m_device.get(), m_width, m_height, FormatHDR, 1, ResourceFlag::NONE,
			1, 1, nullptr, false, MemoryFlag::NONE, (L"TemporalColor" + to_wstring(n)).c_str()), ThrowIfFailed(E_FAIL));

		m_metaBuffers[n] = RenderTarget::MakeUnique(Api);
		XUSG_N_RETURN(m_metaBuffers[n]->Create(m_device.get(), m_width, m_height, Format::R8_UNORM, 1, ResourceFlag::NONE,
			1, 1, nullptr, false, MemoryFlag::NONE, (L"MetadataBuffer" + to_wstring(n)).c_str()), ThrowIfFailed(E_FAIL));
	}

	// Create HDR RT
	m_sceneColor = RenderTarget::MakeShared(Api);
	XUSG_N_RETURN(m_sceneColor->Create(m_device.get(), m_width, m_height, FormatHDR, 1, ResourceFlag::NONE,
		1, 1, nullptr, false, MemoryFlag::NONE, L"SceneColor"), ThrowIfFailed(E_FAIL));

	// Create shade-amount RT
	m_sceneShade = RenderTarget::MakeShared(Api);
	XUSG_N_RETURN(m_sceneShade->Create(m_device.get(), m_width, m_height, Format::R8_UNORM, 1, ResourceFlag::NONE,
		1, 1, nullptr, false, MemoryFlag::NONE, L"SceneShadeAmount"), ThrowIfFailed(E_FAIL));

	// Create a DSV
	m_sceneDepth = DepthStencil::MakeShared(Api);
	XUSG_N_RETURN(m_sceneDepth->Create(m_device.get(), m_width, m_height, Format::UNKNOWN, ResourceFlag::NONE,
		1, 1, 1, 1.0f, 0, false, MemoryFlag::NONE, L"SceneDepth"), ThrowIfFailed(E_FAIL));

	// Set the 3D rendering viewport and scissor rectangle to target the entire window.
	m_viewport = Viewport(0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height));
	m_scissorRect = RectRange(0, 0, m_width, m_height);
}

void RenderingX::ResizeAssets()
{
	const auto pCommandAllocator = m_commandAllocators[m_frameIndex].get();
	XUSG_N_RETURN(pCommandAllocator->Reset(), ThrowIfFailed(E_FAIL));

	const auto pCommandList = m_commandList.get();
	XUSG_N_RETURN(pCommandList->Reset(pCommandAllocator, nullptr), ThrowIfFailed(E_FAIL));

	// Scene
	vector<Resource::uptr> uploaders;
	XUSG_N_RETURN(m_scene->ChangeWindowSize(pCommandList, uploaders,
		m_sceneColor, m_sceneDepth, m_sceneShade), ThrowIfFailed(E_FAIL));

	// Post process
	{
		XUSG_N_RETURN(m_postprocess->ChangeWindowSize(m_device.get(), m_sceneColor.get()), ThrowIfFailed(E_FAIL));

		// Create Descriptor tables
		for (uint8_t n = 0; n < 2; ++n)
		{
			XUSG_X_RETURN(m_srvTables[SRV_AA_INPUT + n], m_postprocess->CreateTAASrvTable(
				m_sceneColor->GetSRV(), m_temporalColors[!n]->GetSRV(), m_scene->GetGBuffer(Scene::MOTION_IDX)->GetSRV(),
				m_sceneShade->GetSRV(), m_metaBuffers[!n]->GetSRV()), ThrowIfFailed(E_FAIL));

			const auto srvTable = Util::DescriptorTable::MakeUnique(Api);
			srvTable->SetDescriptors(0, 1, &m_temporalColors[n]->GetSRV());
			XUSG_X_RETURN(m_srvTables[SRV_HDR_IMAGE + n], srvTable->GetCbvSrvUavTable(m_descriptorTableLib.get()), ThrowIfFailed(E_FAIL));
		}
	}

	// Close the command list and execute it to begin the initial GPU setup.
	XUSG_N_RETURN(pCommandList->Close(), ThrowIfFailed(E_FAIL));
	m_commandQueue->ExecuteCommandList(pCommandList);

	// Create synchronization objects and wait until assets have been uploaded to the GPU.
	{
		if (!m_fence)
		{
			m_fence = Fence::MakeUnique(Api);
			XUSG_N_RETURN(m_fence->Create(m_device.get(), m_fenceValues[m_frameIndex]++, FenceFlag::NONE, L"Fence"), ThrowIfFailed(E_FAIL));
		}

		// Create an event handle to use for frame synchronization.
		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (!m_fenceEvent) ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));

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
	m_commandQueue->ExecuteCommandList(m_commandList.get());

	// Present the frame.
	XUSG_N_RETURN(m_swapChain->Present(0, PresentFlag::ALLOW_TEARING), ThrowIfFailed(E_FAIL));

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
	for (uint8_t n = 0; n < FrameCount; ++n)
	{
		m_renderTargets[n].reset();
		m_fenceValues[n] = m_fenceValues[m_frameIndex];
	}
	m_descriptorTableLib->ResetDescriptorHeap(CBV_SRV_UAV_HEAP);
	m_descriptorTableLib->ResetDescriptorHeap(RTV_HEAP);

	// Determine the render target size in pixels.
	m_width = (max)(width, 1);
	m_height = (max)(height, 1);

	// If the swap chain already exists, resize it, otherwise create one.
	if (m_swapChain)
	{
		// If the swap chain already exists, resize it.
		const auto hr = m_swapChain->ResizeBuffers(FrameCount, m_width,
			m_height, FormatLDR, SwapChainFlag::ALLOW_TEARING);

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
		const auto proj = XMMatrixPerspectiveFovLH(XUSG_FOVAngleY, aspectRatio, XUSG_zNear, XUSG_zFar);
		XMStoreFloat4x4(&m_proj, proj);
	}
}

// User hot-key interactions.
void RenderingX::OnKeyUp(uint8_t key)
{
	switch (key)
	{
	case VK_SPACE:
		m_isPaused = !m_isPaused;
		break;
	case VK_F1:
		m_showFPS = !m_showFPS;
		break;
	case VK_F11:
		m_screenShot = 1;
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

	for (auto i = 1; i < argc; ++i)
	{
		if (wcsncmp(argv[i], L"-warp", wcslen(argv[i])) == 0 ||
			wcsncmp(argv[i], L"/warp", wcslen(argv[i])) == 0)
			m_useWarpDevice = true;
		else if ((wcsncmp(argv[i], L"-scene", wcslen(argv[i])) == 0 ||
			wcsncmp(argv[i], L"/scene", wcslen(argv[i])) == 0) && i + 1 < argc)
			m_sceneFile = argv[++i];
		else if ((wcsncmp(argv[i], L"-width", wcslen(argv[i])) == 0 ||
			wcsncmp(argv[i], L"/width", wcslen(argv[i])) == 0 ||
			wcsncmp(argv[i], L"-w", wcslen(argv[i])) == 0 ||
			wcsncmp(argv[i], L"/w", wcslen(argv[i])) == 0) && i + 1 < argc)
			m_width = stoul(argv[++i]);
		else if ((wcsncmp(argv[i], L"-height", wcslen(argv[i])) == 0 ||
			wcsncmp(argv[i], L"/height", wcslen(argv[i])) == 0 ||
			wcsncmp(argv[i], L"-h", wcslen(argv[i])) == 0 ||
			wcsncmp(argv[i], L"/h", wcslen(argv[i])) == 0) && i + 1 < argc)
			m_height = stoul(argv[++i]);
		else if (wcsncmp(argv[i], L"-noIBL", wcslen(argv[i])) == 0 ||
			wcsncmp(argv[i], L"/noIBL", wcslen(argv[i])) == 0)
			m_useIBL = false;
	}
}

void RenderingX::PopulateCommandList()
{
	// Command list allocators can only be reset when the associated 
	// command lists have finished execution on the GPU; apps should use 
	// fences to determine GPU execution progress.
	const auto pCommandAllocator = m_commandAllocators[m_frameIndex].get();
	XUSG_N_RETURN(pCommandAllocator->Reset(), ThrowIfFailed(E_FAIL));

	// However, when ExecuteCommandList() is called on a particular command 
	// list, that command list can then be reset at any time and must be before 
	// re-recording.
	const auto pCommandList = m_commandList.get();
	XUSG_N_RETURN(pCommandList->Reset(pCommandAllocator, nullptr), ThrowIfFailed(E_FAIL));

	// Record commands.
	// Set descriptor heap
	const auto descriptorHeap = m_descriptorTableLib->GetDescriptorHeap(CBV_SRV_UAV_HEAP);
	pCommandList->SetDescriptorHeaps(1, &descriptorHeap);

	// Render scene
	//const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	const auto pRenderTarget = m_renderTargets[m_frameIndex].get();
	//pCommandList->ClearRenderTargetView(pRenderTarget->GetRTV(), clearColor);
	m_scene->Render(pCommandList);

	// Temporal AA
	RenderTarget* ppDsts[] = { m_temporalColors[m_frameParity].get(), m_metaBuffers[m_frameParity].get() };
	Texture* ppSrcs[] = { m_sceneColor.get(), m_sceneShade.get(), m_metaBuffers[!m_frameParity].get() };
	m_postprocess->Antialias(pCommandList, ppDsts, ppSrcs, m_srvTables[SRV_AA_INPUT + m_frameParity],
		static_cast<uint8_t>(size(ppDsts)), static_cast<uint8_t>(size(ppSrcs)));

	// Postprocessing
	m_postprocess->Render(pCommandList, pRenderTarget, m_temporalColors[m_frameParity].get(),
		m_srvTables[SRV_HDR_IMAGE + m_frameParity]);
	m_frameParity = !m_frameParity;

	// Indicate that the back buffer will now be used to present.
	ResourceBarrier barrier;
	const auto numBarriers = pRenderTarget->SetBarrier(&barrier, ResourceState::PRESENT);
	pCommandList->Barrier(numBarriers, &barrier);

	// Screen-shot helper
	if (m_screenShot == 1)
	{
		if (!m_readBuffer) m_readBuffer = Buffer::MakeUnique();
		pRenderTarget->ReadBack(pCommandList, m_readBuffer.get(), &m_rowPitch);
		m_screenShot = 2;
	}

	XUSG_N_RETURN(pCommandList->Close(), ThrowIfFailed(E_FAIL));
}

// Wait for pending GPU work to complete.
void RenderingX::WaitForGpu()
{
	// Schedule a Signal command in the queue.
	XUSG_N_RETURN(m_commandQueue->Signal(m_fence.get(), m_fenceValues[m_frameIndex]), ThrowIfFailed(E_FAIL));

	// Wait until the fence has been processed, and increment the fence value for the current frame.
	XUSG_N_RETURN(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex]++, m_fenceEvent), ThrowIfFailed(E_FAIL));
	WaitForSingleObject(m_fenceEvent, INFINITE);
}

// Prepare to render the next frame.
void RenderingX::MoveToNextFrame()
{
	// Schedule a Signal command in the queue.
	const auto currentFenceValue = m_fenceValues[m_frameIndex];
	XUSG_N_RETURN(m_commandQueue->Signal(m_fence.get(), currentFenceValue), ThrowIfFailed(E_FAIL));

	// Update the frame index.
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// If the next frame is not ready to be rendered yet, wait until it is ready.
	if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
	{
		XUSG_N_RETURN(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent), ThrowIfFailed(E_FAIL));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	// Set the fence value for the next frame.
	m_fenceValues[m_frameIndex] = currentFenceValue + 1;

	// Screen-shot helper
	if (m_screenShot)
	{
		if (m_screenShot > FrameCount)
		{
			char timeStr[15];
			tm dateTime;
			const auto now = time(nullptr);
			if (!localtime_s(&dateTime, &now) && strftime(timeStr, sizeof(timeStr), "%Y%m%d%H%M%S", &dateTime))
				SaveImage((string("RenderingX_") + timeStr + ".png").c_str(), m_readBuffer.get(), m_width, m_height, m_rowPitch);
			m_screenShot = 0;
		}
		else ++m_screenShot;
	}
}

void RenderingX::SaveImage(char const* fileName, Buffer* pImageBuffer, uint32_t w, uint32_t h, uint32_t rowPitch, uint8_t comp)
{
	assert(comp == 3 || comp == 4);
	const auto pData = static_cast<const uint8_t*>(pImageBuffer->Map(nullptr));

	//stbi_write_png_compression_level = 1024;
	vector<uint8_t> imageData(comp * w * h);
	const auto sw = rowPitch / 4; // Byte to pixel
	for (auto i = 0u; i < h; ++i)
		for (auto j = 0u; j < w; ++j)
		{
			const auto s = sw * i + j;
			const auto d = w * i + j;
			for (uint8_t k = 0; k < comp; ++k)
				imageData[comp * d + k] = pData[4 * s + k];
		}

	stbi_write_png(fileName, w, h, comp, imageData.data(), 0);

	pImageBuffer->Unmap();
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
		windowText << L"    [F1] ";
		if (m_showFPS) windowText << setprecision(2) << fixed << L"fps: " << fps;
		else windowText << L"show fps";

		windowText << L"    [F11] screen shot";
		
		SetCustomWindowText(windowText.str().c_str());
	}

	if (pTimeStep)* pTimeStep = static_cast<float>(totalTime - previousTime);
	previousTime = totalTime;

	return totalTime;
}
