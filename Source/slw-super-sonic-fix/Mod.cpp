#include "pch.h"
#include "..\..\Depends\HE1ML\Source\ModLoader.h"
#include "..\..\Depends\HedgeLib\HedgeLib\include\hedgelib\models\hl_hh_model.h"

#pragma warning(disable: 4996)

volatile char* buffer;
size_t pos = 0;

void* AllocStatic(size_t size, int alignment)
{
	auto p = csl::ut::RoundUp(pos, alignment);
	pos += size;
	return (void*)&buffer[p];
}

void ResetStaticBuffer()
{
	pos = 0;
}

// Replace strcmp with a custom implementation as to make sure that the compiler inlines the function call.
__forceinline int StrCmp(const char* str1, const char* str2) {
	while (*str1 && *str2) {
		if (*str1 != *str2) {
			return *str1 - *str2;
		}
		str1++;
		str2++;
	}

	if (*str1 == '\0' && *str2 == '\0') {
		return 0;
	}
	else if (*str1 == '\0') {
		return -1;
	}
	else {
		return 1;
	}
}

// Replace strcpy with a custom implementation as to make sure that the compiler inlines the function call.
__forceinline void StrCpy(char* dest, const char* src) {
	while (*src != '\0') {
		*dest++ = *src++;
	}
	*dest = '\0';
}

// Replace memmove with custom implementation as to make sure that the compiler inlines the function call.
__forceinline void MemMove(void* dest, const void* src, size_t count) {
	char* d = static_cast<char*>(dest);
	const char* s = static_cast<const char*>(src);

	if (d < s) {
		// Copy forward from start to end
		for (size_t i = 0; i < count; ++i) {
			d[i] = s[i];
		}
	}
	else if (d > s) {
		// Copy backward from end to start
		for (size_t i = count; i > 0; --i) {
			d[i - 1] = s[i - 1];
		}
	}
}


