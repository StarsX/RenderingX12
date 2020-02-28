//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "XUSGModel.h"

namespace XUSG
{
	class StaticModel :
		public Model
	{
	public:
		enum DescriptorTableSlot : uint8_t
		{
			PER_OBJECT	= VARIABLE_SLOT,
			PER_FRAME,
			SAMPLERS,
			MATERIAL,
			SHADOW_MAP,
			IMMUATABLE,
			ALPHA_REF	= SHADOW_MAP
		};

		struct Vegetation
		{
			uint32_t			m_isVeg;
			float				m_heightBase;
			float				m_wavePhase;
			float				m_Padding;
			DirectX::XMFLOAT4	m_pos;
		};

		struct CBBoundBox
		{
			DirectX::XMVECTOR	m_vBBoxCenter;
			DirectX::XMVECTOR	m_vBBoxExt;
		};

		StaticModel(const Device& device, const wchar_t* name = nullptr);
		virtual ~StaticModel();

		bool Init(const InputLayout& inputLayout, const std::shared_ptr<SDKMesh>& mesh,
			const std::shared_ptr<ShaderPool>& shaderPool,
			const std::shared_ptr<Graphics::PipelineCache>& pipelineCache,
			const std::shared_ptr<PipelineLayoutCache>& pipelineLayoutCache,
			const std::shared_ptr<DescriptorTableCache>& descriptorTableCache,
			const Format* rtvFormats = nullptr, uint32_t numRTVs = 0,
			Format dsvFormat = Format::UNKNOWN, Format shadowFormat = Format::UNKNOWN);
		void CreateBoundCBuffer();
		void Update(uint8_t frameIndex);
		void Update(uint8_t frameIndex, DirectX::CXMMATRIX viewProj, DirectX::CXMMATRIX world,
			DirectX::FXMMATRIX* pShadowView = nullptr, DirectX::FXMMATRIX* pShadows = nullptr,
			uint8_t numShadows = 0, bool isTemporal = true);
		void Render(const CommandList& commandList, uint32_t mesh, PipelineLayoutIndex layout,
			SubsetFlags subsetFlags = SUBSET_FULL, uint8_t matrixTableIndex = CBV_MATRICES,
			uint32_t numInstances = 1);
		void RenderBoundary(uint32_t mesh, const DirectX::XMFLOAT4* pTBox);

		const std::shared_ptr<SDKMesh>& GetMesh() const;

		static std::shared_ptr<SDKMesh> LoadSDKMesh(const Device& device, const std::wstring& meshFileName,
			const TextureCache& textureCache);

	protected:
		bool createConstantBuffers();
		bool createPipelineLayouts();
		bool createPipelines(const InputLayout& inputLayout, const Format* rtvFormats,
			uint32_t numRTVs, Format dsvFormat, Format shadowFormat);
		bool createDescriptorTables();

		ConstantBuffer		m_cbVegetations;
		ConstantBuffer		m_cbBoundBox;

		std::vector<DescriptorTable> m_cbvPerObjectTables;
	};
}
