#pragma once

#include <cstdint>
#include <string>

namespace WingsOfSteel
{

class ShaderCompilationError
{
public:
    // The path and line number are those of the file the error is in, which may be an #include.
    // The root line number is the line of the shader being compiled which led to it.
    ShaderCompilationError(const std::string& message, const std::string& path, uint32_t lineNumber, uint32_t linePosition, uint32_t rootLineNumber)
        : m_Message(message)
        , m_Path(path)
        , m_LineNumber(lineNumber)
        , m_LinePosition(linePosition)
        , m_RootLineNumber(rootLineNumber)
    {
    }

    ~ShaderCompilationError() {}

    inline const std::string& GetMessage() const { return m_Message; }
    inline const std::string& GetPath() const { return m_Path; }
    inline uint32_t GetLineNumber() const { return m_LineNumber; }
    inline uint32_t GetLinePosition() const { return m_LinePosition; }
    inline uint32_t GetRootLineNumber() const { return m_RootLineNumber; }

private:
    std::string m_Message;
    std::string m_Path;
    uint32_t m_LineNumber;
    uint32_t m_LinePosition;
    uint32_t m_RootLineNumber;
};

} // namespace WingsOfSteel
