#pragma once

namespace Outpost
{
// The HUD (tasks 3.6 and 4.5, design §9): the Ore stockpile and income, a panel describing the selection with a
// structure's construction, the buttons that build structures and open a structure's windows, and the minimap. It is
// laid out once in 1920×1080 reference units, each element anchored to a corner or an edge, and scaled to the back buffer
// by one uniform factor (ADR-006). It keeps no GPU state: it says what to draw, in pixels, and GameClient draws it through
// the UI pipeline (ADR-015). Clicks on it do not reach the world. Research shows under the Ore (task 5.1). Over it float
// the designer, the production queue and the research as windows (ADR-031, Phase 1 design §11, §12). The main menu, and
// the banner that says how a match ended, are laid out the same way (task 6.2).
class Hud
{
public:
  static constexpr float REFERENCE_WIDTH_UNITS = 1920.0f;
  static constexpr float REFERENCE_HEIGHT_UNITS = 1080.0f;
  // The text's size at the reference scale.
  static constexpr float FONT_UNITS = 20.0f;

  // The interface's fonts (ADR-030), in the order GameClient builds the UI pipeline with them: Segoe UI, a text's default
  // face, and the faces of the owner's mockup (Phase 1 design §11), which the windows and the HUD take (ADR-043): condensed
  // Bahnschrift for titles, labels and names, and Cascadia Mono for figures, or Consolas where it is not installed, with a
  // smaller size for a part card's numbers.
  enum class Typeface : std::uint8_t
  {
    Body,
    Title,
    Label,
    Name,
    Figure,
    LargeFigure,
    Detail
  };

  // Each typeface's size in reference units, which are pixels at 1080p, in Typeface's order (ADR-062). It is the one place
  // a face's size is set: Typefaces rasterizes the faces at these sizes, and the layout sizes the diamond beside a figure
  // from them.
  static constexpr std::array<float, 7> FACE_UNITS{FONT_UNITS, 22.0f, 13.0f, 16.0f, 14.0f, 28.0f, 13.0f};

  [[nodiscard]] static constexpr float FaceUnits(Typeface _face) noexcept
  {
    return FACE_UNITS[static_cast<std::size_t>(_face)];
  }

  [[nodiscard]] static std::vector<Neuron::FontDesc> Typefaces();

  // The interface's sprites (ADR-030), in the order GameClient builds the UI pipeline with them: Ore's diamond, the box of
  // a part research has yet to unlock, and a window's corner bracket.
  enum class Sprite : std::uint8_t
  {
    OreMark,
    Checkbox,
    Corner
  };

  [[nodiscard]] static std::vector<Neuron::SpriteDesc> Sprites();

  // How wide the interface's text is set, as the atlas it is drawn from sets it (ADR-061): the fonts Typefaces names, in
  // its order, rasterized for a back buffer at _scale pixels to a reference unit. The layout measures every line it places
  // with it, so that a line is cut short, wrapped or given room by what it takes on the screen. It reads the fonts where
  // they are, so they must outlive it.
  class TextMetrics
  {
  public:
    TextMetrics(std::span<const Neuron::GlyphAtlas::Font> _fonts, float _scale) noexcept
      : m_fonts(_fonts),
        m_scale(_scale)
    {
    }

    // How wide _text is set in _face, with _trackingUnits more between each two characters, in reference units, as
    // Neuron::UiPipeline::DrawText sets it: each character at its whole-pixel advance, and the tracking at a whole pixel.
    [[nodiscard]] float Width(Typeface _face, std::string_view _text, float _trackingUnits = 0.0f) const noexcept;

    // _text as _face sets it within _widthUnits: whole when it fits, and otherwise cut short with three dots.
    [[nodiscard]] std::string Fit(Typeface _face, std::string_view _text, float _widthUnits, float _trackingUnits = 0.0f) const;

    // _text broken between its words into lines that _face sets within _widthUnits; a word wider than that is cut short.
    [[nodiscard]] std::vector<std::string> Wrap(Typeface _face, std::string_view _text, float _widthUnits) const;

