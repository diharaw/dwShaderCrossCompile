#include "spirv_compiler.h"

int main(int argc, char* argv[])
{
    if (argc > 1)
    {
        std::vector<unsigned int> spirv;
    
        if (spirv_compiler::compile(argv[1], spirv))
            return 0;
    }
    
    return 1;
}
