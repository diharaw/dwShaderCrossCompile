#include "cross_compiler.h"

#include <spirv_glsl.hpp>
#include <spirv_hlsl.hpp>
#include <spirv_msl.hpp>

#include <locale>
#include <iostream>

namespace cross_compiler
{
    const char* g_TypeTableStr[] = {
		"Unknown",
		"Void",
		"Boolean",
		"SByte",
		"UByte",
		"Short",
		"UShort",
		"Int",
		"UInt",
		"Int64",
		"UInt64",
		"AtomicCounter",
		"Half",
		"Float",
		"Double",
		"Struct",
		"Image",
		"SampledImage",
		"Sampler",
		"AccelerationStructureNV",
		"ControlPointArray",
		"Char"
    };
    
    size_t g_TypeTableSize[] = {
        0,
		0,
		1,
		1,
		1,
		4,
		4,
		8,
		8,
		0,
		2,
		4,
		8,
		0,
		0,
		0,
		0,
		0,
		0,
		1
    };
    
    void fix_matrix_force_colmajor(spirv_cross::Compiler& compiler)
    {
        /* go though all uniform block matrixes and decorate them with
         column-major, this is needed in the HLSL backend to fix the
         multiplication order
         */
        spirv_cross::ShaderResources res = compiler.get_shader_resources();
        
        for (const spirv_cross::Resource& ub_res: res.uniform_buffers)
        {
            const spirv_cross::SPIRType& ub_type = compiler.get_type(ub_res.base_type_id);
            
            for (int m_index = 0; m_index < (int)ub_type.member_types.size(); m_index++)
            {
                const spirv_cross::SPIRType& m_type = compiler.get_type(ub_type.member_types[m_index]);
                
                if ((m_type.basetype == spirv_cross::SPIRType::Float) && (m_type.vecsize > 1) && (m_type.columns > 1))
                    compiler.set_member_decoration(ub_res.base_type_id, m_index, spv::DecorationColMajor);
            }
        }
        
        for (const spirv_cross::Resource& ssbo_res: res.storage_buffers)
        {
            const spirv_cross::SPIRType& ssbo_type = compiler.get_type(ssbo_res.base_type_id);
            
            for (int m_index = 0; m_index < (int)ssbo_type.member_types.size(); m_index++)
            {
                const spirv_cross::SPIRType& m_type = compiler.get_type(ssbo_type.member_types[m_index]);
                
                if ((m_type.basetype == spirv_cross::SPIRType::Float) && (m_type.vecsize > 1) && (m_type.columns > 1))
                    compiler.set_member_decoration(ssbo_res.base_type_id, m_index, spv::DecorationColMajor);
            }
        }
    }

    
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
                