  private:
    std::span<const Neuron::GlyphAtlas::Font> m_fonts;
    float m_scale = 1.0f;
  };

  // What a button does when it is pressed.
  enum class ActionKind : std::uint8_t
  {
    // Arms placing a structure for the selected Constructors.
    Build,
    // Queues a job at a Shipyard or the Command Station.
    Queue,
    // Queues a topic at the Research Lab.
    Research,
    // The designer: picks a component for a slot, starts typing the name, or saves the design.
    PickHull,
    PickDrive,
    PickWeapon,
    // The module, or none (Phase 2 design §10).
    PickModule,
    EditName,
    SaveDesign,
    // Queues the designer's picks at a Shipyard when they are no saved design yet: saves them first (ADR-023).
    SaveAndQueue,
    // The main menu's and the match end's (task 6.2): start a match against the AI, leave the game, or leave the match.
    StartSkirmish,
    Quit,
    BackToMenu,
    // The designer's window (Phase 1 design §11): open it aimed at a Shipyard, step its target Shipyard, ask for fewer or
    // more ships, load a saved design, and scroll the saved designs.
    OpenDesigner,
    PreviousShipyard,
    NextShipyard,
    FewerShips,
    MoreShips,
    LoadDesign,
    PreviousDesigns,
    NextDesigns,
    // The production and research windows (Phase 1 design §12): open them, step the production window's producer, and
    // scroll the research window's topics.
    OpenProduction,
    OpenResearch,
    PreviousProducer,
    NextProducer,
    PreviousTopics,
    NextTopics,
    // Upgrades the selected structure by one level (Phase 3 design §4, §9).
    Upgrade
  };

  struct Action
  {
    ActionKind kind = ActionKind::Build;
    StructureKind structure = StructureKind::CommandStation;
    EntityId producer;
    // The design a Shipyard builds; no design for the Command Station's Constructor.
    DesignId design;
    ResearchTopicId topic;
    HullId hull;
    DriveId drive;
    WeaponId weapon;
    // A module to pick, or none.
    ModuleId module;
    // How many ships a Queue asks for (Phase 1 design §11).
    std::uint32_t count = 1;

    friend bool operator==(const Action&, const Action&) = default;
  };

  struct Button
  {
    std::string label;
    Action action;
    // A button the player cannot afford, or may not use, is drawn dim and does nothing.
    bool enabled = true;
    // A designer's pick, drawn lit.
    bool selected = false;
    // Why a button that is not enabled may not be pressed, when the reason is not its cost, shown in place of the cost
    // (ADR-046).
    std::string note;
  };

  // A component as the designer shows it on its card (Phase 1 design §11): its name, cost and numbers, a note under them,
  // such as a weapon's splash, whether it is the pick, and while research has yet to unlock it, the line that names the
  // topic that does, "RESEARCH · LARGE HULL". A locked card takes no click.
  struct PartCard
  {
    std::string name;
    std::int32_t cost = 0;
    std::string numbers;
    std::string note;
    bool picked = false;
    std::string lockedBy;
    Action action;

    [[nodiscard]] bool IsLocked() const noexcept
    {
      return !lockedBy.empty();
    }
  };

  // One slot of the design: its label, its pick's name, and a card for each component.
  struct SlotRow
  {
    std::string label;
    std::string picked;
    std::vector<PartCard> cards;
  };

  // How a hovered part's design compares to the picked one on a number: better, no different, or worse. Lower is better
  // for cost and build time.
  enum class Change : std::uint8_t
  {
    Worse,
    Same,
    Better
  };

  // One of the picked design's numbers and its bar, measured against the best any design in the game reaches, locked
  // components included, so that the scale holds as research unlocks parts. With a part hovered, the design it would make.
  struct StatBar
  {
    std::string label;
    std::string value;
    std::string unit;
    float share = 0.0f;
    std::optional<float> previewShare;
    std::string previewValue;
    Change change = Change::Same;
  };

  // How a design's damage against one hull compares to the best any design does to it: from two thirds up, a third up,
  // and below.
  enum class Rating : std::uint8_t
  {
    Poor,
    Fair,
    Good
  };

