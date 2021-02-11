#pragma once

#include <shaderc/shaderc.hpp>

#include <string>

std::string GetFileContents(const char* filename);

const std::string CompilationStatusToString(shaderc_compilation_status status);

std::vector<uint32_t> CompileShader(const std::string& path, shaderc_shader_kind kind);