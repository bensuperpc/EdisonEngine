#pragma once

#include <boost/container/flat_map.hpp>
#include <boost/container/vector.hpp>
#include <filesystem>
#include <gl/program.h>
#include <gsl/gsl-lite.hpp>
#include <memory>
#include <string>
#include <vector>

namespace render::scene
{
class ShaderProgram
{
public:
  explicit ShaderProgram(const std::string_view& label);

  ShaderProgram(const ShaderProgram&) = delete;
  ShaderProgram(ShaderProgram&&) = delete;
  ShaderProgram& operator=(const ShaderProgram&) = delete;
  ShaderProgram& operator=(ShaderProgram&&) = delete;

  ~ShaderProgram();

  static gsl::not_null<std::shared_ptr<ShaderProgram>> createFromFile(const std::string& programId,
                                                                      const std::string& vshId,
                                                                      const std::filesystem::path& vshPath,
                                                                      const std::string& fshId,
                                                                      const std::filesystem::path& fshPath,
                                                                      const std::vector<std::string>& defines);

  [[nodiscard]] const std::string& getId() const
  {
    return m_id;
  }

  [[nodiscard]] const gl::Uniform* findUniform(const std::string& name) const
  {
    return find(m_uniforms, name);
  }

  gl::Uniform* findUniform(const std::string& name)
  {
    return find(m_uniforms, name);
  }

  [[nodiscard]] const gl::ShaderStorageBlock* findShaderStorageBlock(const std::string& name) const
  {
    return find(m_shaderStorageBlocks, name);
  }

  gl::ShaderStorageBlock* findShaderStorageBlock(const std::string& name)
  {
    return find(m_shaderStorageBlocks, name);
  }

  [[nodiscard]] const gl::UniformBlock* findUniformBlock(const std::string& name) const
  {
    return find(m_uniformBlocks, name);
  }

  gl::UniformBlock* findUniformBlock(const std::string& name)
  {
    return find(m_uniformBlocks, name);
  }

  void bind() const;

  [[nodiscard]] const gl::Program& getHandle() const
  {
    return m_handle;
  }

private:
  static gsl::not_null<std::shared_ptr<ShaderProgram>> createFromSource(const std::string& programId,
                                                                        const std::string& vshId,
                                                                        const std::filesystem::path& vshPath,
                                                                        const std::string& vshSource,
                                                                        const std::string& fshId,
                                                                        const std::filesystem::path& fshPath,
                                                                        const std::string& fshSource,
                                                                        const std::vector<std::string>& defines = {});

  gl::Program m_handle;
  std::string m_id;

  boost::container::flat_map<std::string, gl::ProgramInput> m_vertexAttributes;
  boost::container::flat_map<std::string, gl::Uniform> m_uniforms;
  boost::container::flat_map<std::string, gl::ShaderStorageBlock> m_shaderStorageBlocks;
  boost::container::flat_map<std::string, gl::UniformBlock> m_uniformBlocks;

  template<typename T>
  static const T* find(const boost::container::flat_map<std::string, T>& map, const std::string& needle)
  {
    auto it = map.find(needle);
    return it == map.end() ? nullptr : &it->second;
  }

  template<typename T>
  static T* find(boost::container::flat_map<std::string, T>& map, const std::string& needle)
  {
    auto it = map.find(needle);
    return it == map.end() ? nullptr : &it->second;
  }
};
} // namespace render::scene