  // Damage per second after armor against one hull (design §9), per ship and per 100 Ore: the hovered part's design while
  // one is hovered, and how it compares to the picked one.
  struct DamageCard
  {
    std::string hull;
    std::string armor;
    std::string perShip;
    std::string perOre;
    float share = 0.0f;
    Rating rating = Rating::Poor;
    Change change = Change::Same;
  };

  // A saved design's chip: its name, its components' initials, whether it is the one shown, and loading it.
  struct DesignChip
  {
    std::string name;
    std::string code;
    bool shown = false;
    Action action;
  };

  // The designer's window, after the owner's mockup (Phase 1 design §11, GameDesign/Mockups/ShipDesigner.png).
  struct DesignerPanel
  {
    // The target Shipyard, such as "SHIPYARD 01", or "NO SHIPYARD"; its queue's length, the ships it has built, and the
    // player's Ore.
    std::string shipyard;
    bool hasShipyard = false;
    std::uint32_t queued = 0;
    std::uint32_t built = 0;
    std::int32_t ore = 0;
    std::string name;
    bool editing = false;
    // Whether the server takes the name (IsValidDesignName); it is drawn as a warning when not.
    bool nameValid = true;
    // SAVE for a new design, SAVED when the name and the parts are a saved design; RENAME for a saved design's new name.
    Button save;
    Button rename;
    // The saved designs, and the first shown of those that do not all fit.
    std::vector<DesignChip> chips;
    std::size_t firstChip = 0;
    // Hull, drive, weapon and module (Phase 2 design §10).
    std::array<SlotRow, 4> slots;
    std::vector<StatBar> bars;
    std::vector<DamageCard> damage;
    // What the bars and the cards preview while a part is hovered; the help line otherwise.
    std::string hint;
    // How many ships Queue asks for, and whether it can ask for fewer or more.
    std::uint32_t count = 1;
    bool canFewer = false;
    bool canMore = false;
    // Queue: its cost for every ship asked for, and the build time of one.
    Button queue;
    std::int32_t queueCost = 0;
    std::string queueDetail;
  };

  // One job of a queue as a window shows it: its name, and for the front job how far it has come, or that it waits for
  // Ore.
  struct QueueLine
  {
    std::string name;
    bool front = false;
    std::int32_t permille = 0;
    bool waiting = false;
  };

  // A button of a window that adds to a queue: what it adds, a line under the name, such as a design's abbreviation, its
  // cost, and whether it can be pressed now.
  struct QueueOption
  {
    std::string name;
    std::string detail;
    std::int32_t cost = 0;
    Action action;
    bool enabled = true;
  };

  // The production window (Phase 1 design §12; owner, 2026-10-03): the producer it shows, such as "COMMAND STATION" or
  // "SHIPYARD 02", whether there are others to step to, its queue, and a button for each thing it builds: a Constructor
  // at the Command Station, each saved design at a Shipyard. With no producer, a line saying how to get one.
  struct ProductionPanel
  {
    std::string producer;
    bool hasProducer = false;
    bool canStep = false;
    std::int32_t ore = 0;
    std::vector<QueueLine> queue;
    std::vector<QueueOption> options;
    std::string hint;
  };

  // A research topic not researched or queued yet, as the research window shows it (design §8): what it does, its cost,
  // its tier and time, and in capitals what it needs: the Research Lab's level, while its tier is not open (Phase 3 design
  // §6), and each prerequisite neither researched nor queued. A topic that needs any is dim.
  struct TopicCard
  {
    std::string name;
    std::string effect;
    std::int32_t cost = 0;
    std::string time;
    std::vector<std::string> needs;
    Action action;
    bool enabled = true;
    std::int32_t tier = 1;
  };

  // The research window (Phase 1 design §12): the player's Research Lab, such as "RESEARCH LAB" or "NO RESEARCH LAB", its
  // queue, and the topics, tier by tier, of which TOPICS_SHOWN are shown from firstTopic.
  struct ResearchPanel
  {
    std::string lab;
    bool hasLab = false;
    std::int32_t ore = 0;
    std::vector<QueueLine> queue;
    std::vector<TopicCard> topics;
    std::size_t firstTopic = 0;
  };

