#include "utils.h"

#include <cassert>
#include <cerrno>
#include <fstream>
#include <iostream>

std::string GetFileContents(const char* filename)
{
    std::ifstream in(filename, std::ios::in | std::ios::binary);
    if (in) {
        std::string contents;
        in.seekg(0, std::ios::end);
        contents.resize(in.tellg());
        in.seekg(0, std::ios::beg);
        in.read(&contents[0], contents.size());
        in.close();
        return (contents);
    }
    throw(errno);
}

const std::string CompilationStatusToString(shaderc_compilation_status status)
{
    switch (status) {
    case shaderc_compilation_status_success:
        return "Success";
        break;
    case shaderc_compilation_status_invalid_stage:
        return "Invalid Stage Error";
        break;
    case shaderc_compilation_status_compilation_error:
        return "Compilation Error";
        break;
    case shaderc_compilation_status_internal_error:
        return "Internal Error";
        break;
    case shaderc_compilation_status_null_result_object:
        return "Null Result Error";
        break;
    case shaderc_compilation_status_invalid_assembly:
        return "Invalid Assembly Error";
        break;
    case shaderc_compilation_status_validation_error:
        return "Validation Error";
        break;
    case shaderc_compilation_status_transformation_error:
        return "Transformation Error";
        break;
    case shaderc_compilation_status_configuration_error:
        return "Configuration Error";
        break;
    default:
        return "Unknown Error";
        break;
    }
}

std::vector<uint32_t> CompileShader(const std::string& path, shaderc_shader_kind kind)
{
    std::string shader_source = GetFileContents(path.c_str());
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(shader_source, kind, path.c_str(), options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        //handle errors
        shaderc_compilation_status status = result.GetCompilationStatus();
        std::string error_type = CompilationStatusToString(status);
        std::cerr << error_type << ":\n"
                  << result.GetErrorMessage() << std::endl;
        assert(false);
    }
    std::vector<uint32_t> vertexSPRV;
    vertexSPRV.assign(result.cbegin(), result.cend());
    return vertexSPRV;
}