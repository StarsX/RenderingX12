//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "Core/XUSGPipelineLayout.h"
#include "Core/XUSGGraphicsState.h"
#include "Core/XUSGComputeState.h"
#include "Core/XUSGResource.h"
#include "Core/XUSGDescriptor.h"
#include "XUSGSharedConst.h"

namespace XUSG
{
	class Postprocess
	{
	public:
		enum PipelineIndex : uint8_t
		{
			RESAMPLE_LUM,
			POST_EFFECTS,
			LUM_ADAPT,
			TONE_MAP,
			ANTIALIAS,
			UNSHARP,

			NUM_PIPELINE
		};

		enum DescriptorPoolIndex : uint8_t
		{
			IMMUTABLE_POOL,
			RESIZABLE_POOL
		};

		Postprocess(const Device& device);
		virtual ~Postprocess();

		bool Init(const std::shared_ptr<ShaderPool>& shaderPool,
			const std::shared_ptr<Graphics::PipelineCache>& graphicsPipelineCache,
			const std::shared_ptr<Compute::PipelineCache>& computePipelineCache,
			const std::shared_ptr<PipelineLayoutCache>& pipelineLayoutCache,
			const std::shared_ptr<DescriptorTableCache>& descriptorTableCache,
			Format hdrFormat, Format ldrFormat);
		bool ChangeWindowSize(const RenderTarget& refRT, const Descriptor& srvBaseColor);

		void Update(const DescriptorTable& cbvImmutable, const DescriptorTable& cbvPerFrameTable,
			float timeStep);
		void Render(const CommandList& commandList, RenderTarget& dst, Texture2D& src,
			const Descriptor& rtv, const DescriptorTable& srvTable, bool clearRT = false);
		void ScreenRender(const CommandList& commandList, PipelineIndex pipelineIndex,
			const DescriptorTable& srvTable, bool hasPerFrameCB, bool hasSampler, bool reset = false);
		void LumAdaption(const CommandList& commandList, const DescriptorTable& uavSrvTable, bool reset = false);
		void Antialias(const CommandList& commandList, const Descriptor* pRTVs, const DescriptorTable& srvTable,
			uint8_t numRTVs = 1, bool reset = false);
		void Unsharp(const CommandList& commandList, const Descriptor* pRTVs, const DescriptorTable& srvTable,
			uint8_t numRTVs = 1, bool reset = false);

		DescriptorTable CreateTemporalAASRVTable(const Descriptor& srvCurrent, const Descriptor& srvPrevious,
			const Descriptor& srvVelocity, const Descriptor& pSRVMatEnc);

	protected:
		enum DescriptorTableSlot : uint8_t
		{
			IMMUTABLE,
			TEXTURES,
			SAMPLERS,
			PER_FRAME,
			TIME_STEP = IMMUTABLE
		};

		enum ResampleDescTableSlot : uint8_t
		{
			RESAMPLE_TEXTURE,
			RESAMPLE_SAMPLER
		};

		static const uint32_t FrameCount = FRAME_COUNT;

		enum CBVTableIndex : uint8_t
		{
			CBV_IMMUTABLE,
			CBV_PER_FRAME,
			CBV_TIME_STEP,

			NUM_CBV_TABLE = CBV_TIME_STEP + FrameCount
		};

		enum UavSrvTableIndex : uint8_t
		{
			SRV_COLOR_AVG_LUM,
			SRV_TAA_INPUTS,
			SRV_LOG_LUM,
			UAV_SRV_LUM = SRV_LOG_LUM
		};

		bool createGBuffers(const RenderTarget& refRT, const Descriptor& srvBaseColor);
		bool createPipelineLayouts();
		bool createPipelines(Format hdrFormat, Format ldrFormat);

		Device		m_device;
		uint8_t		m_frameIndex;
		float		m_timeStep;

		std::shared_ptr<ShaderPool>					m_shaderPool;
		std::shared_ptr<Graphics::PipelineCache>	m_graphicsPipelineCache;
		std::shared_ptr<Compute::PipelineCache>		m_computePipelineCache;
		std::shared_ptr<PipelineLayoutCache>		m_pipelineLayoutCache;
		std::shared_ptr<DescriptorTableCache>		m_descriptorTableCache;

		Viewport			m_viewport;
		RectRange			m_scissorRect;

		RenderTarget		m_postImage;
		RenderTarget		m_logLum;

		RawBuffer			m_avgLum;

		PipelineLayout		m_pipelineLayouts[NUM_PIPELINE];
		Pipeline			m_pipelines[NUM_PIPELINE];

		ConstantBuffer		m_cbTimeStep;

		Framebuffer			m_framebuffer;
		DescriptorTable		m_cbvTables[NUM_CBV_TABLE];
		std::vector<DescriptorTable> m_uavSrvTables;
		DescriptorTable		m_samplerTable;
	};
}
