#include "cross_compiler.h"

#include <spirv_glsl.hpp>
#include <spirv_hlsl.hpp>
#include <spirv_msl.hpp>

#include <iostream>

namespace cross_compiler
{
    const char* g_TypeTableStr[] = {
        "Unknown",
        "Void",
        "Boolean",
        "Char",
        "Int",
        "UInt",
        "Int64",
        "UInt64",
        "AtomicCounter",
        "Float",
        "Double",
        "Struct",
        "Image",
        "SampledImage",
        "Sampler"
    };
    
    size_t g_TypeTableSize[] = {
        0,
        0,
        1,
        1,
        4,
        4,
        8,
        8,
        0,
        4,
        8,
        0,
        0,
        0,
        0
    };
    
    bool compile(const std::vector<unsigned int>& spirv, ShadingLanguage output_lang, std::string& output_src)
    {
        if (spirv.size() == 0)
        {
            printf("No valid SPIR-V bytecode provided!");
            return false;
        }
        
        if (output_lang == SHADING_LANGUAGE_GLSL_ES2)
        {
            spirv_cross::CompilerGLSL glsl(std::move(spirv));
            glsl.build_combined_image_samplers();
            
            for (auto& remap : glsl.get_combined_image_samplers())
            {
                glsl.set_name(remap.combined_id, glsl.get_name(remap.image_id));
            }
            
            spirv_cross::ShaderResources resources = glsl.get_shader_resources();
            
            for(auto& ubo : resources.uniform_buffers)
            {
                std::string baseType = glsl.get_name(ubo.base_type_id);
                spirv_cross::SPIRType type = glsl.get_type(ubo.base_type_id);
                
                std::cout << baseType << "\n" << std::endl;
                
                for(uint32_t i = 0; i < type.member_types.size(); i++)
                {
                    spirv_cross::SPIRType memberType = glsl.get_type(type.member_types[i]);
                    std::cout << glsl.get_member_name(ubo.base_type_id, i) << ", Type: " << g_TypeTableStr[memberType.basetype] << ", Size: " << g_TypeTableSize[memberType.basetype];
                    
                    if(memberType.vecsize > 1 || memberType.columns > 1)
                        std::cout << ", Columns: " << memberType.columns << ", Rows: " << memberType.vecsize;
                    
                    if(memberType.array.size() > 0)
                        std::cout << ", Array Size: " << memberType.array[0] << ", Total Size: " << g_TypeTableSize[memberType.basetype] * memberType.array[0] << std::endl;
                    else
                        std::cout << std::endl;
                }
                
                std::cout << "\n" << std::endl;
                
                std::string name = baseType.substr(2, baseType.size() - 2);
                
                name[0] = std::tolower(name[0]);
                
                glsl.set_name(ubo.id, name);
            }
            
            spirv_cross::CompilerGLSL::Options options;
            options.version = 200;
            options.es = true;
            options.vulkan_semantics = false;
            options.enable_420pack_extension = true;
            glsl.set_common_options(options);
            
            output_src = glsl.compile();
        }
        else if (output_lang == SHADING_LANGUAGE_GLSL_ES3)
        {
            spirv_cross::CompilerGLSL glsl(std::move(spirv));
            glsl.build_combined_image_samplers();
            
            for (auto &remap : glsl.get_combined_image_samplers())
            {
                glsl.set_name(remap.combined_id, glsl.get_name(remap.image_id));
            }
            
            spirv_cross::ShaderResources resources = glsl.get_shader_resources();
            
            for(auto& ubo : resources.uniform_buffers)
            {
                std::string baseType = glsl.get_name(ubo.base_type_id);
                baseType[0] = std::tolower(baseType[0]);
                glsl.set_name(ubo.id, baseType);
            }
            
            spirv_cross::CompilerGLSL::Options options;
            options.version = 310;
            options.es = true;
            options.vulkan_semantics = false;
            options.enable_420pack_extension = true;
            glsl.set_common_options(options);
            
            output_src = glsl.compile();
        }
        else if (output_lang == SHADING_LANGUAGE_GLSL_450)
        {
            spirv_cross::CompilerGLSL glsl(std::move(spirv));
            glsl.build_combined_image_samplers();
            
            for (auto &remap : glsl.get_combined_image_samplers())
            {
                glsl.set_name(remap.combined_id, glsl.get_name(remap.image_id));
            }
            
            spirv_cross::ShaderResources resources = glsl.get_shader_resources();
            
            for(auto& ubo : resources.uniform_buffers)
            {
                std::string baseType = glsl.get_name(ubo.base_type_id);
                baseType[0] = std::tolower(baseType[0]);
                glsl.set_name(ubo.id, baseType);
            }
            
            spirv_cross::CompilerGLSL::Options options;
            options.version = 450;
            options.es = false;
            options.vulkan_semantics = false;
            options.enable_420pack_extension = true;
            glsl.set_common_options(options);
            
            output_src = glsl.compile();
        }
        else if (output_lang == SHADING_LANGUAGE_GLSL_VK)
        {
            spirv_cross::CompilerGLSL glsl(std::move(spirv));
            
            spirv_cross::CompilerGLSL::Options options;
            options.version = 450;
            options.es = false;
            options.vulkan_semantics = true;
            options.enable_420pack_extension = true;
            glsl.set_common_options(options);
            
            spirv_cross::ShaderResources resources = glsl.get_shader_resources();
            
            for(auto& ubo : resources.uniform_buffers)
            {
                std::string baseType = glsl.get_name(ubo.base_type_id);
                baseType[0] = std::tolower(baseType[0]);
                glsl.set_name(ubo.id, baseType);
            }
            
            output_src = glsl.compile();
        }
        else if (output_lang == SHADING_LANGUAGE_HLSL)
        {
            spirv_cross::CompilerHLSL hlsl(std::move(spirv));
            
            spirv_cross::CompilerHLSL::Options options;
            options.shader_model = 50;
            
            hlsl.set_hlsl_options(options);
            
            spirv_cross::ShaderResources resources = hlsl.get_shader_resources();
            
            for(auto& ubo : resources.uniform_buffers)
            {
                std::string baseType = hlsl.get_name(ubo.base_type_id);
                baseType[0] = std::tolower(baseType[0]);
                hlsl.set_name(ubo.id, baseType);
            }
            
            output_src = hlsl.compile();
        }
        else if (output_lang == SHADING_LANGUAGE_MSL)
        {
            spirv_cross::CompilerMSL msl(std::move(spirv));
            
            spirv_cross::CompilerMSL::Options options;
            
            msl.set_msl_options(options);
            
            spirv_cross::ShaderResources resources = msl.get_shader_resources();
            
            for(auto& ubo : resources.uniform_buffers)
            {
                std::string baseType = msl.get_name(ubo.base_type_id);
                baseType[0] = std::tolower(baseType[0]);
                msl.set_name(ubo.id, baseType);
            }
            
            output_src = msl.compile();
        }
        
        return true;
    }
}
