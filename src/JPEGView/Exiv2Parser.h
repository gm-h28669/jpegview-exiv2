#pragma once
#include <string>    
#include <cstdint> 

namespace Exiv2Parser {
	std::string GetLensModel(const uint8_t* pApp1Block);
}
