#pragma once

namespace Outpost
{
// The light over the scene, which MeshPipeline's shaders work out from these numbers (ADR-090). It is presentation, not
// tuning: none of it changes a rule (Game §look).
//
// A warm key comes from above and behind the default view's top-left, its edge between light and shade softened by
// KEY_WRAP, and a cool fill blends from SKY_FILL on a face that looks up to GROUND_FILL on one that looks down. Each is a
// linear color with its brightness in it. A face takes the light's color, so the side that faces the key is warm and its
// shade is cool. A line takes only the light's brightness, so it keeps its side's hue, and every line the scene draws keeps
// its own color: the grid, the borders, the rings. Where the camera can look, no face gets less light than the floor of
// ADR-090 decision 3, so a model's lines stand over its faces as they did, and a face that looks straight up is lit nearly gray, so
// a mark that lies flat keeps its color.
inline constexpr DirectX::XMFLOAT3 TOWARD_KEY{-0.4f, 0.8f, 0.45f};
inline constexpr DirectX::XMFLOAT3 KEY_COLOR{0.627f, 0.539f, 0.427f};
inline constexpr float KEY_WRAP = 0.3f;
inline constexpr DirectX::XMFLOAT3 SKY_FILL{0.365f, 0.421f, 0.571f};
inline constexpr DirectX::XMFLOAT3 GROUND_FILL{0.314f, 0.287f, 0.389f};
// How far a face's light is darkened where the rest of its model shuts it in (ADR-092): all the way, so that a vertex the
// baker found wholly shut in is black and one half shut in is lit by half. A line is never darkened, so the lines in a
// recess stand out the more.
inline constexpr float OCCLUSION_STRENGTH = 1.0f;

// The light that reaches a face whose unit normal is _normal, per channel, as MeshPS.hlsl works it out, where the face is
// _openness open (MeshVertex::openness).
[[nodiscard]] DirectX::XMFLOAT3 FaceLight(const DirectX::XMFLOAT3& _normal, float _openness = 1.0f) noexcept;

// How bright the light is that reaches a line whose unit normal is _normal, as MeshLinePS.hlsl works it out: the
// luminance of FaceLight's.
[[nodiscard]] float LineLight(const DirectX::XMFLOAT3& _normal) noexcept;

// The relative luminance of a linear color, by the weights of linear sRGB.
[[nodiscard]] constexpr float Luminance(float _red, float _green, float _blue) noexcept
{
  return (0.2126f * _red) + (0.7152f * _green) + (0.0722f * _blue);
}

// Every model also shows its edges as lines, for a vector look from the eighties (ADR-027). The faces are dark, FILL_SHADE
// of the model's color, and the lines, lit as the model is, carry its shape (ADR-040). A ship's or structure's lines are its
// color made up to EDGE_BRIGHTNESS brighter, but no further than its brightest channel reaching 1, so that its hue holds
// rather than clipping toward cyan or orange, and then EDGE_WHITE_SHARE of the way to white: about 4:1 above the faces
// beside them on the lit side and nearly 3:1 on the dark side. The rocks are terrain, so their lines are only
// ROCK_EDGE_BRIGHTNESS brighter and keep their gray: near white, they outshone both fleets (owner, 2026-10-02, ADR-040 decision 1).
inline constexpr float EDGE_BRIGHTNESS = 2.0f;
inline constexpr float EDGE_WHITE_SHARE = 0.35f;
inline constexpr float ROCK_EDGE_BRIGHTNESS = 1.35f;
inline constexpr float FILL_SHADE = 0.3f;
// A rock's faces are darker still, half a ship's, so that at the default zoom the terrain stands back from the fleet; its
// lines keep ROCK_EDGE_BRIGHTNESS and carry its shape (ADR-040 decision 1).
inline constexpr float ROCK_FILL_SHADE = 0.15f;
// A field's rocks are darker than an ore asteroid, so the two read apart.
inline constexpr float FIELD_SHADE = 0.7f;
// A structure stands back from the ships, which are what take orders: its color STRUCTURE_GRAY_SHARE of the way to a gray
// as light as it, and its faces STRUCTURE_FILL_SHADE of that rather than FILL_SHADE (ADR-040).
inline constexpr float STRUCTURE_GRAY_SHARE = 0.35f;
inline constexpr float STRUCTURE_FILL_SHADE = 0.22f;

// A color times _scale, kept at most 1.
[[nodiscard]] DirectX::XMFLOAT4 Shaded(const DirectX::XMFLOAT4& _color, float _scale) noexcept;

// A model's lines in its _color: made up to _brightness brighter, but no further than its brightest channel reaching 1,
// so that the hue holds, then _whiteShare of the way to a white as bright as that channel. A darker color, such as a
// structure's while it is built, gives darker lines.
[[nodiscard]] DirectX::XMFLOAT4 EdgeColor(const DirectX::XMFLOAT4& _color, float _brightness, float _whiteShare) noexcept;

// _color _share of the way to a gray as light as it is, by the luminance weights of linear sRGB.
[[nodiscard]] DirectX::XMFLOAT4 TowardGray(const DirectX::XMFLOAT4& _color, float _share) noexcept;

// A model's faces take one of three paints, by their materials' names in its source, which are the mesh's paints 0, 1 and
// 2 (Neuron::MeshVertex::paint, ADR-095). Team is the model's color, as every face was before paints. Trim is neutral:
// the gray as light as the team color, so that it shades and lines as the team color does and the side still reads from
// the body. Light is LIGHT_PAINT_COLOR, a warm white that no light reaches and no shade darkens, bright enough for the
// glow to take it (ADR-091): a window or a lamp. Its lines are the same color.
inline constexpr DirectX::XMFLOAT4 LIGHT_PAINT_COLOR{1.0f, 0.86f, 0.62f, 1.0f};

// The color each paint takes, of a model's faces or of its lines.
struct PaintColors
{
  DirectX::XMFLOAT4 team;
  DirectX::XMFLOAT4 trim;
  DirectX::XMFLOAT4 light;
};

// The neutral color a model's trim takes where its team color is _color.
[[nodiscard]] DirectX::XMFLOAT4 TrimColor(const DirectX::XMFLOAT4& _color) noexcept;

// A model's faces where its color is _color: team and trim each Shaded by _fillShade, and light as it is.
[[nodiscard]] PaintColors FacePaints(const DirectX::XMFLOAT4& _color, float _fillShade) noexcept;

// A model's lines where its color is _color: team and trim each an EdgeColor of _brightness and _whiteShare, and light as
// it is.
[[nodiscard]] PaintColors LinePaints(const DirectX::XMFLOAT4& _color, float _brightness, float _whiteShare) noexcept;

// A copy of a model's mesh, its faces or its lines, at _world in _paints, its light glowing.
[[nodiscard]] Neuron::MeshPipeline::Instance PaintedInstance(const DirectX::XMFLOAT4X4& _world, const PaintColors& _paints,
                                                             float _liftShare) noexcept;
} // namespace Outpost
