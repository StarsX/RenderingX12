//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "XUSGCharacter.h"
#include "XUSGOctree.h"
#include "XUSGShadow.h"
#ifdef INCLUDE_XUSG_NATURE_H
#include "XUSGNature.h"
#endif

namespace XUSG
{
	class Scene
	{
	public:
		enum GBufferIndex : uint8_t
		{
			ALBEDO_IDX,
			NORMAL_IDX,
			MATENC_IDX,
#if TEMPORAL
			MOTION_IDX,
#endif
			AO_IDX,

			NUM_GBUFFER,
			NUM_GB_FIXED = MATENC_IDX + 1,
			NUM_GB_RTV = AO_IDX
		};

		struct MeshProp
		{
			std::shared_ptr<SDKMesh> mesh;
			std::wstring sourceFile;
		};

		using MeshLink = Character::MeshLink;
		struct SkinnedMeshProp : MeshProp
		{
			std::vector<SDKMesh> linkedMeshes;
			std::vector<MeshLink> mesheLinkage;
			std::wstring animFile;
		};

		Scene(const Device& device);
		virtual ~Scene();

		bool LoadAssets(void* pSceneReader, const CommandList& commandList,
			const std::shared_ptr<ShaderPool>& shaderPool,
			const std::shared_ptr<Graphics::PipelineCache>& graphicsPipelineCache,
			const std::shared_ptr<Compute::PipelineCache>& computePipelineCache,
			const std::shared_ptr<PipelineLayoutCache>& pipelineLayoutCache,
			const std::shared_ptr<DescriptorTableCache>& descriptorTableCache,
			std::vector<Resource>& uploaders, Format rtvFormat, Format dsvFormat,
			bool useIBL = false);
		virtual bool ChangeWindowSize(const CommandList& commandList, std::vector<Resource>& uploaders,
			RenderTarget& renderTarget, DepthStencil& depth);
		virtual bool CreateResources(const CommandList& commandList, std::vector<Resource>& uploaders);
		void Update(uint8_t frameIndex, double time, float timeStep);
		void Update(uint8_t frameIndex, double time, float timeStep,
			DirectX::CXMMATRIX view, DirectX::CXMMATRIX proj,
			DirectX::CXMVECTOR eyePt);
		void Render(const CommandList& commandList);
		void SetViewProjMatrix(DirectX::CXMMATRIX view, DirectX::CXMMATRIX proj);
		void SetEyePoint(DirectX::CXMVECTOR eyePt);
		void SetFocusAndDistance(DirectX::CXMVECTOR focus_dist);
		virtual void SetRenderTarget(RenderTarget& renderTarget, DepthStencil& depth);
		virtual void SetViewport(const Viewport& viewport, const RectRange& scissorRect);

		DirectX::FXMVECTOR GetFocusAndDistance() const;
		const DescriptorTable& GetCBVTable(uint8_t i) const;
		const RenderTarget& GetGBuffer(const uint8_t i) const;
		Descriptor GetGBufferSRV(const uint8_t i) const;

	protected:
		enum RenderSpace : uint8_t
		{
			VIEW_SPACE = Model::CBV_MATRICES,
			LIGHT_SPACE = Model::CBV_SHADOW_MATRIX,

			NUM_SPACE = Model::NUM_CBV_TABLE
		};

		enum RenderQueueType : uint8_t
		{
			OPAQUE_QUEUE,
			ALPHA_QUEUE,

			NUM_RENDER_QUEUE
		};

		enum PipelineLayoutIndex : uint8_t
		{
			DEFERRED_SHADE_PASS,
			AMBIENT_OCCLUSION_PASS,

			NUM_PIPE_LAYOUT
		};

		enum PipelineIndex : uint8_t
		{
			DEFERRED_SHADE_OPAQUE,
			DEFERRED_SHADE_ALPHA,
			AMBIENT_OCCLUSION,

			NUM_PIPELINE
		};

		enum DescriptorPoolIndex : uint8_t
		{
			IMMUTABLE_POOL,
			RESIZABLE_POOL
		};

		enum DescriptorTableSlot : uint8_t
		{
			SAMPLERS,
			GBUFFERS,
			IMMUTABLE,
			PER_FRAME
		};

		enum FramebufferIndex : uint8_t
		{
			FB_OUTPUT,
			FB_DEPTH,
			FB_G_BUFFER,
			FB_AO_BUFFER,

			NUM_FRAMEBUFFER
		};

		enum SRVTableIndex : uint8_t
		{
			SRV_G_BUFFER,
			SRV_DEPTH,

			NUM_SRV_TABLE
		};

		static const uint32_t FrameCount = FRAME_COUNT;

	public:
		enum CBVTableIndex : uint8_t
		{
			CBV_IMMUTABLE,
			CBV_PER_FRAME_VS,
			CBV_PER_FRAME_PS	= CBV_PER_FRAME_VS + FrameCount,	// Window size dependent
#if TEMPORAL_AA
			CBV_TEMPORAL_BIAS0	= CBV_PER_FRAME_PS + FrameCount,
			CBV_TEMPORAL_BIAS,

			NUM_CBV_TABLE		= CBV_TEMPORAL_BIAS + FrameCount
#else
			NUM_CBV_TABLE		= CBV_PER_FRAME_PS + FrameCount
#endif
		};

