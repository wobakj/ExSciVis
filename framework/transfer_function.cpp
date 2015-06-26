#include "transfer_function.hpp"

#include <iostream>
#include <fstream>
#include <GL/glew.h>

#ifdef __APPLE__
# define __gl3_h_
# define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "utils.hpp"

const char* vertex_shader = "\
#version 330\n\
//#extension GL_ARB_shading_language_420pack : require\n\
#extension GL_ARB_explicit_attrib_location : require\n\
                                                        \n\
layout(location = 0) in vec3 position;\n\
layout(location = 1) in vec2 vtexcoord; \n\
out vec2 fTexCoord;\n\
\n\
void main()\n\
{\n\
    fTexCoord = vtexcoord;\n\
    gl_Position = vec4(position, 1.0);\n\
}\n\
";

const char* fragment_shader = "\
#version 330\n\
//#extension GL_ARB_shading_language_420pack : require\n\
#extension GL_ARB_explicit_attrib_location : require\n\
                                                            \n\
uniform sampler2D transfer_texture;\n\
\n\
in vec3 vColor;\n\
in vec2 fTexCoord;\n\
layout(location = 0) out vec4 FragColor;\n\
\n\
void main()\n\
{\n\
    //FragColor = vec4(fTexCoord.x, fTexCoord.y*2.0 , 0.0, 1.0);\n\
    //FragColor = vec4(fTexCoord, 0.0, 1.0);\n\
    //FragColor = vec4(fTexCoord , 0.0, 1.0);\n\
    FragColor= texture(transfer_texture, fTexCoord);\n\
}\n\
";

namespace helper {

template<typename T>
void clamp(T& val, const T min, const T max)
{
  val = ((val > max) ? max : (val < min) ? min : val);
}

template<typename T>
const T weight(const float w, const T a, const T b)
{    
    return ((1.0f - w) * a + w * b);
}

} // namespace helper

Transfer_function::Transfer_function()
  : m_piecewise_container(),
  m_buffer(255 * 4), // width =255 height = 1 channels = 4 ///TODO: maybe dont hardcode?
  m_program_id(0),
  //m_vao(0),
  m_plane(),
  m_dirty(true)
{
    m_program_id = createProgram(vertex_shader, fragment_shader);
}

void Transfer_function::add(float data_value, glm::vec4 color)
{
  add((unsigned)(data_value * 255.0), color);
  m_dirty = true;
}

void
Transfer_function::add(unsigned data_value, glm::vec4 color)
{
  helper::clamp(data_value, 0u, 255u);
  color = glm::clamp(color, glm::vec4(0.0f), glm::vec4(1.0f));

  //m_piecewise_container.insert(element_type(data_value, color));
  m_piecewise_container[data_value] = color;
  m_dirty = true;
}

void
Transfer_function::remove(unsigned data_value)
{
    helper::clamp(data_value, 0u, 255u);

    //m_piecewise_container.insert(element_type(data_value, color));
    m_piecewise_container.erase(data_value);
    m_dirty = true;
}

void Transfer_function::update_buffer(){

  unsigned data_value_f = 0u;
  unsigned data_value_b = 255u;
  glm::vec4 color_f = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
  glm::vec4 color_b = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

  unsigned  e_value;
  glm::vec4 e_color;

  for (element_type e : m_piecewise_container) {
    e_value = e.first;
    e_color = e.second;

    data_value_b = e_value;
    color_b = e_color;

    unsigned data_value_d = data_value_b - data_value_f;
    float step_size = 1.0f / static_cast<float>(data_value_d);
    float step = 0.0f;
        
    for (unsigned i = data_value_f; i != data_value_b; ++i) {

      m_buffer[i * 4]     = static_cast<unsigned char>(helper::weight(step, color_f.r, color_b.r) * 255.0f);
      m_buffer[i * 4 + 1] = static_cast<unsigned char>(helper::weight(step, color_f.g, color_b.g) * 255.0f);
      m_buffer[i * 4 + 2] = static_cast<unsigned char>(helper::weight(step, color_f.b, color_b.b) * 255.0f);
      m_buffer[i * 4 + 3] = static_cast<unsigned char>(helper::weight(step, color_f.a, color_b.a) * 255.0f);
      step += step_size;     
    }

    data_value_f = data_value_b;
    color_f = color_b;
  }

  // fill TF
  data_value_b = 255u;
  color_b = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

  if (data_value_f != data_value_b) {
    unsigned data_value_d = data_value_b - data_value_f;
    float step_size = 1.0f / static_cast<float>(data_value_d);
    float step = 0.0;
    
    for (unsigned i = data_value_f; i != data_value_b; ++i) {
      m_buffer[i * 4]     = static_cast<unsigned char>(helper::weight(step, color_f.r, color_b.r) * 255.0f);
      m_buffer[i * 4 + 1] = static_cast<unsigned char>(helper::weight(step, color_f.g, color_b.g) * 255.0f);
      m_buffer[i * 4 + 2] = static_cast<unsigned char>(helper::weight(step, color_f.b, color_b.b) * 255.0f);
      m_buffer[i * 4 + 3] = static_cast<unsigned char>(helper::weight(step, color_f.a, color_b.a) * 255.0f);
      step += step_size;
    }
  }
}

image_data_type const&
Transfer_function::get_buffer() {
  if(m_dirty) {
    update_buffer();
    m_dirty = false;
  }
  return m_buffer;
}


void
Transfer_function::reset(){
    m_piecewise_container.clear();
    m_buffer.clear();
    m_buffer.resize(255 * 4);
    m_dirty = true;
}


void                  
Transfer_function::draw_texture(const glm::vec2& tf_pos, const glm::vec2& tf_size, const GLuint& texture) const{

    glViewport((GLint)tf_pos.x, (GLint)tf_pos.y, (GLint)tf_size.x, (GLint)tf_size.y);

    glUseProgram(m_program_id);

    // set texture uniform
    glUniform1i(glGetUniformLocation(m_program_id, "transfer_texture"), 1);

    m_plane.draw();

    glUseProgram(0);
}