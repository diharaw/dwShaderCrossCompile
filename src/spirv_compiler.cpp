#include "spirv_compiler.h"
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <../StandAlone/ResourceLimits.h>
#include <Worklist.h>
#include <DirStackFileIncluder.h>
#include <ShHandle.h>
#include <revision.h>
#include <../Public/ShaderLang.h>
#include <GlslangToSpv.h>
#include <GLSL.std.450.h>
#include <doc.h>
#include <disassemble.h>

#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cmath>
#include <array>
#include <map>
#include <memory>
#include <thread>

#include <../OSDependent/osinclude.h>

namespace spirv_compiler
{
    EShLanguage kShaderStageMap[] =
    {
        EShLangVertex,
        EShLangFragment,
        EShLangCompute
    };
    
    extern "C" {
        SH_IMPORT_EXPORT void ShOutputHtml();
    }
    
    // Command-line options
    enum TOptions {
        EOptionNone                 = 0,
        EOptionIntermediate         = (1 <<  0),
        EOptionSuppressInfolog      = (1 <<  1),
        EOptionMemoryLeakMode       = (1 <<  2),
        EOptionRelaxedErrors        = (1 <<  3),
        EOptionGiveWarnings         = (1 <<  4),
        EOptionLinkProgram          = (1 <<  5),
        EOptionMultiThreaded        = (1 <<  6),
        EOptionDumpConfig           = (1 <<  7),
        EOptionDumpReflection       = (1 <<  8),
        EOptionSuppressWarnings     = (1 <<  9),
        EOptionDumpVersions         = (1 << 10),
        EOptionSpv                  = (1 << 11),
        EOptionHumanReadableSpv     = (1 << 12),
        EOptionVulkanRules          = (1 << 13),
        EOptionDefaultDesktop       = (1 << 14),
        EOptionOutputPreprocessed   = (1 << 15),
        EOptionOutputHexadecimal    = (1 << 16),
        EOptionReadHlsl             = (1 << 17),
        EOptionCascadingErrors      = (1 << 18),
        EOptionAutoMapBindings      = (1 << 19),
        EOptionFlattenUniformArrays = (1 << 20),
        EOptionNoStorageFormat      = (1 << 21),
        EOptionKeepUncalled         = (1 << 22),
        EOptionHlslOffsets          = (1 << 23),
        EOptionHlslIoMapping        = (1 << 24),
        EOptionAutoMapLocations     = (1 << 25),
        EOptionDebug                = (1 << 26),
        EOptionStdin                = (1 << 27),
        EOptionOptimizeDisable      = (1 << 28),
        EOptionOptimizeSize         = (1 << 29),
        EOptionInvertY              = (1 << 30),
        EOptionDumpBareVersion      = (1 << 31),
    };
    bool targetHlslFunctionality1 = false;
    bool SpvToolsDisassembler = false;
    bool SpvToolsValidate = false;
    
    //
    // Return codes from main/exit().
    //
    enum TFailCode {
        ESuccess = 0,
        EFailUsage,
        EFailCompile,
        EFailLink,
        EFailCompilerCreate,
        EFailThreadCreate,
        EFailLinkerCreate
    };
    
    //
    // Forward declarations.
    //
    void CompileFile(const char* fileName, ShHandle);
    char* ReadFileData(const char* fileName);
    void FreeFileData(char* data);
    void InfoLogMsg(const char* msg, const char* name, const int num);
    
    // Globally track if any compile or link failure.
    bool CompileFailed = false;
    bool LinkFailed = false;
    
    // array of unique places to leave the shader names and infologs for the asynchronous compiles
    std::vector<std::unique_ptr<glslang::TWorkItem>> WorkItems;
    
    TBuiltInResource Resources;
    
    int ReflectOptions = EShReflectionDefault;
    int Options = 0;
    const char* ExecutableName = nullptr;
    const char* binaryFileName = nullptr;
    const char* entryPointName = nullptr;
    const char* sourceEntryPointName = nullptr;
    const char* shaderStageName = nullptr;
    const char* variableName = nullptr;
    bool HlslEnable16BitTypes = false;
    bool HlslDX9compatible = false;
    bool DumpBuiltinSymbols = false;
    std::vector<std::string> IncludeDirectoryList;
    
    // Source environment
    // (source 'Client' is currently the same as target 'Client')
    int ClientInputSemanticsVersion = 100;
    
    // Target environment
    glslang::EShClient Client = glslang::EShClientNone;  // will stay EShClientNone if only validating
    glslang::EShTargetClientVersion ClientVersion;       // not valid until Client is set
    glslang::EShTargetLanguage TargetLanguage = glslang::EShTargetNone;
    glslang::EShTargetLanguageVersion TargetVersion;     // not valid until TargetLanguage is set
    
