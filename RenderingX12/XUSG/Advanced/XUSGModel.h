//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "Core/XUSG.h"
#include "XUSGShaderCommon.h"
#include "XUSGSDKMesh.h"
#include "XUSGSharedConst.h"

#define MAX_SHADOW_CASCADES	8

namespace XUSG
{
	class DLL_EXPORT Model
	{
	public:
		enum PipelineLayoutIndex : uint8_t
		{
			BASE_PASS,
			DEPTH_PASS,
			DEPTH_ALPHA_PASS,
			GLOBAL_BASE_PASS,
			GLOBAL_DEPTH_PASS,
			GLOBAL_DEPTH_ALPHA_PASS,

			NUM_PIPELINE_LAYOUT
		};

		enum PipelineIndex : uint8_t
		{
			OPAQUE_FRONT,
			OPAQUE_FRONT_EQUAL,
			OPAQUE_TWO_SIDED,
			OPAQUE_TWO_SIDED_EQUAL,
			ALPHA_TWO_SIDED,
			DEPTH_FRONT,
			DEPTH_TWO_SIDED,
			SHADOW_FRONT,
			SHADOW_TWO_SIDED,
			REFLECTED,

			NUM_PIPELINE
		};

		enum DescriptorTableSlot : uint8_t
		{
			MATRICES,
#if TEMPORAL_AA
			TEMPORAL_BIAS,
#endif
			VARIABLE_SLOT
		};

		enum DescriptorTableSlotOffset : uint8_t
		{
			SAMPLERS_OFFSET,
			MATERIAL_OFFSET,
			SHADOW_MAP_OFFSET,
			ALPHA_REF_OFFSET = SHADOW_MAP_OFFSET,
			IMMUTABLE_OFFSET
		};

		enum CBVTableIndex : uint8_t
		{
			CBV_MATRICES,
			CBV_SHADOW_MATRIX,

			NUM_CBV_TABLE = CBV_SHADOW_MATRIX + MAX_SHADOW_CASCADES
		};

		Model(const Device& device, const wchar_t* name);
		virtual ~Model();

		bool Init(const InputLayout& inputLayout, const std::shared_ptr<SDKMesh>& mesh,
			const ShaderPool::sptr& shaderPool,
			const Graphics::PipelineCache::sptr& pipelineCache,
			const PipelineLayoutCache::sptr& pipelineLayoutCache,
			const DescriptorTableCache::sptr& descriptorTableCache);
		void Update(uint8_t frameIndex);
		void SetMatrices(DirectX::CXMMATRIX viewProj, DirectX::CXMMATRIX world, DirectX::FXMMATRIX* pShadowView = nullptr,
			DirectX::FXMMATRIX* pShadows = nullptr, uint8_t numShadows = 0, bool isTemporal = true);
		void SetPipelineLayout(const CommandList* pCommandList, PipelineLayoutIndex layout);
		void SetPipeline(const CommandList* pCommandList, PipelineIndex pipeline);
		void SetPipeline(const CommandList* pCommandList, SubsetFlags subsetFlags, PipelineLayoutIndex layout);
		void Render(const CommandList* pCommandList, SubsetFlags subsetFlags, uint8_t matrixTableIndex,
			PipelineLayoutIndex layout = NUM_PIPELINE_LAYOUT, uint32_t numInstances = 1);

		static InputLayout CreateInputLayout(Graphics::PipelineCache& pipelineCache);
		static std::shared_ptr<SDKMesh> LoadSDKMesh(const Device& device, const std::wstring& meshFileName,
			const TextureCache& textureCache, bool isStaticMesh);

		static constexpr uint32_t GetFrameCount() { return FrameCount; }

	protected:
		struct CBMatrices
		{
			DirectX::XMFLOAT4X4 WorldViewProj;
			DirectX::XMFLOAT3X4 World;
			DirectX::XMFLOAT3X4 WorldIT;
			DirectX::XMFLOAT4X4 Shadow;
#if TEMPORAL
			DirectX::XMFLOAT4X4 WorldViewProjPrev;
#endif
		};

		bool createConstantBuffers();
		bool createPipelines(bool isStatic, const InputLayout& inputLayout, const Format* rtvFormats,
			uint32_t numRTVs, Format dsvFormat, Format shadowFormat);
		bool createDescriptorTables();
		void render(const CommandList* pCommandList, uint32_t mesh, PipelineLayoutIndex layout,
			SubsetFlags subsetFlags, uint32_t numInstances);

		Util::PipelineLayout::sptr initPipelineLayout(VertexShader vs, PixelShader ps);

		static const uint32_t FrameCount = FRAME_COUNT;

		Device		m_device;
		std::wstring m_name;

		uint8_t		m_currentFrame;
		uint8_t		m_previousFrame;

		uint8_t		m_variableSlot;

		std::shared_ptr<SDKMesh>		m_mesh;
		ShaderPool::sptr				m_shaderPool;
		Graphics::PipelineCache::sptr	m_pipelineCache;
		PipelineLayoutCache::sptr		m_pipelineLayoutCache;
		DescriptorTableCache::sptr		m_descriptorTableCache;

#if TEMPORAL
		DirectX::XMFLOAT4X4		m_worldViewProjs[FrameCount];
#endif

		ConstantBuffer::uptr	m_cbMatrices;
		ConstantBuffer::uptr	m_cbShadowMatrices;

		PipelineLayout			m_pipelineLayouts[NUM_PIPELINE_LAYOUT];
		Pipeline				m_pipelines[NUM_PIPELINE];
		DescriptorTable			m_cbvTables[FrameCount][NUM_CBV_TABLE];
		DescriptorTable			m_samplerTable;
		std::vector<DescriptorTable> m_srvTables;
	};
}
