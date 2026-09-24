#pragma once

#include "CreditFlash.h"
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

  /// **WHETHER THE DESIGN CARRIES ORE AT ALL**, from its derived capacity -- a design that does not draws no
  /// cargo row, rather than an empty one (`design_handoff_hud` section 2).
  bool carriesOre = false;

  /// **HOW MANY OF THE FOUR CHIPS LIGHT: THE GROUP'S MEAN, ROUNDED TO NEAREST** (M2.7, `OpenQuestions.md` Q53),
  /// over what each record's flags carry.
  std::uint8_t cargoChips = 0;

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

  /// **THE CHANGE FLASH UNDER THE BALANCE** (M2.7, Q36): which way it moved, and how far the flash has faded.
  /// `CreditFlash` computes both; `None` draws nothing.
  CreditChange creditFlash = CreditChange::None;
  float creditFlashAlpha = 0.0f;

  /// At most four, in `DesignId` order. Empty means nothing is selected and the panel is not drawn.
  std::vector<SelectionGroupSummary> groups;

  /// Your own station is selected, so the build panel is drawn.
  bool buildPanelOpen = false;

  /// **THE WIRE'S ENCODING, NOT A `DesignId`**: the low four bits are zero for nothing building and the design
  /// plus one otherwise, and the high four how many wait behind it (Q80, `GameCore/Update.h`). Decoded by
  /// `BuildingDesign` and `QueuedOf`.
  std::uint8_t buildingWire = 0;
  std::uint8_t buildProgressPercent = 0;

  /// **THE ARMED MODULE, IF ONE IS** (M2.11). Client-local: the button draws armed, and a tap on the plane is
  /// the placement's.
  bool moduleArmed = false;
  DesignId armedModule = DesignId::ModuleShipyardL1;

  /// **THE PLACEMENT RADIUS, ALREADY PROJECTED** (`ModulePlacement.h`'s `PlacementRingSquares`), in authored
  /// pixels. Empty when nothing is armed. Drawn under the panels and never in the hit table.
  std::vector<HudRect> placementRing;

  /// **THE DESIGNS OF THIS PLAYER'S MODULES** (M2.11b), from the snapshot's module entities: what decides whether
  /// a module button is available -- an L2 needs an L1 of its kind, and a placement needs room under the cap.
  std::vector<DesignId> ownModules;

  LinkState link = LinkState::Joining;
  bool quitArmed = false;

  /// **THE RESULT OVERLAY IS UP** (M3.8): a match ended moments ago. The winner is `NO_PLAYER` for a draw, which is
  /// why the overlay has a flag of its own rather than reading "a winner is set".
  bool matchEnded = false;
  PlayerId winner = NO_PLAYER;

  /// It ended on the six-minute clock rather than on the last station standing (Q65).
  bool endedOnClock = false;

  /// The join was refused because this client derived a different map from the host's (Q76).
  bool fieldMismatch = false;
};

/// The wire's building byte, decoded. False when nothing is building.
[[nodiscard]] bool BuildingDesign(std::uint8_t _wire, DesignId& _outDesign) noexcept;

/// **THE FOUR LOOKS A BUILD BUTTON HAS** (`design_handoff_hud`, M2.11b). *Unaffordable* means save up: the plate
/// stays lit and the cost reddens. *Unavailable* means build something else first: the whole button dims and
/// hatches, and it is not a target. They must not look alike, because one fixes itself second by second and
/// the other never does.
enum class BuildButtonState : std::uint8_t
{
  Live,
  Unaffordable,
  Unavailable,
  Armed
};

/// **WHETHER A MODULE CAN BE CHOSEN AT ALL** (M2.11b): an L2 needs one of your modules it upgrades (Q54), and a
/// placed level needs room under the cap of four. Credits have nothing to do with it.
[[nodiscard]] bool ModuleAvailable(DesignId _design, std::span<const DesignId> _ownModules) noexcept;

/// A module button's state. **Unavailable wins over unaffordable** -- the state credits cannot fix is the one to
/// show -- and armed over both, since only an available button can have been armed.
[[nodiscard]] BuildButtonState ModuleButtonState(DesignId _design, const HudState& _state) noexcept;

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
[[nodiscard]] std::vector<SelectionGroupSummary> SummarizeSelection(std::span<const WireIdentity> _selection,
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
