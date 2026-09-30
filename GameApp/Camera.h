#pragma once

namespace Outpost
{
// The camera's numbers, from OutpostCommander/Assets/Camera.json (design §4, ADR-012). The zoom limits are provisional
// until gate G3.
struct CameraSettings
{
  // How wide the ground is across the middle of the screen, at the focus point.
  float defaultViewWidthMeters = 0.0f;
  float minimumViewWidthMeters = 0.0f;
  float maximumViewWidthMeters = 0.0f;
  // How far the camera looks down from the horizontal, fully zoomed in and fully zoomed out; between the two it follows
  // the view width in a straight line.
  float pitchAtMinimumDegrees = 0.0f;
  float pitchAtMaximumDegrees = 0.0f;
  float verticalFieldOfViewDegrees = 0.0f;
  // The focus point stays within this distance of the origin along x and along z.
  float focusLimitMeters = 0.0f;
  // Panning speed, in view widths per second, so that it feels the same at every zoom.
  float panViewWidthsPerSecond = 0.0f;
  // How close to the screen's edge the cursor scrolls the view.
  std::int32_t edgeScrollMarginPixels = 0;
  // One notch of the wheel divides or multiplies the view width by this.
  float zoomFactorPerNotch = 0.0f;
  float rotateDegreesPerSecond = 0.0f;
};

// The back buffer the camera draws into, in physical pixels.
struct Viewport
{
  std::uint32_t widthPixels = 0;
  std::uint32_t heightPixels = 0;

  [[nodiscard]] float AspectRatio() const noexcept
  {
    return heightPixels == 0 ? 1.0f : static_cast<float>(widthPixels) / static_cast<float>(heightPixels);
  }
};

// Reads the text of OutpostCommander/Assets/Camera.json. Throws Neuron::Exception on the first problem, naming where it
// is. Besides types and ranges it checks that the default width is within the limits and the pitches are between 0 and
// 90 degrees.
[[nodiscard]] CameraSettings LoadCameraSettings(std::string_view _json);

// The RTS camera (design §4, ADR-012): a focus point on the ground, a direction to look in and a zoom, with the pitch
// following the zoom. It is client state, not server state (ADR-002). Its math is pure, so it is tested without a GPU.
class Camera
{
public:
  explicit Camera(const CameraSettings& _settings) noexcept;

  // Pans by edge scroll, the arrow keys and middle-drag, zooms by the wheel and turns by Q and E. Edge scroll works only
  // while the cursor is held inside the window (ADR-012). A and S are orders, not pans (design §9).
  void Update(const Neuron::InputState& _input, float _elapsedSeconds, std::uint32_t _viewportWidthPixels,
              std::uint32_t _viewportHeightPixels) noexcept;

  // Moves the focus by distances along the ground, to the screen's right and to its top, and keeps it inside the limit.
  void Pan(float _rightMeters, float _forwardMeters) noexcept;
  // Moves the focus to a point on the ground, kept inside the limit.
  void SetFocus(float _xMeters, float _zMeters) noexcept;
  // Positive notches zoom in. The view width stays within its limits.
  void Zoom(float _notches) noexcept;
  // Positive turns counterclockwise seen from above.
  void Rotate(float _radians) noexcept;

  [[nodiscard]] float ViewWidthMeters() const noexcept
  {
    return m_viewWidthMeters;
  }
  [[nodiscard]] DirectX::XMFLOAT2 Focus() const noexcept
  {
    return {m_focusXMeters, m_focusZMeters};
  }
  // How far the camera looks down from the horizontal.
  [[nodiscard]] float PitchRadians() const noexcept;
  // The camera's height and distance follow from the width it has to show across a screen of this shape.
  [[nodiscard]] DirectX::XMFLOAT3 EyePosition(float _aspectRatio) const noexcept;
  [[nodiscard]] DirectX::XMFLOAT4X4 ViewProjection(float _aspectRatio) const noexcept;

  // Where the ray through a point of the screen meets the ground (y = 0), with the screen from -1 to 1 on both axes and
  // +1 at the top. Nothing when the ray does not come down to the ground.
  [[nodiscard]] std::optional<DirectX::XMFLOAT2> GroundPoint(float _screenX, float _screenY, float _aspectRatio) const noexcept;

  // The same for a pixel of the viewport, counted from its top-left corner.
  [[nodiscard]] std::optional<PlanePosition> GroundPointAtPixel(float _xPixels, float _yPixels, const Viewport& _viewport) const noexcept;
  // Where a point on the ground shows on the viewport, in pixels from its top-left corner; nothing when it is behind
  // the camera. The point may be off the viewport.
  [[nodiscard]] std::optional<DirectX::XMFLOAT2> PixelOf(PlanePosition _point, const Viewport& _viewport) const noexcept;

private:
  // The ground directions that are the screen's right and top.
  [[nodiscard]] DirectX::XMFLOAT2 GroundForward() const noexcept;
  [[nodiscard]] DirectX::XMFLOAT2 GroundRight() const noexcept;
  [[nodiscard]] float DistanceToFocus(float _aspectRatio) const noexcept;

  CameraSettings m_settings;
  float m_focusXMeters = 0.0f;
  float m_focusZMeters = 0.0f;
  // The direction the camera looks along the ground, counterclockwise from +x seen from above.
  float m_yawRadians = 0.0f;
  float m_viewWidthMeters = 0.0f;
  // Where the cursor was on the last frame the middle button was held; the drag moves the ground by the difference.
  std::optional<std::pair<std::int32_t, std::int32_t>> m_dragCursorPixels;
};
} // namespace Outpost
