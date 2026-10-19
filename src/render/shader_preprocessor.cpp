#include <memory>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "render/shader_preprocessor.hpp"

#include "pandora.hpp"
#include "vfs/vfs.hpp"

namespace WingsOfSteel
{

namespace
{

    struct PreprocessorState
    {
        std::string rootPath;
        std::unordered_map<std::string, std::vector<std::string>> files; // Every file read so far, as lines.
        std::unordered_set<std::string> requested;
        uint32_t pending{ 0 };
        OnShaderPreprocessedCallback callback;
    };

    using PreprocessorStateSharedPtr = std::shared_ptr<PreprocessorState>;

    std::vector<std::string> SplitLines(const std::string& source)
    {
        std::vector<std::string> lines;
        std::istringstream stream(source);
        std::string line;
        while (std::getline(stream, line))
        {
            lines.push_back(line);
        }
        return lines;
    }

    // Returns nullopt if the line isn't an #include directive, and an empty path if it is a malformed one.
    std::optional<std::string> ParseInclude(const std::string& line)
    {
        static const std::string kDirective("#include");
        const size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos || line.compare(start, kDirective.size(), kDirective) != 0)
        {
            return std::nullopt;
        }

        const size_t open = line.find('"', start + kDirective.size());
        const size_t close = (open == std::string::npos) ? std::string::npos : line.find('"', open + 1);
        if (close == std::string::npos || line[open + 1] != '/')
        {
            return std::string();
        }

        return line.substr(open + 1, close - open - 1);
    }

    void Finish(const PreprocessorStateSharedPtr& pState);

    void ReadIncludes(const PreprocessorStateSharedPtr& pState, const std::string& path)
    {
        for (const std::string& line : pState->files[path])
        {
            const std::optional<std::string> include = ParseInclude(line);
            if (!include.has_value() || include->empty() || !pState->requested.insert(include.value()).second)
            {
                continue;
            }

            // Native reads complete before FileRead returns, which is why Execute holds a pending read
            // of its own until everything has been requested.
            pState->pending++;
            const std::string includePath = include.value();
            GetVFS()->FileRead(includePath, [pState, includePath](FileReadResult result, FileSharedPtr pFile) {
                // A file that can't be read is left out of `files`, and reported where it is included.
                if (result == FileReadResult::Ok)
                {
                    pState->files[includePath] = SplitLines(std::string(pFile->GetData().data(), pFile->GetData().size()));
                    ReadIncludes(pState, includePath);
                }

                if (--pState->pending == 0)
                {
                    Finish(pState);
                }
            });
        }
    }

    void Expand(const PreprocessorState& state, const std::string& path, uint32_t rootLine, std::unordered_set<std::string>& included, ShaderPreprocessorResult& result)
    {
        const std::vector<std::string>& lines = state.files.at(path);
        for (uint32_t i = 0; i < lines.size(); i++)
        {
            const uint32_t lineNumber = i + 1;
            const uint32_t currentRootLine = (path == state.rootPath) ? lineNumber : rootLine;
            const std::optional<std::string> include = ParseInclude(lines[i]);
            if (!include.has_value())
            {
                result.code += lines[i];
                result.code += '\n';
                result.sourceMap.lines.push_back({ .path = path, .line = lineNumber, .rootLine = currentRootLine });
            }
            else if (include->empty())
            {
                result.errors.emplace_back("Malformed #include, expected #include \"/absolute/path.wgsl\".", path, lineNumber, 1, currentRootLine);
            }
            else if (!state.files.contains(include.value()))
            {
                result.errors.emplace_back("Failed to read included file '" + include.value() + "'.", path, lineNumber, 1, currentRootLine);
            }
            else if (included.insert(include.value()).second)
            {
                Expand(state, include.value(), currentRootLine, included, result);
            }
        }
    }

    void Finish(const PreprocessorStateSharedPtr& pState)
    {
        ShaderPreprocessorResult result;
        result.sourceMap.rootPath = pState->rootPath;
        std::unordered_set<std::string> included{ pState->rootPath };
        Expand(*pState, pState->rootPath, 0, included, result);
        pState->callback(result);
    }

} // namespace

ShaderSourceLine ShaderSourceMap::Resolve(uint32_t lineNumber) const
{
    if (lineNumber == 0 || lineNumber > lines.size())
    {
        return { .path = rootPath, .line = lineNumber, .rootLine = lineNumber };
    }
    return lines[lineNumber - 1];
}

void ShaderPreprocessor::Execute(const std::string& path, const std::string& source, OnShaderPreprocessedCallback callback)
{
    PreprocessorStateSharedPtr pState = std::make_shared<PreprocessorState>();
    pState->rootPath = path;
    pState->files[path] = SplitLines(source);
    pState->requested.insert(path);
    pState->callback = std::move(callback);

    pState->pending = 1;
    ReadIncludes(pState, path);
    if (--pState->pending == 0)
    {
        Finish(pState);
    }
}

} // namespace WingsOfSteel