  // How many topic cards the research window shows at once, in rows of TOPIC_COLUMNS; it scrolls by a row.
  static constexpr std::size_t TOPIC_COLUMNS = 2;
  static constexpr std::size_t TOPICS_SHOWN = 10;

  // Whose a minimap mark is, which sets its color.
  enum class Side : std::uint8_t
  {
    Own,
    Enemy,
    Neutral
  };

  // One entity on the minimap: a ship as a dot, a structure as a larger square, an asteroid or field at its size.
  struct Mark
  {
    PlanePosition position;
    float radiusMeters = 0.0f;
    Side side = Side::Neutral;
    EntityKind kind = EntityKind::Ship;
    // An ore asteroid that has run out, as far as the player knows: drawn darker (Phase 1 design §8).
    bool dry = false;
  };

  // One of the map's sectors on the minimap (ADR-056): tinted by whose it is, outlined in that side's color over the fog,
  // since who holds what is known to both sides, and hatched while suppressed.
  struct SectorMark
  {
    float minXMeters = 0.0f;
    float maxXMeters = 0.0f;
    float minZMeters = 0.0f;
    float maxZMeters = 0.0f;
    Side side = Side::Neutral;
    bool suppressed = false;
  };

  // How many nodes each side holds, of how many the map has (Phase 2 design §4), and each side's tickets (§8). With them,
  // how many the player's Command Station lets it hold (Phase 3 design §7), shown against its own; zero for none.
  struct Territory
  {
    std::int32_t ownNodes = 0;
    std::int32_t enemyNodes = 0;
    std::int32_t nodes = 0;
    std::int32_t cap = 0;
    std::optional<std::int32_t> ownTickets;
    std::optional<std::int32_t> enemyTickets;
  };

  // How the match ended for the player (design §6): "Victory", "Defeat" or "Draw", and how long it lasted.
  struct Outcome
  {
    std::string title;
    std::string detail;
  };

  // What the HUD shows, in words and marks.
  struct Content
  {
    std::int32_t ore = 0;
    std::int32_t oreIncomeHundredthsPerSecond = 0;
    // The selection panel's lines, first to last; none when nothing is selected. With them, the share of its hit points
    // the selection has left, for a bar under them (ADR-046).
    std::vector<std::string> selection;
    std::optional<float> selectionHealth;
    std::vector<Button> buttons;
    // A line at the top while a structure's placement is armed.
    std::string hint;
    // A line under the Ore while the player's Research Lab has a topic (task 5.1).
    std::string research;
    std::optional<DesignerPanel> designer;
    // The production and research windows' content, while they are open; GameClient fills them.
    std::optional<ProductionPanel> production;
    std::optional<ResearchPanel> laboratory;
    // No minimap when the map's size is not known.
    float mapSizeMeters = 0.0f;
    std::vector<Mark> marks;
    // The map's sectors, and the nodes each side holds; none on a map without sectors.
    std::vector<SectorMark> sectors;
    std::optional<Territory> territory;
    // The alerts to show, newest first, each with where it happened, for the minimap (ADR-059); GameClient fills them.
    std::vector<std::pair<std::string, PlanePosition>> alerts;
    // Under fog of war, the fog is drawn over the marks (ADR-024): one panel over the minimap, which the client fills
    // from the fog's own texture (ADR-052).
    bool fog = false;
    // Once the match is over, a banner with a button back to the menu; the world runs on behind it (owner, 2026-10-01).
    std::optional<Outcome> outcome;
  };

  // How a panel is filled: solid, or with diagonal stripes, as a window's title bar is (ADR-030).
  enum class Fill : std::uint8_t
  {
    Solid,
    Hatched,
    // The fog's shades over the minimap: the color, as opaque as each point's shade (ADR-052).
    Fog
  };

  // A hatched panel's stripes: this wide, this far apart, in reference units.
  static constexpr float HATCH_STRIPE_UNITS = 2.0f;
  static constexpr float HATCH_PERIOD_UNITS = 7.0f;