    std::vector<std::string> Processes;                     // what should be recorded by OpModuleProcessed, or equivalent
    
    // Per descriptor-set binding base data
    typedef std::map<unsigned int, unsigned int> TPerSetBaseBinding;
    
    std::vector<std::pair<std::string, int>> uniformLocationOverrides;
    int uniformBase = 0;
    
    std::array<std::array<unsigned int, EShLangCount>, glslang::EResCount> baseBinding;
    std::array<std::array<TPerSetBaseBinding, EShLangCount>, glslang::EResCount> baseBindingForSet;
    std::array<std::vector<std::string>, EShLangCount> baseResourceSetBinding;
    
    // Add things like "#define ..." to a preamble to use in the beginning of the shader.
    class TPreamble {
    public:
        TPreamble() { }
        
        bool isSet() const { return text.size() > 0; }
        const char* get() const { return text.c_str(); }
        
        // #define...
        void addDef(std::string def)
        {
            text.append("#define ");
            fixLine(def);
            
            Processes.push_back("D");
            Processes.back().append(def);
            
            // The first "=" needs to turn into a space
            const size_t equal = def.find_first_of("=");
            if (equal != def.npos)
                def[equal] = ' ';
            
            text.append(def);
            text.append("\n");
        }
        
        // #undef...
        void addUndef(std::string undef)
        {
            text.append("#undef ");
            fixLine(undef);
            
            Processes.push_back("U");
            Processes.back().append(undef);
            
            text.append(undef);
            text.append("\n");
        }
        
    protected:
        void fixLine(std::string& line)
        {
            // Can't go past a newline in the line
            const size_t end = line.find_first_of("\n");
            if (end != line.npos)
                line = line.substr(0, end);
        }
        
        std::string text;  // contents of preamble
    };
    
    TPreamble UserPreamble;
    
    //
    // Give error and exit with failure code.
    //
    void Error(const char* message)
    {
        fprintf(stderr, "%s: Error %s (use -h for usage)\n", ExecutableName, message);
        exit(EFailUsage);
    }
    
    // Outputs the given string, but only if it is non-null and non-empty.
    // This prevents erroneous newlines from appearing.
    void PutsIfNonEmpty(const char* str)
    {
        if (str && str[0]) {
            puts(str);
        }
    }
    
    // Outputs the given string to stderr, but only if it is non-null and non-empty.
    // This prevents erroneous newlines from appearing.
    void StderrIfNonEmpty(const char* str)
    {
        if (str && str[0])
            fprintf(stderr, "%s\n", str);
    }
    
    // Simple bundling of what makes a compilation unit for ease in passing around,
    // and separation of handling file IO versus API (programmatic) compilation.
    struct ShaderCompUnit {
        EShLanguage stage;
        static const int maxCount = 1;
        int count;                          // live number of strings/names
        const char* text[maxCount];         // memory owned/managed externally
        std::string fileName[maxCount];     // hold's the memory, but...
        const char* fileNameList[maxCount]; // downstream interface wants pointers
        
        ShaderCompUnit(EShLanguage stage) : stage(stage), count(0) { }
        
        ShaderCompUnit(const ShaderCompUnit& rhs)
        {
            stage = rhs.stage;
            count = rhs.count;
            for (int i = 0; i < count; ++i) {
                fileName[i] = rhs.fileName[i];
                text[i] = rhs.text[i];
                fileNameList[i] = rhs.fileName[i].c_str();
            }
        }
        
        void addString(std::string& ifileName, const char* itext)
        {
            assert(count < maxCount);
            fileName[count] = ifileName;
            text[count] = itext;
            fileNameList[count] = fileName[count].c_str();
            ++count;
        }
    };

    //
    // For linking mode: Will independently parse each compilation unit, but then put them
    // in the same program and link them together, making at most one linked module per
    // pipeline stage.
    //
    // Uses the new C++ interface instead of the old handle-based interface.
    //
    
