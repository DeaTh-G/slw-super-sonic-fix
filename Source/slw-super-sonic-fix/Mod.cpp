#include "pch.h"
#include "..\..\Depends\HE1ML\Source\ModLoader.h"
#include "..\..\Depends\HedgeLib\HedgeLib\include\hedgelib\models\hl_hh_model.h"

HOOK(void, __fastcall, ResModelReplaceHook, ASLR(0x00BFDC10), char** in_pThis, void* edx, const char* in_pName, uint* in_pUnk1, csl::fnd::IAllocator* in_pAllocator)
{
	if (!strcmp(in_pName, "chr_supersonic"))
	{
		// Calculate and allocate the necessary size required to create a copy of the model data that is currently being loaded.
		hl::u32 fileSize = _byteswap_ulong(reinterpret_cast<hh::db::CSampleChunkResource2*>(*in_pThis)->m_Size) & ~csl::ut::SIGN_BIT;
		auto* pEditedData = malloc(fileSize);
		memcpy(pEditedData, *in_pThis, fileSize);

		// Resolve the pointers on the copied data to be able to use the pointers in the file as memory addresses.
		reinterpret_cast<hh::db::CSampleChunkResource2*>(pEditedData)->ResolvePointer();
		auto* pEditedModel = reinterpret_cast<hl::hh::mirage::raw_skeletal_model_v5*>((char*)pEditedData + 0x30);
		
		auto* pMesh = pEditedModel->meshGroups[0]->opaq[0].get();
		for (size_t i = 0; i < _byteswap_ulong(pMesh->faces.count); i++)
		{
			// Skip over any degenerate tris found.
			if (pMesh->faces[i] == USHRT_MAX)
				continue;
		
			// Replace the tris that are connected to Teeth_Low_L or Teeth_Up_L with degenerate tris as to no longer have them displayed from this mesh
			// TODO: Move these into the mesh group Sonic_Mouth_L once that mesh group has been created.
			size_t offset = _byteswap_ulong(pMesh->vertexSize) * _byteswap_ushort(pMesh->faces[i]) + 96;
			if (((char*)pMesh->vertices.get())[offset] == 9 || ((char*)pMesh->vertices.get())[offset] == 10)
			{
				pMesh->faces[i] = USHRT_MAX;
			}
		}

		// Store the mouth mesh that needs to go into the mesh group Sonic_Mouth_L for later usage.
		auto* pMouthMesh = pEditedModel->meshGroups[0]->opaq[3].get();

		// Adjust the array back by one since we removed an element in the middle of it.
		memmove(&pEditedModel->meshGroups[0]->opaq[3], &pEditedModel->meshGroups[0]->opaq[4], 32);
		pEditedModel->meshGroups[0]->opaq.count -=  _byteswap_ulong(1);

		// Unresolve the pointers on the copied data before sending it back to the game so that the game process them normally.
		reinterpret_cast<hh::db::CSampleChunkResource2*>(pEditedData)->UnresolvePointer();
		*in_pThis = (char*)pEditedData;
	}

	originalResModelReplaceHook(in_pThis, edx, in_pName, in_pUnk1, in_pAllocator);
}

extern "C"
{
	void _declspec(dllexport) __cdecl Init(ModInfo_t* in_pModInfo)
	{
		INSTALL_HOOK(ResModelReplaceHook);
	}
}