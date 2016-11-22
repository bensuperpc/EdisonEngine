///////////////////////////////////////////////////////////
// Atttributes
attribute vec3 a_position;
attribute vec2 a_texCoord;
attribute vec4 a_color;

///////////////////////////////////////////////////////////
// Uniforms
uniform mat4 u_projectionMatrix;

///////////////////////////////////////////////////////////
// Varyings
varying vec2 v_texCoord;


void main()
{
  gl_Position = u_projectionMatrix * vec4(a_position, 1);
  v_texCoord = a_texCoord;
}
