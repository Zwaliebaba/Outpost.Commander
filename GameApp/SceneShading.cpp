#include "pch.h"
#include "SceneShading.h"

#include <algorithm>
#include <cmath>

DirectX::XMFLOAT3 Outpost::FaceLight(const DirectX::XMFLOAT3& _normal, float _openness) noexcept
{
  // As MeshPS.hlsl has it: the fill by how far the face looks up, and the key by how far it faces the key, softened, the
  // two darkened by how shut in the face is.
  const float keyLength = std::sqrt((TOWARD_KEY.x * TOWARD_KEY.x) + (TOWARD_KEY.y * TOWARD_KEY.y) + (TOWARD_KEY.z * TOWARD_KEY.z));
  const float facing = ((_normal.x * TOWARD_KEY.x) + (_normal.y * TOWARD_KEY.y) + (_normal.z * TOWARD_KEY.z)) / keyLength;
  const float key = std::clamp((facing + KEY_WRAP) / (1.0f + KEY_WRAP), 0.0f, 1.0f);
  const float up = (_normal.y * 0.5f) + 0.5f;
  const float darkened = 1.0f - (OCCLUSION_STRENGTH * (1.0f - std::clamp(_openness, 0.0f, 1.0f)));
  const auto channel = [up, key, darkened](float _ground, float _sky, float _key)
  { return (std::lerp(_ground, _sky, up) + (_key * key)) * darkened; };
  return {channel(GROUND_FILL.x, SKY_FILL.x, KEY_COLOR.x), channel(GROUND_FILL.y, SKY_FILL.y, KEY_COLOR.y),
          channel(GROUND_FILL.z, SKY_FILL.z, KEY_COLOR.z)};
}

float Outpost::LineLight(const DirectX::XMFLOAT3& _normal) noexcept
{
  const DirectX::XMFLOAT3 light = FaceLight(_normal);
  return Luminance(light.x, light.y, light.z);
}

DirectX::XMFLOAT4 Outpost::Shaded(const DirectX::XMFLOAT4& _color, float _scale) noexcept
{
  return {std::min(1.0f, _color.x * _scale), std::min(1.0f, _color.y * _scale), std::min(1.0f, _color.z * _scale), _color.w};
}

DirectX::XMFLOAT4 Outpost::EdgeColor(const DirectX::XMFLOAT4& _color, float _brightness, float _whiteShare) noexcept
{
  const float brightest = std::max({_color.x, _color.y, _color.z});
  if (brightest <= 0.0f)
    return _color;
  const float gain = std::min(_brightness, 1.0f / brightest);
  const float peak = brightest * gain;
  const auto channel = [gain, peak, _whiteShare](float _channel) { return std::lerp(_channel * gain, peak, _whiteShare); };
  return {channel(_color.x), channel(_color.y), channel(_color.z), _color.w};
}

DirectX::XMFLOAT4 Outpost::TowardGray(const DirectX::XMFLOAT4& _color, float _share) noexcept
{
  const float lightness = Luminance(_color.x, _color.y, _color.z);
  return {std::lerp(_color.x, lightness, _share), std::lerp(_color.y, lightness, _share), std::lerp(_color.z, lightness, _share), _color.w};
}