  // A window's title bar, which it is dragged by, and its close box at the bar's right end; and how much of a window's
  // width stays on the screen however far it is dragged, so that its title bar can always be taken hold of again.
  static constexpr float TITLE_BAR_UNITS = 36.0f;
  static constexpr float WINDOW_KEPT_ON_SCREEN_UNITS = 120.0f;

  struct Rect
  {
    float left = 0.0f;
    float top = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    DirectX::XMFLOAT4 color{};
    Fill fill = Fill::Solid;

    [[nodiscard]] bool Contains(float _xPixels, float _yPixels) const noexcept
    {
      return _xPixels >= left && _xPixels < left + width && _yPixels >= top && _yPixels < top + height;
    }
  };

  // A line of text, in UTF-8, in a typeface and with extra space between its letters.
  struct Text
  {
    std::string text;
    float left = 0.0f;
    float top = 0.0f;
    DirectX::XMFLOAT4 color{};
    Typeface typeface = Typeface::Body;
    float trackingPixels = 0.0f;
  };

  // A sprite drawn into a rectangle, in the rectangle's color, mirrored when asked (ADR-030).
  struct SpriteMark
  {
    Sprite sprite = Sprite::OreMark;
    Rect area;
    bool mirrorX = false;
    bool mirrorY = false;
  };

  // A floating window as laid out (ADR-031): its kind; where it stands, its title bar and its close box, in pixels; its
  // top-left corner in reference units, as WindowManager::Grab takes it; and where its panels, texts, sprites and
  // buttons start in the layout's lists. Each runs to where the next window's starts, or to the list's end.
  struct Window
  {
    WindowKind kind = WindowKind::Designer;
    Rect frame;
    Rect titleBar;
    Rect closeBox;
    WindowManager::Point corner;
    std::size_t firstPanel = 0;
    std::size_t firstText = 0;
    std::size_t firstSprite = 0;
    std::size_t firstAction = 0;
  };

  // A layer's share of one of the layout's lists, from first up to end.
  struct Span
  {
    std::size_t first = 0;
    std::size_t end = 0;
  };

  // The HUD on a back buffer of one size, in its pixels. It is drawn in layers: the HUD, then each window back to front,
  // each layer's panels, then its sprites, then its texts. A click belongs to the front layer under it.
  struct Layout
  {
    float fontPixels = 0.0f;
    std::vector<Rect> panels;
    std::vector<Text> texts;
    // Where each enabled button is, for clicks.
    std::vector<std::pair<Rect, Action>> actions;
    // The minimap's drawing area, the map's square; empty when there is no minimap.
    Rect minimap;
    float mapSizeMeters = 0.0f;
    std::vector<SpriteMark> sprites;
    // Back to front, the order they are drawn in.
    std::vector<Window> windows;

    // Layer 0 is the HUD and layer i + 1 the window windows[i]; there are windows.size() + 1.
    [[nodiscard]] std::size_t LayerCount() const noexcept
    {
      return windows.size() + 1;
    }
    // The front layer under a point: the front window's that holds it, or the HUD's.
    [[nodiscard]] std::size_t LayerAt(float _xPixels, float _yPixels) const noexcept;
    [[nodiscard]] Span PanelsOf(std::size_t _layer) const noexcept;
    [[nodiscard]] Span TextsOf(std::size_t _layer) const noexcept;
    [[nodiscard]] Span SpritesOf(std::size_t _layer) const noexcept;
    [[nodiscard]] Span ActionsOf(std::size_t _layer) const noexcept;
    // The front window under a point, if any.
    [[nodiscard]] const Window* WindowAt(float _xPixels, float _yPixels) const noexcept;

