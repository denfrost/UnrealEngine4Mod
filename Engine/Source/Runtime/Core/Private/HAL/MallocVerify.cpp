// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MallocVerify.cpp: Helper class to track memory allocations
=============================================================================*/

#include "CorePrivatePCH.h"
#include "MemoryMisc.h"
#include "MallocVerify.h"

#if MALLOC_VERIFY

void FMallocVerify::Malloc(void* Ptr)
{
	if (Ptr)
	{
		FScopeLock Lock(&AllocatedPointersCritical);

		UE_CLOG(AllocatedPointers.Contains(Ptr), LogMemory, Fatal, TEXT("Malloc allocated pointer that's already been allocated: 0x%016llx"), (int64)(PTRINT)Ptr);

		AllocatedPointers.Add(Ptr);			
	}
}

void FMallocVerify::Realloc(void* OldPtr, void* NewPtr)
{
	FScopeLock Lock(&AllocatedPointersCritical);
	if (OldPtr != NewPtr)
	{			
		if (OldPtr)
		{
			UE_CLOG(!AllocatedPointers.Contains(OldPtr), LogMemory, Fatal, TEXT("Realloc entered with old pointer that hasn't been allocated yet: 0x%016llx"), (int64)(PTRINT)OldPtr);

			AllocatedPointers.Remove(OldPtr);
		}
		if (NewPtr)
		{
			UE_CLOG(AllocatedPointers.Contains(NewPtr), LogMemory, Fatal, TEXT("Realloc allocated new pointer that has already been allocated: 0x%016llx"), (int64)(PTRINT)NewPtr);

			AllocatedPointers.Add(NewPtr);
		}
	}
	else if (OldPtr)
	{
		UE_CLOG(!AllocatedPointers.Contains(OldPtr), LogMemory, Fatal, TEXT("Realloc entered with old pointer that hasn't been allocated yet: 0x%016llx"), (int64)(PTRINT)OldPtr);
	}
}

void FMallocVerify::Free(void* Ptr)
{
	if (Ptr)
	{
		FScopeLock Lock(&AllocatedPointersCritical);

		UE_CLOG(!AllocatedPointers.Contains(Ptr), LogMemory, Fatal, TEXT("Free attempts to free a pointer that hasn't been allocated yet: 0x%016llx"), (int64)(PTRINT)Ptr);

		AllocatedPointers.Remove(Ptr);
	}
}

#endif // MALLOC_VERIFY