	protected:
		virtual void readGlobalParams(void* pSceneReader);
		virtual void readCharacters(void* pSceneReader, std::vector<SkinnedMeshProp>& configs,
			std::vector<uint32_t>& indices, std::vector<DirectX::XMFLOAT4>& posRots, std::vector<std::wstring>& names);
		virtual void readStaticModels(void* pSceneReader, std::vector<MeshProp>& configs,
			std::vector<uint32_t>& indice, std::vector<std::wstring>& names);
		virtual void readEnvironment(void* pSceneReader, std::wstring& skyTexture);

		virtual bool createShaders(bool useIBL);
		virtual bool createSkinnedMeshes(std::vector<SkinnedMeshProp>& configs, const TextureCache& textureCache);
		virtual bool createStaticMeshes(std::vector<MeshProp>& configs, const TextureCache& textureCache);
		virtual bool createCharacters(Format dsvFormat, const std::vector<uint32_t>& indices,
			const std::vector<SkinnedMeshProp>& configs, const std::vector<DirectX::XMFLOAT4>& posRotList,
			const std::vector<std::wstring>& names);
		virtual bool createStaticModels(Format dsvFormat, const std::vector<uint32_t>& indices,
			const std::vector<MeshProp>& configs, const std::vector<std::wstring>& names);
		virtual bool createGBuffers();
		virtual bool createImmutable(const CommandList& commandList, std::vector<Resource>& uploaders);
		virtual bool createConstantBuffers();
		virtual bool createShadowMap();
		virtual bool createPipelineLayouts();
		virtual bool createPipelines(Format rtvFormat);

		virtual void renderOpaque(const CommandList& commandList);
		virtual void renderAlpha(const CommandList& commandList);
		virtual void renderGBuffersOpaque(const CommandList& commandList, uint32_t& numBarriers, ResourceBarrier* pBarriers);
		virtual void renderGBuffersAlpha(const CommandList& commandList, uint32_t& numBarriers, ResourceBarrier* pBarriers);
		virtual void renderDepth(const CommandList& commandList, uint8_t space, uint8_t temporalBiasIdx);
		virtual void renderShadowMap(const CommandList& commandList);
		virtual void deferredShade(const CommandList& commandList, bool alpha, uint32_t& numBarriers, ResourceBarrier* pBarriers);
		virtual void ambientOcclusion(const CommandList& commandList);

		Device		m_device;
		uint8_t		m_frameIndex;

		std::shared_ptr<ShaderPool>					m_shaderPool;
		std::shared_ptr<Graphics::PipelineCache>	m_graphicsPipelineCache;
		std::shared_ptr<Compute::PipelineCache>		m_computePipelineCache;
		std::shared_ptr<PipelineLayoutCache>		m_pipelineLayoutCache;
		std::shared_ptr<DescriptorTableCache>		m_descriptorTableCache;

		InputLayout	m_inputLayout;
		Viewport	m_viewport;
		Viewport	m_viewportLowRes;
		RectRange	m_scissorRect;
		RectRange	m_scissorLowRes;

		Octree m_octrees[NUM_RENDER_QUEUE];
		std::unique_ptr<Shadow> m_shadow;

		std::vector<std::unique_ptr<Character>>		m_characters;
		std::vector<std::shared_ptr<StaticModel>>	m_staticModels;
		std::vector<DirectX::XMUINT2> m_meshIDQueues[NUM_SPACE][NUM_RENDER_QUEUE];

#ifdef INCLUDE_XUSG_NATURE_H
		std::unique_ptr<Nature> m_nature;
		bool m_renderWater;
#endif

		float m_mapSize;
		float m_shadowMapSize;
		float m_looseCoeff;
		float m_time;
		float m_timeStep;

		DirectX::XMFLOAT3	m_eyePt;
		DirectX::XMFLOAT4	m_focus_dist;
		DirectX::XMFLOAT4	m_lightPt;
		DirectX::XMFLOAT4	m_radiance;
		DirectX::XMFLOAT4	m_ambient;
#if TEMPORAL_AA
		DirectX::XMFLOAT2	m_projBias;
#endif
		DirectX::XMFLOAT4X4	m_viewProj;
		DirectX::XMFLOAT4X4	m_viewProjI;
		DirectX::XMFLOAT4X4	m_view;
		DirectX::XMFLOAT4X4	m_proj;

		std::vector<Format>	m_gBufferFormats;
		std::vector<RenderTarget> m_gBuffers;
		DescriptorTable		m_srvTables[NUM_SRV_TABLE];
		Framebuffer			m_framebuffers[NUM_FRAMEBUFFER];
		RenderTarget*		m_pRenderTarget;
		DepthStencil*		m_pDepth;

		PipelineLayout		m_pipelineLayouts[NUM_PIPE_LAYOUT];
		Pipeline			m_pipelines[NUM_PIPELINE];

		ConstantBuffer		m_cbImmutable;
		ConstantBuffer		m_cbGlobal;
#if TEMPORAL_AA
		ConstantBuffer		m_cbTemporalBias;
#endif
		DescriptorTable		m_cbvTables[NUM_CBV_TABLE];
		DescriptorTable		m_samplerTable;
	};
}
