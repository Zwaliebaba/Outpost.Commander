#pragma once

#include "HudLayout.h"
#include "JoinState.h"
#include "PanelHitTest.h"
#include "Selection.h"

#include "GameCore.h"
#include "NeuronClient.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Outpost
{

/// One design's row in the selection panel. R8: a public aggregate.
struct SelectionGroupSummary
{
  DesignId design = DesignId::Miner;
  std::uint32_t count = 0;

  /// **CONTINUOUS 0 TO 100, AGGREGATED ACROSS THE GROUP** -- the mean of what the wire carries per
  /// entity, which is a percentage already (ADR-003).
  std::uint8_t hullPercent = 100;

  [[nodiscard]] friend constexpr bool operator==(const SelectionGroupSummary&, const SelectionGroupSummary&) noexcept = default;
};

/// **EVERYTHING THE PANELS READ, AND NOTHING ELSE.** `design_handoff_hud` *Data the HUD may read* is the
/// boundary: a readout that needs more than this must not be built. R8: a public aggregate.
struct HudState
{
  /// **THE ONE VALUE `OpenQuestions.md` Q33 COSTS.** False is the primary, right-handed layout.
  bool leftHanded = false;

  /// This client's player, one-based, for the winner's team color.
  PlayerId player = NO_PLAYER;

  std::uint32_t credits = 0;

  /// At most four, in `DesignId` order. Empty means nothing is selected and the panel is not drawn.
  std::vector<SelectionGroupSummary> groups;

  /// Your own station is selected, so the build panel is drawn.
  bool buildPanelOpen = false;

  /// **THE WIRE'S ENCODING, NOT A `DesignId`**: zero is nothing building, and anything else is the design
  /// plus one (`GameLogic/BuildSystem.cpp`, `WireBuildingDesign`). Decoded by `BuildingDesign`.
  std::uint8_t buildingWire = 0;
  std::uint8_t buildProgressPercent = 0;

  LinkState link = LinkState::Joining;
  bool quitArmed = false;

  /// Set when the match ends, to the winner. **Nothing sets it before M3**, which is where a match first
  /// can end; the result overlay is drawn from it rather than written then.
  PlayerId winner = NO_PLAYER;
};

/// The wire's building byte, decoded. False when nothing is building.
[[nodiscard]] bool BuildingDesign(std::uint8_t _wire, DesignId& _outDesign) noexcept;

/// The designs a station's build panel offers, in the order the buttons are drawn. **Rows, not types**
/// (ADR-006): the panel draws whatever the design table marks buildable, and nothing here names one.
[[nodiscard]] std::span<const DesignId> BuildableDesigns() noexcept;

/// **ONE DRAW ITEM OF THE INTERFACE**, in authored coordinates. The list is in draw order, which is the
/// only ordering the interface pass has. R8: a public aggregate.
struct HudItem
{
  enum class Kind : std::uint8_t
  {
    Solid,
    Text
  };

  Kind kind = Kind::Solid;
  HudRect rect{};
  Neuron::QuadColor color{};

  std::wstring text;
  Neuron::TextSize size = Neuron::TextSize::Body;
  Neuron::TextAlign align = Neuron::TextAlign::Left;
  float trackingEm = 0.0f;
};

/// One frame of interface: what to draw, and what a tap on it means. **Built together so they cannot
/// disagree** -- a button drawn where the hit table does not have it is the defect a separate pass
/// would eventually produce.
struct HudFrame
{
  std::vector<HudItem> items;
  HudHitTable hits;
};

/// **THE FOUR PANELS AND THE OVERLAYS, AS DATA** (M1.14). Pure: no device, no atlas, no clock -- which is
/// what lets the suite assert every target's tier and clear space in both handedness states rather than
/// measuring the screen by eye.
[[nodiscard]] HudFrame BuildHud(const HudState& _state);

/// The selection grouped by design, as the panel shows it, **in `DesignId` order and at most four**.
/// Identities the snapshot no longer carries are skipped, as `Selection::RetainLiving` would.
[[nodiscard]] std::vector<SelectionGroupSummary> SummarizeSelection(std::span<const std::uint16_t> _selection,
                                                                    std::span<const EntityRecord> _entities);

/// A tap on a group: **the selection narrows to that design**. Returns how many remain.
std::size_t NarrowToDesign(Selection& _selection, std::span<const EntityRecord> _entities, DesignId _design);

/// A credit balance with a thousands separator: `1,000`, `12,450`. The worst case is five digits.
[[nodiscard]] std::wstring FormatCredits(std::uint32_t _credits);

/// A station's order: build a design, or cancel what is building. **No selection travels with it** --
/// the host refuses a station order that carries one (M1.6), because the order is the player's rather
/// than any ship's.
[[nodiscard]] Command BuildStationCommand(std::uint16_t _sequence, CommandType _type, DesignId _design) noexcept;

/// What `EmitQuads` needs from the glyph atlas, lifted out so a suite can supply it by hand.
///
/// R8: a public aggregate.
struct HudTypeface
{
  std::array<const Neuron::GlyphTable*, Neuron::TEXT_SIZE_COUNT> tables{};
  std::array<Neuron::FaceMetrics, Neuron::TEXT_SIZE_COUNT> faces{};
  Neuron::AtlasSlot solid{};
  std::uint32_t atlasWidthPixels = 0;
  std::uint32_t atlasHeightPixels = 0;
};

/// The atlas's tables and metrics, for the frame loop.
[[nodiscard]] HudTypeface TypefaceOf(const Neuron::GlyphAtlas& _atlas) noexcept;

/// **THE FRAME INTO QUADS, THROUGH THE INTERFACE FIT** (ADR-011, ADR-016). Every rect is mapped edge by
/// edge by `Neuron::MapAuthoredRect`, so two rects authored flush stay flush at any window size, and
/// every text run is laid out at the physical size the atlas was rasterized for.
void EmitQuads(const HudFrame& _frame, const HudTypeface& _typeface, const Neuron::FitTransform& _interfaceFit,
               std::vector<Neuron::GlyphQuad>& _out);

} // namespace Outpost
