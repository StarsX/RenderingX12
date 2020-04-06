//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "Core/XUSG.h"
#include "XUSGSharedConst.h"

namespace XUSG
{
	class DLL_EXPORT Nature
	{
	public:
		Nature(const Device& device);
		virtual ~Nature();

		bool Init(CommandList* pCommandList, const ShaderPool::sptr& shaderPool,
			const Graphics::PipelineCache::sptr& graphicsPipelineCache,
			const PipelineLayoutCache::sptr& pipelineLayoutCache,
			const DescriptorTableCache::sptr& descriptorTableCache,
			const std::wstring& skyTexture, std::vector<Resource>& uploaders,
			bool renderWater, Format rtvFormat = Format::R11G11B10_FLOAT,
			Format dsvFormat = Format::D24_UNORM_S8_UINT);
		bool CreateResources(ResourceBase* pSceneColor, const DepthStencil& depth);

		void Update(uint8_t frameIndex, DirectX::FXMMATRIX* pViewProj, DirectX::FXMMATRIX* pWorld = nullptr);
		void SetGlobalCBVTables(DescriptorTable cbvImmutable, DescriptorTable cbvPerFrameTable);
		void RenderSky(const CommandList* pCommandList, bool reset = false);
		void RenderWater(const CommandList* pCommandList, const Framebuffer& framebuffer,
			uint32_t& numBarriers, ResourceBarrier* pBarriers, bool reset = false);

		Descriptor GetSkySRV() const;

	protected:
		enum DescriptorPoolIndex : uint8_t
		{
			IMMUTABLE_POOL,
			RESIZABLE_POOL
		};

		enum DescriptorTableSlot : uint8_t
		{
			IMMUTABLE,
			PER_FRAME,
			TEXTURES,
			SAMPLERS,
			MATRICES
		};

		enum ResampleDescTableSlot : uint8_t
		{
			RESAMPLE_TEXTURE,
			RESAMPLE_SAMPLER
		};

		enum PipelineLayoutIndex : uint8_t
		{
			RESAMPLE_PASS,
			SKYDOME_PASS,
			REFLECT_PASS,
			WATER_PASS,

			NUM_PIPE_LAYOUT
		};

		enum PipelineIndex : uint8_t
		{
			RESAMPLE_COLOR,
			RESAMPLE_DEPTH,
			SKYDOME,
			SS_REFLECT,
			WATER,

			NUM_PIPELINE
		};

		static const uint8_t MaxReflectionMipsCount = 6;
		static const uint8_t MaxDepthMipsCount = 6;
		enum SRVTableIndex : uint8_t
		{
			SRV_SKYDOME,
			SRV_WATER,
			SRV_REFLECT,
			SRV_DEPTH = SRV_REFLECT + MaxReflectionMipsCount,

			NUM_SRV_TABLE = SRV_DEPTH + MaxDepthMipsCount
		};

		static const uint32_t FrameCount = FRAME_COUNT;

		enum CBVTableIndex : uint8_t
		{
			CBV_IMMUTABLE,
			CBV_PER_FRAME,
			CBV_MATRICES,

			NUM_CBV_TABLE = CBV_MATRICES + FrameCount
		};

		bool createGBuffers(ResourceBase* pSceneColor, const DepthStencil& depth);
		bool createConstantBuffer();
		bool loadTextures(CommandList* pCommandList, const std::wstring& skyTexture, std::vector<Resource>& uploaders);
		bool createPipelineLayouts(bool renderWater);
		bool createPipelines(bool renderWater, Format rtvFormat, Format dsvFormat);
		void setWaterResources(const CommandList* pCommandList, uint32_t& numBarriers, ResourceBarrier* pBarriers);
		void renderReflection(const CommandList* pCommandList);
		void renderWater(const CommandList* pCommandList, const Framebuffer& framebuffer);

		Device		m_device;
		uint8_t		m_frameIndex;

		ShaderPool::sptr				m_shaderPool;
		Graphics::PipelineCache::sptr	m_graphicsPipelineCache;
		PipelineLayoutCache::sptr		m_pipelineLayoutCache;
		DescriptorTableCache::sptr		m_descriptorTableCache;

		ResourceBase::sptr m_skyTexture;
		ResourceBase*	m_pSceneColor;

		Viewport		m_viewport;
		RectRange		m_scissorRect;

		Texture2D::uptr	m_refraction;
		RenderTarget::uptr m_reflection;
		RenderTarget::uptr m_depth;

		ConstantBuffer::uptr m_cbMatrices;

		PipelineLayout	m_pipelineLayouts[NUM_PIPE_LAYOUT];
		Pipeline		m_pipelines[NUM_PIPELINE];

		Framebuffer		m_framebuffer;

		DescriptorTable	m_cbvTables[NUM_CBV_TABLE];
		DescriptorTable	m_srvTables[NUM_SRV_TABLE];
		DescriptorTable	m_samplerTable;
	};
}
