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
#include "Core/XUSG.h"
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
	RenderingX(uint32_t width, uint32_t height, std::wstring name);
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
	enum RenderQueueType
	{
		OPAQUE_QUEUE,
		ALPHA_QUEUE,

		NUM_RENDER_QUEUE
	};

	static const uint32_t FrameCount = XUSG::Model::GetFrameCount();

	enum SrvTableIndex : uint8_t
	{
		SRV_HDR,
		SRV_AA_INPUT,
		SRV_ANTIALIASED = SRV_AA_INPUT + 2,

		NUM_SRV = SRV_ANTIALIASED + 2
	};

	XUSG::com_ptr<IDXGIFactory4> m_factory;

	std::shared_ptr<XUSG::ShaderPool>				m_shaderPool;
	std::shared_ptr<XUSG::Graphics::PipelineCache>	m_graphicsPipelineCache;
	std::shared_ptr<XUSG::Compute::PipelineCache>	m_computePipelineCache;
	std::shared_ptr<XUSG::PipelineLayoutCache>		m_pipelineLayoutCache;
	std::shared_ptr<XUSG::DescriptorTableCache>		m_descriptorTableCache;

	// Pipeline objects.
	XUSG::Viewport			m_viewport;
	XUSG::RectRange			m_scissorRect;

	XUSG::SwapChain			m_swapChain;
	XUSG::CommandAllocator	m_commandAllocators[FrameCount];
	XUSG::CommandQueue		m_commandQueue;

	XUSG::Device			m_device;
	XUSG::RenderTarget		m_renderTargets[FrameCount];
	XUSG::CommandList		m_commandList;

	// App resources.
	std::unique_ptr<XUSG::Scene> m_scene;
	std::unique_ptr<XUSG::Postprocess> m_postprocess;
	XUSG::RenderTarget		m_rtHDR;
	XUSG::RenderTarget		m_rtLDR;
	XUSG::RenderTarget		m_rtTAAs[2];
	XUSG::DepthStencil		m_depth;
	XMFLOAT4X4	m_proj;
	XMFLOAT4X4	m_view;
	XMFLOAT3	m_eyePt;

	// Simple tone mapping
	XUSG::PipelineLayout m_pipelineLayout;
	XUSG::Pipeline m_pipeline;
	XUSG::DescriptorTable m_srvTables[NUM_SRV];

	// Synchronization objects.
	uint8_t		m_frameParity;
	uint32_t	m_frameIndex;
	HANDLE		m_fenceEvent;
	XUSG::Fence	m_fence;
	uint64_t	m_fenceValues[FrameCount];

	// Application state
	bool		m_useIBL;
	bool		m_isPaused;
	StepTimer	m_timer;

	// User camera interactions
	bool		m_isTracking;
	XMFLOAT2	m_mousePt;

	std::wstring m_sceneFile;

	void LoadPipeline();
	void LoadAssets();
	void CreateSwapchain();
	void CreateResources();
	void ResizeAssets();
	void PopulateCommandList();
	void WaitForGpu();
	void MoveToNextFrame();
	double CalculateFrameStats(float* fTimeStep = nullptr);

	static const XUSG::Format FormatHDR = XUSG::Format::R11G11B10_FLOAT;
	static const XUSG::Format FormatLDR = XUSG::Format::B8G8R8A8_UNORM;
	static const XUSG::Format FormatDepth = XUSG::Format::D24_UNORM_S8_UINT;
};
