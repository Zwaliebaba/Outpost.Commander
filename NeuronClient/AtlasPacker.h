#pragma once

#include <cstdint>

namespace Neuron
{

/// Where a rectangle landed in the atlas. R8: a public aggregate.
struct AtlasSlot
{
  std::uint32_t left = 0;
  std::uint32_t top = 0;
  std::uint32_t widthPixels = 0;
  std::uint32_t heightPixels = 0;

  [[nodiscard]] friend constexpr bool operator==(const AtlasSlot&, const AtlasSlot&) noexcept = default;
};

/// **SHELF PACKING, AND THE CHOICE IS THE POINT OF THIS CLASS** (M1.12, ADR-009).
///
/// Rectangles are placed left to right along a shelf; when one does not fit, a new shelf opens at the
/// tallest point of the one below. It wastes the space above every rectangle shorter than its shelf,
/// and for **glyphs of one or two sizes that waste is small** — a row of Segoe UI Semibold at 40 physical
/// pixels varies by the difference between an `x` and an `H`, not by orders of magnitude. A general
/// packer (skyline, MaxRects) buys back a few per cent of a texture that is already small, in exchange
/// for code with a failure mode nobody here would find.
///
/// **IT IS ORDER-DEPENDENT AND THAT IS NOT A DEFECT**, but it is the thing to know: the same rectangles
/// in a different order give a different and possibly taller packing. `GlyphAtlas` therefore feeds it in
/// a fixed order, so an atlas is a function of its inputs and a test can pin one.
///
/// **NOTHING HERE KNOWS WHAT A GLYPH IS** (R9), which is why it is the half of M1.12 a suite can reach:
/// `GlyphAtlas` needs DirectWrite and a graphics device and this needs neither.
class AtlasPacker
{
public:
  AtlasPacker() noexcept = default;

  /// Empties the packer and sets the extent it packs into. Calling it again is how a resize or a device
  /// removal rebuilds the atlas (ADR-009), and it is the only way to reclaim space -- there is no
  /// removal, because a glyph cache never gives one back.
  void Reset(std::uint32_t _widthPixels, std::uint32_t _heightPixels) noexcept;

  /// Places one rectangle, or returns false and leaves the packer untouched.
  ///
  /// **A ZERO-SIZED RECTANGLE IS PLACED RATHER THAN REFUSED**, because a space has no coverage and is a
  /// perfectly ordinary glyph: refusing it would make the caller special-case the commonest character in
  /// any string. It takes no room and its slot is empty at the cursor.
  [[nodiscard]] bool Place(std::uint32_t _widthPixels, std::uint32_t _heightPixels, AtlasSlot& _outSlot) noexcept;

  [[nodiscard]] std::uint32_t WidthPixels() const noexcept;
  [[nodiscard]] std::uint32_t HeightPixels() const noexcept;

  /// How far down the atlas is actually used -- the bottom of the current shelf. **The measurement the
  /// packing quality claim above rests on**, and what a suite asserts rather than trusting.
  [[nodiscard]] std::uint32_t UsedHeightPixels() const noexcept;

private:
  std::uint32_t m_widthPixels = 0;
  std::uint32_t m_heightPixels = 0;

  /// The top of the shelf being filled, and how tall the tallest rectangle on it is.
  std::uint32_t m_shelfTop = 0;
  std::uint32_t m_shelfHeightPixels = 0;

  /// How far along the shelf the next rectangle goes.
  std::uint32_t m_cursorX = 0;
};

} // namespace Neuron
