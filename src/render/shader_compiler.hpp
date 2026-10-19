#pragma once

#include <functional>
#include <string>
#include <vector>

#include "core/smart_ptr.hpp"
#include "render/shader_compilation_result.hpp"
#include "render/shader_preprocessor.hpp"

namespace WingsOfSteel
{

DECLARE_SMART_PTR(ResourceShader);
DECLARE_SMART_PTR(ShaderCompilationResult);

using OnShaderCompiledCallback = std::function<void(ShaderCompilationResult*)>;

class ShaderCompiler
{
public:
    ShaderCompiler();
    ~ShaderCompiler();

    // The path is the shader's VFS path, used to report errors in the right file; the label names
    // the shader module.
    void Compile(const std::string& path, const std::string& label, const std::string& code, OnShaderCompiledCallback callback);
    ShaderCompilationResult* GetShaderCompilationResult(uint32_t id) const;

private:
    void CompilePreprocessed(uint32_t id, const std::string& label, const ShaderPreprocessorResult& preprocessed);

    std::vector<ShaderCompilationResultUniquePtr> m_Results;
};

} // namespace WingsOfSteel