HOOK(void, __fastcall, ResModelReplaceHook, ASLR(0x00BFDC10), char** in_pThis, void* edx, const char* in_pName, uint* in_pUnk1, csl::fnd::IAllocator* in_pAllocator)
{
	if (StrCmp(in_pName, "chr_supersonic"))
		return originalResModelReplaceHook(in_pThis, edx, in_pName, in_pUnk1, in_pAllocator);

	// Calculate and allocate the necessary size required to create a copy of the model data that is currently being loaded.
	hl::u32 fileSize = _byteswap_ulong(reinterpret_cast<hh::db::CSampleChunkResource2*>(*in_pThis)->m_Size);
	if ((fileSize & csl::ut::SIGN_BIT) == 0)
		return originalResModelReplaceHook(in_pThis, edx, in_pName, in_pUnk1, in_pAllocator);

	fileSize &= ~csl::ut::SIGN_BIT;

	// Calculate checksum of the current model and compare against the original Super Sonic model's checksum to see if the data should be modified or not.
	hl::u32 checksum = 0;
	for (size_t i = 0; i < fileSize; i++)
		checksum += (*in_pThis)[i];

	if (checksum != 0x003E7A1D)
		return originalResModelReplaceHook(in_pThis, edx, in_pName, in_pUnk1, in_pAllocator);

	ResetStaticBuffer();

	auto* pEditedData = AllocStatic(fileSize, 16);
	MemMove(pEditedData, *in_pThis, fileSize);

	// Resolve the pointers on the copied data to be able to use the pointers in the file as memory addresses.
	reinterpret_cast<hh::db::CSampleChunkResource2*>(pEditedData)->ResolvePointer();
	auto* pEditedModel = reinterpret_cast<hl::hh::mirage::raw_skeletal_model_v5*>((char*)pEditedData + 0x30);
	
	// Allocate a new array of pointers as a way to extend the original mesh group array so that we can add Sonic_Mouth_L to it later.
	pEditedModel->meshGroups.count += _byteswap_ulong(1);
	auto** ppEditedMeshGroup = reinterpret_cast<hl::hh::mirage::raw_mesh_group_r1**>(AllocStatic(sizeof(hl::hh::mirage::raw_mesh_group_r1*) * _byteswap_ulong(pEditedModel->meshGroups.count), 4));
	for (size_t i = 0; i < _byteswap_ulong(pEditedModel->meshGroups.count) - 1; i++)
		ppEditedMeshGroup[i] = pEditedModel->meshGroups[i].get();

	pEditedModel->meshGroups.dataPtr.set(reinterpret_cast<hl::off32<hl::hh::mirage::raw_mesh_group_r1>*>(ppEditedMeshGroup));

	// Allocate a array of pointers for two meshes to be able to recreate how Sonic_Mouth_L should look like.
	auto** ppLeftMouthMeshes = reinterpret_cast<hl::hh::mirage::raw_mesh_r1**>(AllocStatic(sizeof(hl::hh::mirage::raw_mesh_r1*) * 2, 4));

	// Allocate a new mesh group to store the inner and outer mouth meshes in.
	auto* pLeftMouthMeshGroup = reinterpret_cast<hl::hh::mirage::raw_mesh_group_r1*>(AllocStatic(sizeof(hl::hh::mirage::raw_mesh_group_r1) + strlen("Sonic_Mouth_L"), 4));
	if (pLeftMouthMeshGroup)
	{
		pLeftMouthMeshGroup->opaq.count = _byteswap_ulong(2);
		pLeftMouthMeshGroup->opaq.dataPtr.set(reinterpret_cast<hl::off32<hl::hh::mirage::raw_mesh_r1>*>(ppLeftMouthMeshes));
		pLeftMouthMeshGroup->trans.count = 0;
		pLeftMouthMeshGroup->punch.count = 0;
		pLeftMouthMeshGroup->special.count = 0;
		StrCpy(pLeftMouthMeshGroup->name(), "Sonic_Mouth_L");

		// Add the newly creates mesh group for the left mouth to the array of mesh groups on the model.
		pEditedModel->meshGroups[2] = pLeftMouthMeshGroup;
	}

	// Store the pointer to the outer mouth mesh in our newly created mesh group.
	if (ppLeftMouthMeshes)
	{
		ppLeftMouthMeshes[0] = pEditedModel->meshGroups[0]->opaq[3].get();

		// Adjust the array back by one since we removed an element in the middle of it.
		MemMove(&pEditedModel->meshGroups[0]->opaq[3], &pEditedModel->meshGroups[0]->opaq[4], 32);
		pEditedModel->meshGroups[0]->opaq.count -= _byteswap_ulong(1);
	}

	// Allocate a new mesh to be able to the store the information needed by vertices for the teeth and tongue meshes for the left mouth.
	auto* pInnerLeftMouthMesh = reinterpret_cast<hl::hh::mirage::raw_mesh_r1*>(AllocStatic(sizeof(hl::hh::mirage::raw_mesh_r1), 4));
	if (pInnerLeftMouthMesh)
	{
		// Copy offsets that are matching with other meshes in the model data.
		pInnerLeftMouthMesh->materialName = pEditedModel->meshGroups[0]->opaq[0]->materialName;
		pInnerLeftMouthMesh->vertexElements = pEditedModel->meshGroups[0]->opaq[0]->vertexElements;
		pInnerLeftMouthMesh->textureUnits = pEditedModel->meshGroups[0]->opaq[0]->textureUnits;

		// Set the vertex count to the total amount of vertices found in the teeth and tongue meshes for the left mouth.
		pInnerLeftMouthMesh->vertexCount = _byteswap_ulong(76);
		pInnerLeftMouthMesh->vertexSize = _byteswap_ulong(104);

		// Store the pointer to the mesh that contains the teeth and the tongue for the left mouth in our newly created mesh group.
		if (ppLeftMouthMeshes)
			ppLeftMouthMeshes[1] = pInnerLeftMouthMesh;
	}

	auto* pInnerLeftMouthMeshBoneIndices = reinterpret_cast<hl::u8*>(AllocStatic(sizeof(hl::u8) * 2, 1));
	auto* pInnerLeftMouthMeshFaces = reinterpret_cast<hl::u16*>(AllocStatic(sizeof(hl::u16) * 139, 2));
	auto* pInnerLeftMouthMeshVertices = reinterpret_cast<hl::u8*>(AllocStatic(sizeof(hl::u8) * _byteswap_ulong(pInnerLeftMouthMesh->vertexSize) * _byteswap_ulong(pInnerLeftMouthMesh->vertexCount), 1));

	// Add the necessary bone node indices to the newly created mesh so that the weighting can be carried over for the teeth and tongue.
	if (pInnerLeftMouthMeshBoneIndices)
	{
		pInnerLeftMouthMeshBoneIndices[0] = 47;
		pInnerLeftMouthMeshBoneIndices[1] = 51;

		pInnerLeftMouthMesh->boneNodeIndices.count = _byteswap_ulong(2);
		pInnerLeftMouthMesh->boneNodeIndices.dataPtr.set(pInnerLeftMouthMeshBoneIndices);
	}

	// Add the necessary faces to the newly created mesh so that the faces can be carried over for the teeth and tongue.
	if (pInnerLeftMouthMeshFaces)
	{
		size_t faceCount = 0;

		auto* pMesh = pEditedModel->meshGroups[0]->opaq[0].get();
		for (size_t i = 0; i < _byteswap_ulong(pMesh->faces.count); i++)
		{
			// Skip over any degenerate tris found.
			if (pMesh->faces[i] == USHRT_MAX)
				continue;

			size_t offset = _byteswap_ulong(pMesh->vertexSize) * _byteswap_ushort(pMesh->faces[i]) + 96;
			if (((char*)pMesh->vertices.get())[offset] == 9 || ((char*)pMesh->vertices.get())[offset] == 10)
			{
				// Transfer the face to the newly created mesh so that it shows up on the correct mesh group.
				pInnerLeftMouthMeshFaces[faceCount] = _byteswap_ushort(_byteswap_ushort(pMesh->faces[i]) - (ushort)254);
				faceCount++;

				// Replace the tris that are connected to Teeth_Low_L or Teeth_Up_L with degenerate tris as to no longer have them displayed from this mesh.
				pMesh->faces[i] = USHRT_MAX;
			}
		}

		pInnerLeftMouthMesh->faces.count = _byteswap_ulong(faceCount);
		pInnerLeftMouthMesh->faces.dataPtr.set(pInnerLeftMouthMeshFaces);
	}

	// Add the necessary vertices to the newly created mesh so that the vertices can be carried over for the teeth and tongue.
	if (pInnerLeftMouthMeshVertices)
	{
		size_t vertexCount = 0;

		auto* pMesh = pEditedModel->meshGroups[0]->opaq[0].get();
		for (size_t i = 0; i < _byteswap_ulong(pMesh->vertexCount) * _byteswap_ulong(pMesh->vertexSize); i += 104)
		{
			if (((char*)pMesh->vertices.get())[i + 96] == 9 || ((char*)pMesh->vertices.get())[i + 96] == 10)
			{
				// Transfer the vertex to the newly created mesh so that it shows up on the correct mesh group.
				MemMove(&pInnerLeftMouthMeshVertices[vertexCount * _byteswap_ulong(pInnerLeftMouthMesh->vertexSize)], &((char*)pMesh->vertices.get())[i], _byteswap_ulong(pMesh->vertexSize));
				pInnerLeftMouthMeshVertices[vertexCount * _byteswap_ulong(pInnerLeftMouthMesh->vertexSize) + 96] -= 9;
				vertexCount++;
			}

			// Exit the loop early if all relevant vertices have been copied over from the mesh to the new mesh.
			if (vertexCount == _byteswap_ulong(pInnerLeftMouthMesh->vertexCount))
				break;
		}

		pInnerLeftMouthMesh->vertices = pInnerLeftMouthMeshVertices;
	}

	// The offset count is set to 0 so that the game doesn't try to resolve the offsets again when it later tries to process the model data.
	reinterpret_cast<hh::db::CSampleChunkResource2*>(pEditedData)->m_OffsetCount = 0;

	// Replace the original model data with the edited model data.
	*in_pThis = (char*)pEditedData;
	
	originalResModelReplaceHook(in_pThis, edx, in_pName, in_pUnk1, in_pAllocator);
}

extern "C"
{
	void _declspec(dllexport) __cdecl Init(ModInfo_t* in_pModInfo)
	{
		INSTALL_HOOK(ResModelReplaceHook);
	}
}