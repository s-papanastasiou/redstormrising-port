#version 130
// Test pass for the shader-chain presenter: darken the gap between
// source rows. Samples PassPrev1 (== Texture), Orig and the STAGE alias so a
// broken uniform binding shows up as a wrong picture.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma parameter SCANLINE_DARK "Scanline darkness" 0.3 0.0 1.0 0.05

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
uniform sampler2D OrigTexture;
uniform sampler2D STAGETexture;
uniform vec2 TextureSize;
uniform vec2 OrigTextureSize;
#ifdef PARAMETER_UNIFORM
uniform float SCANLINE_DARK;
#else
#define SCANLINE_DARK 0.3
#endif
void main() {
  // An sRGB framebuffer encodes on write and decodes on read, so the staged
  // copy round-trips to the input value. All three inputs hold the same
  // picture; averaging them makes a wrong binding visible.
  vec3 staged = texture(Texture, uv).rgb;
  vec3 alias = texture(STAGETexture, uv).rgb;
  vec3 orig = texture(OrigTexture, uv).rgb;
  vec3 c = (staged + alias + orig) / 3.0;
  float row = fract(uv.y * OrigTextureSize.y);
  float beam = 1.0 - SCANLINE_DARK * smoothstep(0.25, 0.5, abs(row - 0.5));
  FragColor = vec4(c * beam, 1.0);
}
#endif