    void CompileAndLinkShaderUnits(const ShaderCompUnit& compUnit, std::vector<unsigned int>& spirv)
    {
        EShMessages messages = EShMsgDefault;
        
        //
        // Per-shader processing...
        //
        
        glslang::TProgram& program = *new glslang::TProgram;
        
        glslang::TShader* shader = new glslang::TShader(compUnit.stage);
        shader->setStringsWithLengthsAndNames(compUnit.text, NULL, compUnit.fileNameList, compUnit.count);
        if (entryPointName)
            shader->setEntryPoint(entryPointName);
        if (sourceEntryPointName)
        {
            if (entryPointName == nullptr)
                printf("Warning: Changing source entry point name without setting an entry-point name.\n"
                       "Use '-e <name>'.\n");
            shader->setSourceEntryPoint(sourceEntryPointName);
        }
        if (UserPreamble.isSet())
            shader->setPreamble(UserPreamble.get());
        shader->addProcesses(Processes);
        
        // Set IO mapper binding shift values
        for (int r = 0; r < glslang::EResCount; ++r)
        {
            const glslang::TResourceType res = glslang::TResourceType(r);
            
            // Set base bindings
            shader->setShiftBinding(res, baseBinding[res][compUnit.stage]);
            
            // Set bindings for particular resource sets
            // TODO: use a range based for loop here, when available in all environments.
            for (auto i = baseBindingForSet[res][compUnit.stage].begin();
                 i != baseBindingForSet[res][compUnit.stage].end(); ++i)
                shader->setShiftBindingForSet(res, i->second, i->first);
        }
        
        shader->setFlattenUniformArrays((Options & EOptionFlattenUniformArrays) != 0);
        shader->setNoStorageFormat((Options & EOptionNoStorageFormat) != 0);
        shader->setResourceSetBinding(baseResourceSetBinding[compUnit.stage]);
        
        if (Options & EOptionHlslIoMapping)
            shader->setHlslIoMapping(true);
        
        if (Options & EOptionAutoMapBindings)
            shader->setAutoMapBindings(true);
        
        if (Options & EOptionAutoMapLocations)
            shader->setAutoMapLocations(true);
        
        if (Options & EOptionInvertY)
            shader->setInvertY(true);
        
        for (auto& uniOverride : uniformLocationOverrides) {
            shader->addUniformLocationOverride(uniOverride.first.c_str(),
                                               uniOverride.second);
        }
        
        shader->setUniformLocationBase(uniformBase);
        
        // Set up the environment, some subsettings take precedence over earlier
        // ways of setting things.
        if (Options & EOptionSpv)
        {
            shader->setEnvInput((Options & EOptionReadHlsl) ? glslang::EShSourceHlsl
                                : glslang::EShSourceGlsl,
                                compUnit.stage, Client, ClientInputSemanticsVersion);
            shader->setEnvClient(Client, ClientVersion);
            shader->setEnvTarget(TargetLanguage, TargetVersion);
            if (targetHlslFunctionality1)
                shader->setEnvTargetHlslFunctionality1();
        }
        
        const int defaultVersion = Options & EOptionDefaultDesktop ? 110 : 100;
        
        DirStackFileIncluder includer;
        
        std::for_each(IncludeDirectoryList.rbegin(), IncludeDirectoryList.rend(), [&includer](const std::string& dir)
                      {
                          includer.pushExternalLocalDirectory(dir);
                      });
        
        if (Options & EOptionOutputPreprocessed)
        {
            std::string str;
            if (shader->preprocess(&Resources, defaultVersion, ENoProfile, false, false, messages, &str, includer))
                PutsIfNonEmpty(str.c_str());
            else
                CompileFailed = true;
            
            StderrIfNonEmpty(shader->getInfoLog());
            StderrIfNonEmpty(shader->getInfoDebugLog());
        }
        
        if (! shader->parse(&Resources, defaultVersion, false, messages, includer))
            CompileFailed = true;
        
        program.addShader(shader);
        
        if (! (Options & EOptionSuppressInfolog) &&
            ! (Options & EOptionMemoryLeakMode)) {
            PutsIfNonEmpty(compUnit.fileName[0].c_str());
            PutsIfNonEmpty(shader->getInfoLog());
            PutsIfNonEmpty(shader->getInfoDebugLog());
        }
        
        //
        // Program-level processing...
        //
        
        // Link
        if (! (Options & EOptionOutputPreprocessed) && ! program.link(messages))
            LinkFailed = true;
        
        // Map IO
        if (Options & EOptionSpv) {
            if (!program.mapIO())
                LinkFailed = true;
        }
        
        // Reflect
        if (Options & EOptionDumpReflection) {
            program.buildReflection(ReflectOptions);
            program.dumpReflection();
        }
        
        // Dump SPIR-V
        if (CompileFailed || LinkFailed)
            printf("SPIR-V is not generated for failed compile or link\n");
        else {
            for (int stage = 0; stage < EShLangCount; ++stage)
            {
                if (program.getIntermediate((EShLanguage)stage))
                {
                    std::string warningsErrors;
                    spv::SpvBuildLogger logger;
                    glslang::SpvOptions spvOptions;
                    
                    if (Options & EOptionDebug)
                        spvOptions.generateDebugInfo = true;
                    
                    spvOptions.disableOptimizer = (Options & EOptionOptimizeDisable) != 0;
                    spvOptions.optimizeSize = (Options & EOptionOptimizeSize) != 0;
                    spvOptions.disassemble = SpvToolsDisassembler;
                    spvOptions.validate = SpvToolsValidate;
                    
                    glslang::GlslangToSpv(*program.getIntermediate((EShLanguage)stage), spirv, &logger, &spvOptions);
                }
            }
        }
        
        // Free everything up, program has to go before the shaders
        // because it might have merged stuff from the shaders, and
        // the stuff from the shaders has to have its destructors called
        // before the pools holding the memory in the shaders is freed.
        delete &program;
        delete shader;
    }
    
