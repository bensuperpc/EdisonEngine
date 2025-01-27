#include "shaderprogram.h"

#include "util/helpers.h"

#include <algorithm>
#include <array>
#include <boost/algorithm/string/join.hpp>
#include <boost/log/trivial.hpp>
#include <boost/throw_exception.hpp>
#include <cstddef>
#include <fstream>
#include <gl/program.h>
#include <gl/renderstate.h>
#include <gl/shader.h>
#include <gslu.h>
#include <iterator>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace render::scene
{
namespace
{
std::string readAll(const std::filesystem::path& filePath)
{
  std::ifstream stream{util::ensureFileExists(filePath), std::ios::in};
  std::noskipws(stream);
  return std::string{std::istream_iterator<char>{stream}, std::istream_iterator<char>{}};
}

std::string replaceDefines(const std::vector<std::string>& defines, bool isInput)
{
  std::string out;
  if(!defines.empty())
  {
    out += std::string("#define ") + boost::algorithm::join(defines, "\n#define ");
  }

  out += isInput ? "\n#define IN_OUT in\n" : "\n#define IN_OUT out\n";

  return out;
}

void replaceIncludes(const std::filesystem::path& filepath,
                     const std::string& source,
                     std::string& out,
                     std::set<std::filesystem::path>& included)
{
  included.emplace(filepath);

  // Replace the #include "foo.bar" with the sourced file contents of "filepath/foo.bar"
  size_t headPos = 0;
  size_t line = 1;
  while(headPos < source.length())
  {
    const auto lastPos = headPos;
    if(headPos == 0)
    {
      // find the first "#include"
      headPos = source.find("#include");
    }
    else
    {
      // find the next "#include"
      headPos = source.find("#include", headPos + 1);
    }

    // If "#include" is found
    if(headPos != std::string::npos)
    {
      // append from our last position for the length (head - last position)
      {
        const auto part = source.substr(lastPos, headPos - lastPos);
        out.append(part);
        line += std::count(part.begin(), part.end(), '\n');
      }

      // find the start quote "
      const size_t startQuote = source.find('"', headPos) + 1;
      if(startQuote == std::string::npos)
      {
        // We have started an "#include" but missing the leading quote "
        BOOST_LOG_TRIVIAL(error) << "Compile failed for shader '" << filepath << "' missing leading \".";
        return;
      }
      // find the end quote "
      const size_t endQuote = source.find('"', startQuote);
      if(endQuote == std::string::npos)
      {
        // We have a start quote but missing the trailing quote "
        BOOST_LOG_TRIVIAL(error) << "Compile failed for shader '" << filepath << "' missing trailing \".";
        return;
      }

      // jump the head position past the end quote
      headPos = endQuote + 1;

      // File path to include and 'stitch' in the value in the quotes to the file path and source it.
      const size_t len = endQuote - startQuote;
      const auto includePath = filepath.parent_path() / std::filesystem::path{source.substr(startQuote, len)};
      if(included.count(includePath) > 0)
      {
        continue;
      }

      std::string includedSource = readAll(includePath);
      if(includedSource.empty())
      {
        BOOST_LOG_TRIVIAL(error) << "Compile failed for shader '" << filepath << "': failed to include'" << includePath
                                 << "'";
        return;
      }
      // Valid file so lets attempt to see if we need to append anything to it too (recurse...)
      replaceIncludes(includePath, includedSource, out, included);
    }
    else
    {
      // Append the remaining
      out.append(source, lastPos);
      break;
    }
  }
}
} // namespace

ShaderProgram::ShaderProgram(const std::string_view& label)
    : m_handle{label}
    , m_id{label}
{
}

ShaderProgram::~ShaderProgram() = default;

gsl::not_null<std::shared_ptr<ShaderProgram>> ShaderProgram::createFromFile(const std::string& programId,
                                                                            const std::string& vshId,
                                                                            const std::filesystem::path& vshPath,
                                                                            const std::string& fshId,
                                                                            const std::filesystem::path& fshPath,
                                                                            const std::vector<std::string>& defines)
{
  util::ensureFileExists(vshPath);
  util::ensureFileExists(fshPath);

  const std::string vshSource = readAll(vshPath);
  if(vshSource.empty())
  {
    BOOST_LOG_TRIVIAL(error) << "Failed to read vertex shader from file '" << vshPath << "'.";
    BOOST_THROW_EXCEPTION(std::runtime_error("Failed to create shader from sources"));
  }
  const std::string fshSource = readAll(fshPath);
  if(fshSource.empty())
  {
    BOOST_LOG_TRIVIAL(error) << "Failed to read fragment shader from file '" << fshPath << "'.";
    BOOST_THROW_EXCEPTION(std::runtime_error("Failed to create shader from sources"));
  }

  return createFromSource(programId, vshId, vshPath, vshSource, fshId, fshPath, fshSource, defines);
}

gsl::not_null<std::shared_ptr<ShaderProgram>> ShaderProgram::createFromSource(const std::string& programId,
                                                                              const std::string& vshId,
                                                                              const std::filesystem::path& vshPath,
                                                                              const std::string& vshSource,
                                                                              const std::string& fshId,
                                                                              const std::filesystem::path& fshPath,
                                                                              const std::string& fshSource,
                                                                              const std::vector<std::string>& defines)
{
  static constexpr size_t SHADER_SOURCE_LENGTH = 3;
  std::array<gsl::czstring, SHADER_SOURCE_LENGTH> shaderSource{nullptr};
  shaderSource[0] = "#version 450\n#extension GL_ARB_bindless_texture : require\n";

  std::string vshSourceStr;
  if(!vshPath.empty())
  {
    // Replace the #include "foo.bar" with the sources that come from file paths
    std::set<std::filesystem::path> included;
    replaceIncludes(vshPath, vshSource, vshSourceStr, included);
  }
  shaderSource[2] = !vshPath.empty() ? vshSourceStr.c_str() : vshSource.c_str();

  std::string definesStr = replaceDefines(defines, false);
  shaderSource[1] = definesStr.c_str();
  gl::VertexShader vertexShader{shaderSource, vshId};

  // Compile the fragment shader.
  std::string fshSourceStr;
  if(!fshPath.empty())
  {
    // Replace the #include "foo.bar" with the sources that come from file paths
    std::set<std::filesystem::path> included;
    replaceIncludes(std::filesystem::path{fshPath}, fshSource, fshSourceStr, included);
  }
  shaderSource[2] = !fshPath.empty() ? fshSourceStr.c_str() : fshSource.c_str();

  definesStr = replaceDefines(defines, true);
  shaderSource[1] = definesStr.c_str();
  gl::FragmentShader fragmentShader{shaderSource, fshId};

  auto shaderProgram = gslu::make_nn_shared<ShaderProgram>(programId);
  shaderProgram->m_handle.attach(vertexShader);
  shaderProgram->m_handle.attach(fragmentShader);
  shaderProgram->m_handle.link();

  if(!shaderProgram->m_handle.getLinkStatus())
  {
    BOOST_LOG_TRIVIAL(error) << "Linking program failed (" << (vshPath.empty() ? "<none>" : vshPath) << ","
                             << (fshPath.empty() ? "<none>" : fshPath) << "): " << shaderProgram->m_handle.getInfoLog();

    BOOST_THROW_EXCEPTION(std::runtime_error("Failed to link program"));
  }

  BOOST_LOG_TRIVIAL(debug) << "Program vertex=" << vshPath << " fragment=" << fshPath
                           << " defines=" << boost::algorithm::join(defines, ";");

  for(auto&& input : shaderProgram->m_handle.getInputs())
  {
    if(input.getLocation() < 0)
      continue; // only accept directly accessible uniforms

    BOOST_LOG_TRIVIAL(debug) << "  input " << input.getName() << ", location=" << input.getLocation();

    shaderProgram->m_vertexAttributes.emplace(input.getName(), std::move(input));
  }

  for(auto&& uniform : shaderProgram->m_handle.getUniforms())
  {
    if(uniform.getLocation() < 0)
      continue; // only accept directly accessible uniforms

    BOOST_LOG_TRIVIAL(debug) << "  uniform " << uniform.getName() << ", location=" << uniform.getLocation();

    shaderProgram->m_uniforms.emplace(uniform.getName(), std::move(uniform));
  }

  for(auto&& ub : shaderProgram->m_handle.getUniformBlocks())
  {
    BOOST_LOG_TRIVIAL(debug) << "  uniform block " << ub.getName() << ", binding=" << ub.getBinding();
    shaderProgram->m_uniformBlocks.emplace(ub.getName(), std::move(ub));
  }

  for(auto&& ssb : shaderProgram->m_handle.getShaderStorageBlocks())
  {
    BOOST_LOG_TRIVIAL(debug) << "  shader storage block " << ssb.getName() << ", binding=" << ssb.getBinding();
    shaderProgram->m_shaderStorageBlocks.emplace(ssb.getName(), std::move(ssb));
  }

  for(auto&& output : shaderProgram->m_handle.getOutputs())
  {
    BOOST_LOG_TRIVIAL(debug) << "  output " << output.getName() << ", location=" << output.getLocation();
  }

  return shaderProgram;
}

void ShaderProgram::bind() const
{
  gl::RenderState::getWantedState().setProgram(m_handle.getHandle());
}
} // namespace render::scene
