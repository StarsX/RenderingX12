//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#pragma once

#include "Core/XUSGResource.h"
#include "Core/XUSGDescriptor.h"
#include "XUSGSharedConst.h"

#define MAX_CASCADES	8

namespace XUSG
{
	class Shadow
	{
	public:
		struct CBShadowData
		{
			DirectX::XMVECTOR	m_cascadeOffset[MAX_CASCADES];
			DirectX::XMFLOAT4	m_cascadeScale[MAX_CASCADES];

			// For Map based selection scheme, this keeps the pixels inside of the the valid range.
			// When there is no boarder, these values are 0 and 1 respectivley.
			DirectX::XMFLOAT2	m_borderPadding;
			float				m_partitionSize;
			float				m_casBlendArea;	// Amount to overlap when blending between cascades.
		};

		Shadow(const Device& device);
		virtual ~Shadow();

		// This runs when the application is initialized.
		bool Init(float sceneMapSize, float shadowMapSize,
			const std::shared_ptr<DescriptorTableCache>& descriptorTableCache,
			uint8_t numCasLevels = NUM_CASCADE);
		// This runs per frame. This data could be cached when the cameras do not move.
		void Update(uint8_t frameIndex, const DirectX::XMFLOAT4X4& view,
			const DirectX::XMFLOAT4X4& proj, const DirectX::XMFLOAT4& lightPt);

		void SetViewport(const CommandList& commandList, uint8_t i);
		void GetShadowMatrices(DirectX::XMMATRIX* pShadows) const;

		DepthStencil& GetShadowMap();
		const DescriptorTable& GetShadowTable() const;
		const Framebuffer& GetFramebuffer() const;
		DirectX::FXMMATRIX GetShadowMatrix(uint8_t i) const;
		DirectX::FXMMATRIX GetShadowViewMatrix() const;

	protected:
		bool createConstantBuffers();
		bool createShadowMaps();
		bool createDescriptorTables();

		void setMatrices(const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj,
			const DirectX::XMFLOAT4& lightPt);
		void setConstants();
		// Compute the near and far plane by intersecting an Ortho Projection with the Scenes AABB.
		void computeNearAndFar(float& nearPlane, float& farPlane, DirectX::CXMVECTOR lightOrthoMin,
			DirectX::CXMVECTOR lightOrthoMax, DirectX::FXMVECTOR* pPointsInCameraView);
		void createFrustumPoints(const float casIntervalBegin, const float casIntervalEnd,
			DirectX::CXMMATRIX proj, DirectX::XMVECTOR* pCornerPtsWorld);

		static const uint32_t FrameCount = FRAME_COUNT;

		Device				m_device;

		float				m_casPartPercent[MAX_CASCADES];	// Values are 0% to 100% and represent a percent of the frustum
		float				m_pcfBlurSize;
		float				m_casBlendAmt;

		float				m_shadowMapSize;
		uint8_t				m_numCasLevels;
		uint8_t				m_frameIndex;

		DirectX::XMFLOAT4	m_sceneAABBMin;
		DirectX::XMFLOAT4	m_sceneAABBMax;

		DirectX::XMFLOAT4X4	m_shadowProj[MAX_CASCADES];
		DirectX::XMFLOAT4X4	m_shadowView;

		std::shared_ptr<DescriptorTableCache> m_descriptorTableCache;

		Viewport			m_viewports[MAX_CASCADES];
		RectRange			m_scissorRects[MAX_CASCADES];

		ConstantBuffer		m_cbShadow;
		DepthStencil		m_shadowMap;
		DescriptorTable		m_shadowTables[FrameCount];
		Framebuffer			m_framebuffer;
	};
}
