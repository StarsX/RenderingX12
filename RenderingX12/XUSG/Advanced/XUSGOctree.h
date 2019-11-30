//--------------------------------------------------------------------------------------
// By Stars XU Tianchen
//--------------------------------------------------------------------------------------

#pragma once

#include "XUSGStaticModel.h"

#define CHILDRIEN_NUM	8

namespace XUSG
{
	class OctNode;
	using Octree = std::unique_ptr<OctNode>;

	class OctNode
	{
	public:
		enum Visibility : uint8_t
		{
			VISIBILITY_INTERSECT,
			VISIBILITY_INSIDE,
			VISIBILITY_OUTSIDE
		};

		OctNode(uint32_t numModels, const std::shared_ptr<StaticModel>* pModels,
			const DirectX::XMFLOAT3& eyePt);
		virtual ~OctNode();

		void CreateTree(SubsetFlags subsetFlags);
		void Init(const DirectX::XMFLOAT3& center, float diameter);
		void Sort(std::vector<DirectX::XMUINT2>& meshIDQueue,
			DirectX::CXMMATRIX viewProj, bool isNearToFar, bool isAllVisible = false) const;
		void TraverseBoundary(DirectX::CXMMATRIX viewProj, bool isNearToFar,
			bool allVisible, uint8_t begin, uint8_t logSize) const;	// 0x, 1y, 2z
		void RenderBoundary(DirectX::CXMMATRIX viewProj, bool isNearToFar, bool isAllVisible) const;

		static void SetLooseCoeff(float looseCoeff);
		static void Sort(const Octree* pOctTrees, std::vector<DirectX::XMUINT2>* pMeshIDQueues,
			uint8_t opaqueQueue, uint8_t alphaQueue, DirectX::CXMMATRIX viewProj,
			DirectX::CXMMATRIX viewProjI, bool isAlphaQueueN2F);
		static void ComputeFrustumBound(DirectX::CXMMATRIX viewProjI);

	protected:
		friend OctNode;

		void addMesh(const DirectX::XMUINT2& meshID, uint8_t level);
		void shrinkList();
		void sort(std::vector<DirectX::XMUINT2>& meshIDQueue,
			DirectX::CXMMATRIX viewProj, bool isNearToFar, bool isAllVisible) const;
		void traverse(std::vector<DirectX::XMUINT2>& meshIDQueue, DirectX::CXMMATRIX viewProj,
			bool isNearToFar, bool isAllVisible, uint8_t begin, uint8_t logSize) const;	// 0x, 1y, 2z

		bool isInside(const DirectX::XMUINT2& meshID, const DirectX::XMFLOAT3& center, float radius) const;
		uint8_t isVisible(DirectX::CXMMATRIX viewProj) const;
		uint8_t isVisible(const DirectX::XMUINT2& meshID, DirectX::CXMMATRIX viewProj) const;

		DirectX::XMFLOAT3			m_corner[CHILDRIEN_NUM];	// Corner Position

		std::unique_ptr<OctNode>	m_child[CHILDRIEN_NUM];		// Children

		std::vector<DirectX::XMUINT2> m_meshIDs;
		const std::shared_ptr<StaticModel>* m_pModels;			// Mesh List

		uint32_t					m_numModels;

		float						m_center[3];				// Center Position
		float						m_diameter;

		const DirectX::XMFLOAT3& m_eyePt;

		static DirectX::XMFLOAT3	m_frustumMax;
		static DirectX::XMFLOAT3	m_frustumMin;
		static float				m_looseCoeff;
	};
}
