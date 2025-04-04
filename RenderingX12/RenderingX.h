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

#pragma once

#include "DXFramework.h"
#include "StepTimer.h"
#include "Advanced/XUSGAdvanced.h"

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().

class RenderingX : public DXFramework
{
public:
	RenderingX(uint32_t width, uint32_t height, const std::wstring& name);
	virtual ~RenderingX();

	virtual void OnInit();
	virtual void OnUpdate();
	virtual void OnRender();
	virtual void OnDestroy();

	virtual void OnWindowSizeChanged(int width, int height);

	virtual void OnKeyUp(uint8_t key);
	virtual void OnLButtonDown(float posX, float posY);
	virtual void OnLButtonUp(float posX, float posY);
	virtual void OnMouseMove(float posX, float posY);
	virtual void OnMouseWheel(float deltaZ, float posX, float posY);
	virtual void OnMouseLeave();

	virtual void ParseCommandLineArgs(wchar_t* argv[], int argc);

private:
	enum DeviceType : uint8_t
	{
		DEVICE_DISCRETE,
		DEVICE_UMA,
		DEVICE_WARP
	};

	enum SrvTableIndex : uint8_t
	{
		SRV_AA_INPUT,
		SRV_HDR_IMAGE = SRV_AA_INPUT + 2,

		NUM_SRV = SRV_HDR_IMAGE + 2
	};

	static const auto Api = XUSG::API::DIRECTX_12;
	static const auto FrameCount = XUSG::Model::GetFrameCount();

	XUSG::com_ptr<IDXGIFactory5> m_factory;

	XUSG::ShaderLib::sptr				m_shaderLib;
	XUSG::Graphics::PipelineLib::sptr	m_graphicsPipelineLib;
	XUSG::Compute::PipelineLib::sptr	m_computePipelineLib;
	XUSG::PipelineLayoutLib::sptr		m_pipelineLayoutLib;
	XUSG::DescriptorTableLib::sptr		m_descriptorTableLib;

	// Pipeline objects.
	XUSG::Viewport	m_viewport;
	XUSG::RectRange	m_scissorRect;

	XUSG::SwapChain::uptr			m_swapChain;
	XUSG::CommandAllocator::uptr	m_commandAllocators[FrameCount];
	XUSG::CommandQueue::uptr		m_commandQueue;

	XUSG::Device::uptr			m_device;
	XUSG::RenderTarget::uptr	m_renderTargets[FrameCount];
	XUSG::CommandList::uptr		m_commandList;

	// App resources.
	XUSG::Scene::uptr			m_scene;
	XUSG::Postprocess::uptr		m_postprocess;
	XUSG::RenderTarget::uptr	m_temporalColors[2];
	XUSG::RenderTarget::uptr	m_metaBuffers[2];
	XUSG::RenderTarget::sptr	m_sceneColor;
	XUSG::RenderTarget::sptr	m_sceneShade;
	XUSG::DepthStencil::sptr	m_sceneDepth;
	XUSG::DescriptorTable		m_srvTables[NUM_SRV];
	XMFLOAT4X4	m_proj;
	XMFLOAT4X4	m_view;
	XMFLOAT3	m_eyePt;

	// Synchronization objects.
	uint8_t				m_frameParity;
	uint8_t				m_frameIndex;
	HANDLE				m_fenceEvent;
	XUSG::Fence::uptr	m_fence;
	uint64_t			m_fenceValues[FrameCount];

	// Application state
	DeviceType	m_deviceType;
	StepTimer	m_timer;
	bool		m_useIBL;
	bool		m_showFPS;
	bool		m_isPaused;

	// User camera interactions
	bool		m_isTracking;
	XMFLOAT2	m_mousePt;

	// User external settings
	std::wstring m_sceneFile;

	// Screen-shot helpers and state
	XUSG::Buffer::uptr	m_readBuffer;
	uint32_t			m_rowPitch;
	uint8_t				m_screenShot;

	void LoadPipeline();
	void LoadAssets();
	void CreateSwapchain();
	void CreateResources();
	void ResizeAssets();
	void PopulateCommandList();
	void WaitForGpu();
	void MoveToNextFrame();
	void SaveImage(char const* fileName, XUSG::Buffer* pImageBuffer,
		uint32_t w, uint32_t h, uint32_t rowPitch, uint8_t comp = 3);
	double CalculateFrameStats(float* fTimeStep = nullptr);

	static const XUSG::Format FormatHDR = XUSG::Format::R11G11B10_FLOAT;
	static const XUSG::Format FormatLDR = XUSG::Format::R8G8B8A8_UNORM;
	static const XUSG::Format FormatDepth = XUSG::Format::D24_UNORM_S8_UINT;
};
