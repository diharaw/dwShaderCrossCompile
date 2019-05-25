#pragma once

#include <string>
#include <vector>

namespace spirv_compiler
{
    enum ShaderStage
    {
        SHADER_STAGE_VERTEX,
        SHADER_STAGE_FRAGMENT,
        SHADER_STAGE_COMPUTE
    };
    
    extern bool compile(const std::string& path, std::vector<unsigned int>& spirv);
}