    //
    // Do file IO part of compile and link, handing off the pure
    // API/programmatic mode to CompileAndLinkShaderUnits(), which can
    // be put in a loop for testing memory footprint and performance.
    //
    // This is just for linking mode: meaning all the shaders will be put into the
    // the same program linked together.
    //
    // This means there are a limited number of work items (not multi-threading mode)
    // and that the point is testing at the linking level. Hence, to enable
    // performance and memory testing, the actual compile/link can be put in
    // a loop, independent of processing the work items and file IO.
    //
    bool CompileAndLinkShaderFiles(std::string path, ShaderStage stage, std::vector<unsigned int>& spirv)
    {
        ShaderCompUnit compUnit(kShaderStageMap[stage]);
        char* fileText = ReadFileData(path.c_str());
        
        if (fileText == nullptr)
        {
            printf("Failed to read shader source: %s\n", path.c_str());
            return false;
        }
        
        compUnit.addString(path, fileText);
        
        CompileAndLinkShaderUnits(compUnit, spirv);
        
        // free memory from ReadFileData, which got stored in a const char*
        // as the first string above
        FreeFileData(const_cast<char*>(compUnit.text[0]));
        
        return true;
    }
    
#if !defined _MSC_VER && !defined MINGW_HAS_SECURE_API
    
#include <errno.h>
    
    int fopen_s(
                FILE** pFile,
                const char* filename,
                const char* mode
                )
    {
        if (!pFile || !filename || !mode) {
            return EINVAL;
        }
        
        FILE* f = fopen(filename, mode);
        if (! f) {
            if (errno != 0) {
                return errno;
            } else {
                return ENOENT;
            }
        }
        *pFile = f;
        
        return 0;
    }
    
#endif
    
    //
    //   Malloc a string of sufficient size and read a string into it.
    //
    char* ReadFileData(const char* fileName)
    {
        FILE *in = nullptr;
        int errorCode = fopen_s(&in, fileName, "r");
        if (errorCode || in == nullptr)
            Error("unable to open input file");
        
        int count = 0;
        while (fgetc(in) != EOF)
            count++;
        
        fseek(in, 0, SEEK_SET);
        
        char* return_data = (char*)malloc(count + 1);  // freed in FreeFileData()
        if ((int)fread(return_data, 1, count, in) != count) {
            free(return_data);
            Error("can't read input file");
        }
        
        return_data[count] = '\0';
        fclose(in);
        
        return return_data;
    }
    
    void FreeFileData(char* data)
    {
        free(data);
    }

    bool compile(const std::string& src, ShaderStage stage, std::vector<unsigned int>& spirv, bool vulkan_glsl)
    {
        Resources = glslang::DefaultTBuiltInResource;

		if (vulkan_glsl)
		{
			ClientVersion = glslang::EShTargetVulkan_1_1;
			Client = glslang::EShClientVulkan;
		}
		else
		{
			if (Client == glslang::EShClientNone)
				ClientVersion = glslang::EShTargetOpenGL_450;
			Client = glslang::EShClientOpenGL;
		}
        

        Options |= EOptionSpv;
        Options |= EOptionLinkProgram;
        // undo a -H default to Vulkan
        Options &= ~EOptionVulkanRules;
        
        ClientInputSemanticsVersion = 450;
        
        // rationalize client and target language
        if (TargetLanguage == glslang::EShTargetNone)
        {
            switch (ClientVersion)
            {
                case glslang::EShTargetVulkan_1_0:
                    TargetLanguage = glslang::EShTargetSpv;
                    TargetVersion = glslang::EShTargetSpv_1_0;
                    break;
                case glslang::EShTargetVulkan_1_1:
                    TargetLanguage = glslang::EShTargetSpv;
                    TargetVersion = glslang::EShTargetSpv_1_3;
                    break;
                case glslang::EShTargetOpenGL_450:
                    TargetLanguage = glslang::EShTargetSpv;
                    TargetVersion = glslang::EShTargetSpv_1_0;
                    break;
                default:
                    break;
            }
        }
        
        glslang::InitializeProcess();
        CompileAndLinkShaderFiles(src, stage, spirv);
        glslang::FinalizeProcess();
        
        if (CompileFailed)
            return false;
        if (LinkFailed)
            return false;
        
        return true;
    }
}
