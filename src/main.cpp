#include "spirv_compiler.h"
#include "cross_compiler.h"

#include <fstream>
#include <iostream>
#include <unordered_map>

class ArgumentParser
{
public:
    ArgumentParser()
    {
        
    }
    
    void parse(int argc, char* argv[])
    {
        for (int i = 1; i < argc; i++)
        {
            if (argv[i][0] == '-' && argv[i][1] == '-')
            {
                std::string arg = argv[i];
                std::size_t equal_sign = arg.find_last_of("=");
                
                if (equal_sign == std::string::npos)
                {
                    std::string name = arg.substr(2, arg.size() - 2);
                    
                    if (option_map.find(name) != option_map.end())
                        bool_arguments[name] = true;
                }
                else
                {
                    std::string name = arg.substr(2, equal_sign - 2);
                    std::string value = arg.substr(equal_sign + 1, arg.size() - equal_sign + 1);
    
                    if (option_map.find(name) != option_map.end())
                        arguments[name] = value;
                }
            }
            else
                ordered_arguments.push_back(argv[i]);
        }
    }
    
    void add_option(std::string name, std::string description)
    {
        if (option_map.find(name) == option_map.end())
        {
            option_map[name] = description;
            arguments[name] = "";
        }
    }
    
    void add_bool_option(std::string name, std::string description)
    {
        if (option_map.find(name) == option_map.end())
        {
            option_map[name] = description;
            bool_arguments[name] = false;
        }
    }
    
    std::string argument(std::string name)
    {
        if (option_map.find(name) == option_map.end())
            return "";
        
        return arguments[name];
    }
    
    bool bool_argument(std::string name)
    {
        if (option_map.find(name) == option_map.end())
            return "";
        
        return bool_arguments[name];
    }
    
    std::string ordered_argument(int idx)
    {
        if (idx + 1 > ordered_arguments.size())
            return "";
        
        return ordered_arguments[idx];
    }
    
    void usage()
    {
        for (auto& it : option_map)
            std::cout << "  --" << it.first << "=<value>\t\t" << it.second << std::endl;
    }
    
private:
    std::unordered_map<std::string, std::string> option_map;
    std::unordered_map<std::string, std::string> arguments;
    std::unordered_map<std::string, bool> bool_arguments;
    std::vector<std::string> ordered_arguments;
};

std::string path_without_file(std::string filepath)
{
#ifdef WIN32
    std::replace(filepath.begin(), filepath.end(), '\\', '/');
#endif
    std::size_t found = filepath.find_last_of("/\\");
    
    if (found == std::string::npos)
        return "";
    
    std::string path  = filepath.substr(0, found);
    return path;
}

std::string file_name_from_path(std::string filepath)
{
    std::size_t slash = filepath.find_last_of("/");
    
    if (slash == std::string::npos)
        slash = 0;
    else
        slash++;
    
    std::size_t dot      = filepath.find_last_of(".");
    std::string filename = filepath.substr(slash, dot - slash);
    
    return filename;
}

int main(int argc, char* argv[])
{
    ArgumentParser parser;
    
    parser.add_option("shader-stage", "The shader stage corresponding to the input shader source (vertex, fragment or compute).");
    parser.add_option("target-language", "Target shading language that the input shader source must be cross-compiled into (GLSL_ES2, GLSL_ES3, GLSL_450, GLSL_VK, HLSL or MSL).");

    if (argc > 1)
    {
        parser.parse(argc, argv);
        
        std::vector<unsigned int> spirv;
    
        std::string input_path = parser.ordered_argument(0);
        std::string output_path = parser.ordered_argument(1);
        std::string shader_stage = parser.argument("shader-stage");
        std::string target_lang = parser.argument("target-language");
        
        if (output_path == "")
            output_path = path_without_file(input_path);
        
        std::string file_name = file_name_from_path(input_path);
  
        std::unordered_map<std::string, spirv_compiler::ShaderStage> shader_stage_map =
        {
            { "vertex", spirv_compiler::SHADER_STAGE_VERTEX },
            { "fragment", spirv_compiler::SHADER_STAGE_FRAGMENT },
            { "compute", spirv_compiler::SHADER_STAGE_COMPUTE }
        };
        
        std::unordered_map<std::string, cross_compiler::ShadingLanguage> target_lang_map =
        {
            { "GLSL_ES2", cross_compiler::SHADING_LANGUAGE_GLSL_ES2 },
            { "GLSL_ES3", cross_compiler::SHADING_LANGUAGE_GLSL_ES3 },
            { "GLSL_450", cross_compiler::SHADING_LANGUAGE_GLSL_450 },
            { "GLSL_VK", cross_compiler::SHADING_LANGUAGE_GLSL_VK },
            { "HLSL", cross_compiler::SHADING_LANGUAGE_HLSL },
            { "MSL", cross_compiler::SHADING_LANGUAGE_MSL }
        };
        
        std::string shader_extensions[] =
        {
            "_es2.glsl",
            "_es3.glsl",
            "_450.glsl",
            "_vk.glsl",
            ".hlsl",
            ".metal"
        };
        
        spirv_compiler::ShaderStage stage;
        cross_compiler::ShadingLanguage lang;
        
        if (shader_stage_map.find(shader_stage) == shader_stage_map.end())
        {
            printf("ERROR: Shader stage not specified!\n");
            return 1;
        }
        else
            stage = shader_stage_map[shader_stage];
        
        if (target_lang_map.find(target_lang) == target_lang_map.end())
        {
            printf("ERROR: Target language not specified!\n");
            return 1;
        }
        else
            lang = target_lang_map[target_lang];

        if (spirv_compiler::compile(input_path, spirv))
        {
            std::string output_src;
            
            if (cross_compiler::compile(spirv, lang, output_src))
            {
                std::string write_path = output_path;
                
                if (write_path == "")
                    write_path = write_path + file_name;
                else
                {
                    write_path = write_path + "/";
                    write_path = write_path + file_name;
                }
                
                write_path = write_path + shader_extensions[lang];
                
                std::ofstream out(write_path);
                out << output_src;
                out.close();
                
                return 0;
            }
            
            return 1;
        }
    }
    else
        parser.usage();
    
    return 1;
}