    // Whether a point in back-buffer pixels is on a panel, where a click belongs to the HUD.
    [[nodiscard]] bool Covers(float _xPixels, float _yPixels) const noexcept;
    // The enabled button of the front layer under a point, if any: a button of the HUD under a window takes no click.
    [[nodiscard]] std::optional<Action> ActionAt(float _xPixels, float _yPixels) const noexcept;
    // The point on the map under a point on the minimap, if it is on the minimap.
    [[nodiscard]] std::optional<PlanePosition> MapPointAt(float _xPixels, float _yPixels) const noexcept;
    // Where a point on the map shows on the minimap, in pixels.
    [[nodiscard]] DirectX::XMFLOAT2 MinimapPixelOf(PlanePosition _point) const noexcept;
  };

  // The content for _player: its Ore and income from the newest snapshot, its research, a description of _selected, by
  // design name from the snapshot's designs, the buttons the selection offers, and the minimap's marks. _placing is the
  // structure being placed, if any. With a _designer, which GameClient gives while its window is open, the designer;
  // _hovered is the button under the pointer, and a part's previews the design it would make.
  [[nodiscard]] static Content Describe(const Snapshot& _newest, std::span<const EntityView> _entities, std::span<const EntityId> _selected,
                                        std::optional<StructureKind> _placing = std::nullopt, const Designer* _designer = nullptr,
                                        std::optional<Action> _hovered = std::nullopt);

  // The production window's content for _producer, one of the player's finished producers, or nullptr while it has none
  // (Phase 1 design §12).
  [[nodiscard]] static ProductionPanel DescribeProduction(const Snapshot& _newest, const EntityView* _producer);

  // The research window's content: the player's Research Lab among _entities, its queue, and the topics from _firstTopic
  // (Phase 1 design §12).
  [[nodiscard]] static ResearchPanel DescribeResearch(const Snapshot& _newest, std::span<const EntityView> _entities,
                                                      std::size_t _firstTopic = 0);

  // The first topic shown after scrolling a row forward, or back with a negative _step, among _topics: never before the
  // first, nor past where the last row is shown.
  [[nodiscard]] static std::size_t StepTopics(std::size_t _firstTopic, int _step, std::size_t _topics) noexcept;

  // How the match in _newest ended for its player, the length counted at _ticksPerSecond; nothing while it runs.
  [[nodiscard]] static std::optional<Outcome> DescribeOutcome(const Snapshot& _newest, std::uint32_t _ticksPerSecond);

  // The main menu on a back buffer of this size, its text measured with _metrics: the game's name, and buttons to start a
  // skirmish against the AI and to quit (task 6.2).
  [[nodiscard]] static Layout LayMenu(const TextMetrics& _metrics, std::uint32_t _widthPixels, std::uint32_t _heightPixels);

  // Where everything goes on a back buffer of this size, its text measured with _metrics, which are the fonts at this
  // size's scale. _view is the ground the camera shows, its corners in order, outlined on the minimap; empty when the
  // camera sees past the horizon. The floating windows (ADR-031) are those _windows has open, in its order and where it
  // left them; without a manager, every window the content has, at its default place.
  [[nodiscard]] static Layout Lay(const Content& _content, const TextMetrics& _metrics, std::uint32_t _widthPixels,
                                  std::uint32_t _heightPixels, std::span<const PlanePosition> _view = {},
                                  const WindowManager* _windows = nullptr);

  // Where a window _widthUnits wide may stand with its top-left corner at _corner on a screen of this size, in reference
  // units: moved only as far as keeps its title bar on the screen, and WINDOW_KEPT_ON_SCREEN_UNITS of its width.
  [[nodiscard]] static WindowManager::Point KeepOnScreen(WindowManager::Point _corner, float _widthUnits, float _screenWidthUnits,
                                                         float _screenHeightUnits) noexcept;

  // The scale from reference units to pixels: the largest at which the whole reference frame fits (ADR-006).
  [[nodiscard]] static float Scale(std::uint32_t _widthPixels, std::uint32_t _heightPixels) noexcept;
};

// _value with a comma between each group of three digits, such as "12,000".
[[nodiscard]] std::string WithThousands(std::int64_t _value);

// A length of time as minutes and seconds, "6:13", with hours in front once there are any, "1:02:03".
[[nodiscard]] std::string MinutesAndSeconds(std::uint64_t _seconds);
} // namespace Outpost