				std::locale loc;
                name[0] = std::tolower(name[0], loc);
                
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
				std::locale loc;
                baseType[0] = std::tolower(baseType[0], loc);
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
				std::locale loc;
                baseType[0] = std::tolower(baseType[0], loc);
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
				std::locale loc;
                baseType[0] = std::tolower(baseType[0], loc);
                glsl.set_name(ubo.id, baseType);
            }
            
            output_src = glsl.compile();
        }
        else if (output_lang == SHADING_LANGUAGE_HLSL)
        {
            spirv_cross::CompilerHLSL hlsl(std::move(spirv));
            
            spirv_cross::CompilerGLSL::Options common_options;
            hlsl.set_common_options(common_options);
            
            spirv_cross::CompilerHLSL::Options options;
            options.shader_model = 50;
            
            hlsl.set_hlsl_options(options);
            
            spirv_cross::ShaderResources resources = hlsl.get_shader_resources();
            
            for(auto& ubo : resources.uniform_buffers)
            {
                std::string baseType = hlsl.get_name(ubo.base_type_id);
				std::locale loc;
                baseType[0] = std::tolower(baseType[0], loc);
                hlsl.set_name(ubo.id, baseType);
            }
            
            fix_matrix_force_colmajor(hlsl);
            
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
				std::locale loc;
                baseType[0] = std::tolower(baseType[0], loc);
                msl.set_name(ubo.id, baseType);
            }
            
            output_src = msl.compile();
        }
        
        return true;
    }

	bool compare_descriptors(Descriptor d1, Descriptor d2)
	{
		return d1.binding < d2.binding;
	}

	bool generate_reflection_data(const std::vector<unsigned int>& spirv, ShadingLanguage output_lang, ReflectionData& reflection_data)
	{
		const std::string kDescriptorTypes[] =
		{
			"DESCRIPTOR_TYPE_UBO",
			"DESCRIPTOR_TYPE_SSBO",
			"DESCRIPTOR_TYPE_SAMPLER",
			"DESCRIPTOR_TYPE_TEXTURE",
			"DESCRIPTOR_TYPE_IMAGE",
			"DESCRIPTOR_TYPE_PUSH_CONSTANT"
		};

		if (output_lang == SHADING_LANGUAGE_GLSL_VK)
		{
			spirv_cross::CompilerGLSL glsl(std::move(spirv));

			spirv_cross::CompilerGLSL::Options options;
			options.version = 450;
			options.es = false;
			options.vulkan_semantics = true;
			options.enable_420pack_extension = true;
			glsl.set_common_options(options);

			spirv_cross::ShaderResources resources = glsl.get_shader_resources();

			for (const spirv_cross::Resource &resource : resources.separate_images)
			{
				uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
				uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);

				Descriptor desc;

				desc.type = DESCRIPTOR_TYPE_TEXTURE;
				desc.binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
				desc.name = resource.name;

				reflection_data.descriptor_sets[set].push_back(desc);
			}

			for (const spirv_cross::Resource &resource : resources.separate_samplers)
			{
				uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
				uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);

				Descriptor desc;

				desc.type = DESCRIPTOR_TYPE_SAMPLER;
				desc.binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
				desc.name = resource.name;

				reflection_data.descriptor_sets[set].push_back(desc);
			}

			for (const spirv_cross::Resource &resource : resources.uniform_buffers)
			{
				uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
				uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);

				Descriptor desc;

				desc.type = DESCRIPTOR_TYPE_UBO;
				desc.binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
				desc.name = resource.name;

				reflection_data.descriptor_sets[set].push_back(desc);
			}

			for (const spirv_cross::Resource &resource : resources.storage_buffers)
			{
				uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
				uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);

				Descriptor desc;

				desc.type = DESCRIPTOR_TYPE_SSBO;
				desc.binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
				desc.name = resource.name;

				reflection_data.descriptor_sets[set].push_back(desc);
			}

			for (const spirv_cross::Resource &resource : resources.storage_images)
			{
				uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
				uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);

				Descriptor desc;

				desc.type = DESCRIPTOR_TYPE_IMAGE;
				desc.binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
				desc.name = resource.name;

				reflection_data.descriptor_sets[set].push_back(desc);
			}

			for (const spirv_cross::Resource &resource : resources.push_constant_buffers)
			{
				uint32_t set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
				uint32_t binding = glsl.get_decoration(resource.id, spv::DecorationBinding);

				std::string baseType = glsl.get_name(resource.base_type_id);
				spirv_cross::SPIRType type = glsl.get_type(resource.base_type_id);

				for (uint32_t i = 0; i < type.member_types.size(); i++)
				{
					spirv_cross::SPIRType memberType = glsl.get_type(type.member_types[i]);

					PushConstantMembers desc;
					
					desc.name = glsl.get_member_name(resource.base_type_id, i);
					desc.offset = glsl.type_struct_member_offset(type, i);
					desc.type = g_TypeTableStr[memberType.basetype];
					desc.array_size = memberType.array.size() > 0 ? memberType.array[0] : 1;

					reflection_data.push_constant_members.push_back(desc);
				}
			}

			std::cout << "Push Constant Members : " << std::endl;

			for (auto& member : reflection_data.push_constant_members)
			{
				std::cout << "\tName = " << member.name << std::endl;
				std::cout << "\tOffset = " << member.offset << std::endl;
				std::cout << "\tType = " << member.type << std::endl;
				std::cout << "\tArray Size = " << member.array_size << std::endl;
			}

			for (auto& set : reflection_data.descriptor_sets)
			{
				std::sort(set.second.begin(), set.second.end(), compare_descriptors);

				std::cout << "Set = " << set.first << std::endl;
				
				for (auto& binding : set.second)
				{
					std::cout << "\tBinding = " << binding.binding << std::endl;
					std::cout << "\tType = " << kDescriptorTypes[binding.type] << std::endl;
					std::cout << "\tName = " << binding.name << std::endl;
				}
			}
		}

		return true;
	}
}
