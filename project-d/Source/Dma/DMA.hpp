#pragma once
#include <Memory/Memory.h>

class DMA
{
private:

public:

	bool Init();
	bool RefreshGameBases(bool reinitializeProcess);

	static DMA& Get()
	{
		static DMA instance;
		return instance;
	}
};

inline DMA& dma = DMA::Get();
