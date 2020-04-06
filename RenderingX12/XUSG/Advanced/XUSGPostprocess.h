//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "Core/XUSG.h"
#include "XUSGSharedConst.h"

namespace XUSG
{
	class DLL_EXPORT Postprocess
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

		bool Init(const ShaderPool::sptr& shaderPool,
			const Graphics::PipelineCache::sptr& graphicsPipelineCache,
			const Compute::PipelineCache::sptr& computePipelineCache,
			const PipelineLayoutCache::sptr& pipelineLayoutCache,
			const DescriptorTableCache::sptr& descriptorTableCache,
			Format hdrFormat, Format ldrFormat);
		bool ChangeWindowSize(const RenderTarget& refRT, const Descriptor& srvBaseColor);

		void Update(const DescriptorTable& cbvImmutable, const DescriptorTable& cbvPerFrameTable,
			float timeStep);
		void Render(const CommandList* pCommandList, RenderTarget& dst, Texture2D& src,
			const Descriptor& rtv, const DescriptorTable& srvTable, bool clearRT = false);
		void ScreenRender(const CommandList* pCommandList, PipelineIndex pipelineIndex,
			const DescriptorTable& srvTable, bool hasPerFrameCB, bool hasSampler, bool reset = false);
		void LumAdaption(const CommandList* pCommandList, const DescriptorTable& uavSrvTable, bool reset = false);
		void Antialias(const CommandList* pCommandList, const Descriptor* pRTVs, const DescriptorTable& srvTable,
			uint8_t numRTVs = 1, bool reset = false);
		void Unsharp(const CommandList* pCommandList, const Descriptor* pRTVs, const DescriptorTable& srvTable,
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

		ShaderPool::sptr				m_shaderPool;
		Graphics::PipelineCache::sptr	m_graphicsPipelineCache;
		Compute::PipelineCache::sptr	m_computePipelineCache;
		PipelineLayoutCache::sptr		m_pipelineLayoutCache;
		DescriptorTableCache::sptr		m_descriptorTableCache;

		Viewport			m_viewport;
		RectRange			m_scissorRect;

		RenderTarget::uptr	m_postImage;
		RenderTarget::uptr	m_logLum;

		RawBuffer::uptr		m_avgLum;

		PipelineLayout		m_pipelineLayouts[NUM_PIPELINE];
		Pipeline			m_pipelines[NUM_PIPELINE];

		ConstantBuffer::uptr m_cbTimeStep;

		Framebuffer			m_framebuffer;
		DescriptorTable		m_cbvTables[NUM_CBV_TABLE];
		std::vector<DescriptorTable> m_uavSrvTables;
		DescriptorTable		m_samplerTable;
	};
}
