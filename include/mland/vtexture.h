#pragma once
#include "common.h"
#include "vulk.h"
namespace mland {

struct VTexture {
	MCLASS(VTexture);
	vkr::DeviceMemory memory;
	vkr::Image image;

	~VTexture(){
		image.clear();
		memory.clear();
	}
};

}