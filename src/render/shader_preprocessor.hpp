#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "render/shader_compilation_error.hpp"

namespace WingsOfSteel
{

// Where a line of preprocessed code came from. Line numbers are 1-based, as the shader compiler
// reports them.
struct ShaderSourceLine
{
    std::string path;
    uint32_t line{ 0 };
    uint32_t rootLine{ 0 }; // The line of the root shader which brought this one in, through #includes.
};

struct ShaderSourceMap
{
    std::string rootPath;
    std::vector<ShaderSourceLine> lines; // One per line of preprocessed code.

    ShaderSourceLine Resolve(uint32_t lineNumber) const;
};

struct ShaderPreprocessorResult
{
    std::string code;
    ShaderSourceMap sourceMap;
    std::vector<ShaderCompilationError> errors;
};

using OnShaderPreprocessedCallback = std::function<void(const ShaderPreprocessorResult&)>;

// Expands `#include "/shaders/file.wgsl"` directives, which take absolute VFS paths. Each file is
// included at most once per shader, so shared files need no include guards and can't recurse.
// Included files are read through the VFS, so the callback may run on a later frame.
class ShaderPreprocessor
{
public:
    static void Execute(const std::string& path, const std::string& source, OnShaderPreprocessedCallback callback);
};

} // namespace WingsOfSteel
