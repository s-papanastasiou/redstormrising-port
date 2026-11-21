#version 130
// Test pass for the shader-chain presenter: copy the input.
// SPDX-License-Identifier: GPL-3.0-or-later

#if defined(VERTEX)
in vec4 VertexCoord;
in vec4 TexCoord;
out vec2 uv;
uniform mat4 MVPMatrix;
void main() {
  gl_Position = MVPMatrix * VertexCoord;
  uv = TexCoord.xy;
}
#elif defined(FRAGMENT)
in vec2 uv;
out vec4 FragColor;
uniform sampler2D Texture;
void main() { FragColor = texture(Texture, uv); }
#endif
