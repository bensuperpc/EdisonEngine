#include "shadercache.h"

#include "shaderprogram.h"

#include <algorithm>
#include <boost/algorithm/string/join.hpp>

namespace render::scene
{
namespace
{
std::string
  makeId(const std::filesystem::path& vshPath, const std::filesystem::path& fshPath, std::vector<std::string> defines)
{
  std::string id = vshPath.string();
  id += ';';
  id += fshPath.string();
  id += ';';
  std::sort(defines.begin(), defines.end());
  id += boost::algorithm::join(defines, ";");
  return id;
}
std::string makeId(const std::filesystem::path& shaderPath, std::vector<std::string> defines)
{
  std::string id = shaderPath.string();
  id += ';';
  std::sort(defines.begin(), defines.end());
  id += boost::algorithm::join(defines, ";");
  return id;
}
} // namespace

gsl::not_null<std::shared_ptr<ShaderProgram>> ShaderCache::get(const std::filesystem::path& vshPath,
                                                               const std::filesystem::path& fshPath,
                                                               const std::vector<std::string>& defines)
{
  const auto programId = makeId(vshPath, fshPath, defines);
  const auto it = m_programs.find(programId);
  if(it != m_programs.end())
    return it->second;

  auto shader = ShaderProgram::createFromFile(
    programId, makeId(vshPath, defines), m_root / vshPath, makeId(fshPath, defines), m_root / fshPath, defines);
  m_programs.emplace(programId, shader);
  return shader;
}
} // namespace render::scene
