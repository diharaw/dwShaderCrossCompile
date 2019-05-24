#pragma once

#include <string>
#include <vector>

namespace spirv_compiler
{
    extern bool compile(const std::string& path, std::vector<unsigned int>& spirv);
}
