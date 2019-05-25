#pragma once

#include <vector>

namespace cross_compiler
{
    enum ShadingLanguage
    {
        SHADING_LANGUAGE_GLSL_ES2,
        SHADING_LANGUAGE_GLSL_ES3,
        SHADING_LANGUAGE_GLSL_450,
        SHADING_LANGUAGE_GLSL_VK,
        SHADING_LANGUAGE_HLSL,
        SHADING_LANGUAGE_MSL
    };
    
    extern bool compile(const std::vector<unsigned int>& spirv, ShadingLanguage output_lang, std::string& output_src);
}
