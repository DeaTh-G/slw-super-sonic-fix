#include "pch.h"
#include "..\..\Depends\HE1ML\Source\ModLoader.h"

HOOK(void, __fastcall, ResModelReplaceHook, ASLR(0x00BFDC10), hh::gfx::res::ResModel* in_pThis, void* edx, const char* in_pName, uint* in_pUnk1, csl::fnd::IAllocator* in_pAllocator)
{
	originalResModelReplaceHook(in_pThis, edx, in_pName, in_pUnk1, in_pAllocator);
}

/*HOOK(void, __fastcall, InitializeSSHook, ASLR(0x008F88E0), app::Player::SuperSonicInfo* in_pThis, void* edx, app::GameDocument& in_rDocument)
{
	originalInitializeSSHook(in_pThis, edx, in_rDocument);

	auto mainMeshGroup = in_pThis->Model.GetResMeshGroup(0);
	auto mesh = mainMeshGroup.GetResMesh(0);

	ushort* pFaces{};
	void* pVertices{};
	auto* pIndexBuffer = mesh->m_pPrimitive->m_pIndexBuffer;
	auto* pVertexStream = mesh->m_pPrimitive->m_pVertexBuffers->m_pStream;

	pIndexBuffer->m_pIndexBuffer->Lock(mesh->m_pPrimitive->m_StartIndex, pIndexBuffer->m_IndexSize * pIndexBuffer->m_IndexCount, (void**)&pFaces, 0);
	pVertexStream->m_pVertexBuffer->Lock(mesh->m_pPrimitive->m_pVertexBuffers->m_Offset, pVertexStream->m_Stride * pVertexStream->m_Unk1, (void**)&pVertices, D3DLOCK_READONLY);

	for (size_t i = 0; i < pIndexBuffer->m_IndexCount; i++)
	{
		// Skip over any degenerate tris found.
		if (pFaces[i] == USHRT_MAX)
			continue;

		// Replace the tris that are connected to Teeth_Low_L or Teeth_Up_L with degenerate tris as to no longer have them displayed from this mesh
		// TODO: Move these into the mesh group Sonic_Mouth_L once that mesh group has been created.
		if (((char*)pVertices)[pVertexStream->m_Stride * pFaces[i] + 99] == 9 || ((char*)pVertices)[pVertexStream->m_Stride * pFaces[i] + 99] == 10)
		{
			pFaces[i] = USHRT_MAX;
		}
	}

	pVertexStream->m_pVertexBuffer->Unlock();
	pIndexBuffer->m_pIndexBuffer->Unlock();

	// Store the mouth mesh that needs to go into the mesh group Sonic_Mouth_L for later usage.
	auto mouthMesh = mainMeshGroup.GetResMesh(3);

	// Adjust the array back by one since we removed an element in the middle of it.
	memmove(&mainMeshGroup->pMeshes[3], &mainMeshGroup->pMeshes[4], sizeof(hh::gfx::res::ResMeshData) * 8);
	mainMeshGroup->MeshCount -= 1;
}*/

extern "C"
{
	void _declspec(dllexport) __cdecl Init(ModInfo_t* in_pModInfo)
	{
		INSTALL_HOOK(ResModelReplaceHook);
	}
}