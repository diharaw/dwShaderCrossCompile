#include "spirv_compiler.h"
#include "cross_compiler.h"

#include <fstream>

int main(int argc, char* argv[])
{
    if (argc > 1)
    {
        std::vector<unsigned int> spirv;
    
        if (spirv_compiler::compile(argv[1], spirv))
        {
            std::string output_src;
            
            if (cross_compiler::compile(spirv, cross_compiler::SHADING_LANGUAGE_MSL, output_src))
            {
                std::ofstream out("test.metal");
                out << output_src;
                out.close();
                
                return 0;
            }
            
            return 1;
        }
    }
    
    return 1;
}
