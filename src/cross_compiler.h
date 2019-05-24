#pragma once

#include <vector>

namespace cross_compiler
{
    enum ShadingLanguage
    {
        SHADING_LANGUAGE_HLSL,
        SHADING_LANGUAGE_MSL
    };
    
    extern bool compile(const std::vector<unsigned int>& spirv, ShadingLanguage output_lang, std::string& output_src);
}
