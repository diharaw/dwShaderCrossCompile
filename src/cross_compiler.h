#pragma once

#include <vector>
#include <string>
#include <unordered_map>

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

	enum DescriptorType
	{
		DESCRIPTOR_TYPE_UBO,
		DESCRIPTOR_TYPE_SSBO,
		DESCRIPTOR_TYPE_SAMPLER,
		DESCRIPTOR_TYPE_TEXTURE,
		DESCRIPTOR_TYPE_IMAGE
	};

	struct PushConstantMembers
	{
		uint32_t offset;
		std::string type;
		std::string name;
		uint32_t array_size;
	};

	struct Descriptor
	{
		DescriptorType type;
		uint32_t binding;
		std::string name;
	};

	struct ReflectionData
	{
		std::vector<PushConstantMembers> push_constant_members;
		std::unordered_map<uint32_t, std::vector<Descriptor>> descriptor_sets;
	};
    
    extern bool compile(const std::vector<unsigned int>& spirv, ShadingLanguage output_lang, std::string& output_src);
	extern bool generate_reflection_data(const std::vector<unsigned int>& spirv, ShadingLanguage output_lang, ReflectionData& reflection_data);
}
