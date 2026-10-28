#pragma once

namespace Outpost
{
// How strong an AI the player meets, picked on the main menu (ADR-065): each is a settings file of the AI's own.
enum class Difficulty : std::uint8_t
{
  Easy,
  Normal,
  Hard
};

// The HUD (Game §interface.hud): the Ore stockpile and income, a panel describing the selection with a
// structure's construction, the command card that gives orders, shows what Constructors build and opens a structure's
// windows, and the minimap. It is
// laid out once in 1920×1080 reference units, each element anchored to a corner or an edge, and scaled to the back buffer
// by one uniform factor (ADR-006). It keeps no GPU state: it says what to draw, in pixels, and GameClient draws it through
// the UI pipeline (ADR-015). Clicks on it do not reach the world. Research shows under the Ore (ADR-017 decision 9). Over
// it float the designer, the production queue and the research as windows, among others (ADR-031; Game
// §interface.windows). The main
// menu, and the banner that says how a match ended, are laid out the same way (ADR-020 decisions 9 and 10).
class Hud
{
public:
  static constexpr float REFERENCE_WIDTH_UNITS = 1920.0f;
  static constexpr float REFERENCE_HEIGHT_UNITS = 1080.0f;
  // The text's size at the reference scale.
  static constexpr float FONT_UNITS = 20.0f;

  // The interface's fonts (ADR-030), in the order GameClient builds the UI pipeline with them: Segoe UI, a text's default
  // face, and the faces of the owner's mockup (ADR-030 decision 1), which the windows and the HUD take (ADR-043): condensed
  // Bahnschrift for titles, labels and names, and Cascadia Mono for figures, or Consolas where it is not installed, with a
  // smaller size for a part card's numbers; and the count on a fleet's plate, in Bahnschrift's bold semicondensed face with
  // a halo, since it stands on the scene with no panel behind it (ADR-102 decision 2).
  enum class Typeface : std::uint8_t
  {
    Body,
    Title,
    Label,
    Name,
    Figure,
    LargeFigure,
    Detail,
    Plate
  };

  // Each typeface's size in reference units, which are pixels at 1080p, in Typeface's order (ADR-062). It is the one place
  // a face's size is set: Typefaces rasterizes the faces at these sizes, and the layout sizes the gem beside a figure
  // from them.
  static constexpr std::array<float, 8> FACE_UNITS{FONT_UNITS, 22.0f, 13.0f, 16.0f, 14.0f, 28.0f, 13.0f, 15.0f};

  [[nodiscard]] static constexpr float FaceUnits(Typeface _face) noexcept
  {
    return FACE_UNITS[static_cast<std::size_t>(_face)];
  }

  [[nodiscard]] static std::vector<Neuron::FontDesc> Typefaces();

  // The interface's sprites (ADR-030), in the order GameClient builds the UI pipeline with them: Ore's gem, the box of
  // a part research has yet to unlock, a window's corner bracket, and a fleet plate's ring, its disc and the insignia on
  // it: one, two or three chevrons for its hull and a plus for a Constructor (ADR-102 decision 2).
  enum class Sprite : std::uint8_t
  {
    OreMark,
    Checkbox,
    Corner,
    PlateRing,
    PlateDisc,
    OneChevron,
    TwoChevrons,
    ThreeChevrons,
    Plus
  };

  [[nodiscard]] static std::vector<Neuron::SpriteDesc> Sprites();

  // A typeface's index among the atlas's fonts, and a sprite's among its sprites, as the display list names them (ADR-100).
  [[nodiscard]] static constexpr std::size_t FontOf(Typeface _face) noexcept
  {
    return static_cast<std::size_t>(_face);
  }
  [[nodiscard]] static constexpr std::size_t SpriteOf(Sprite _sprite) noexcept
  {
    return static_cast<std::size_t>(_sprite);
  }

  // The game's look as the canvas's theme (ADR-100): its windows' colors, typefaces and sizes, which the overseer is drawn in
  // too (ADR-103).
  [[nodiscard]] static const Neuron::Theme& Theme();

  // How wide the interface's text is set, as the atlas it is drawn from sets it (ADR-061): the engine's measure (ADR-100) of
  // the fonts Typefaces names, in its order, rasterized for a back buffer at _scale pixels to a reference unit, each named
  // by its typeface. The layout measures every line it places with it, so that a line is cut short, wrapped or given room
  // by what it takes on the screen. It reads the fonts where they are, so they must outlive it.
  class TextMetrics : public Neuron::TextMetrics
  {
  public:
    using Neuron::TextMetrics::TextMetrics;

    // How wide _text is set in _face, with _trackingUnits more between each two characters, in reference units, as
    // Neuron::UiPipeline::DrawText sets it: each character at its whole-pixel advance, and the tracking at a whole pixel.
    [[nodiscard]] float Width(Typeface _face, std::string_view _text, float _trackingUnits = 0.0f) const noexcept
    {
      return Neuron::TextMetrics::Width(static_cast<std::size_t>(_face), _text, _trackingUnits);
    }

    // _text as _face sets it within _widthUnits: whole when it fits, and otherwise cut short with three dots.
    [[nodiscard]] std::string Fit(Typeface _face, std::string_view _text, float _widthUnits, float _trackingUnits = 0.0f) const
    {
      return Neuron::TextMetrics::Fit(static_cast<std::size_t>(_face), _text, _widthUnits, _trackingUnits);
    }

    // _text broken between its words into lines that _face sets within _widthUnits; a word wider than that is cut short.
    [[nodiscard]] std::vector<std::string> Wrap(Typeface _face, std::string_view _text, float _widthUnits) const
    {
      return Neuron::TextMetrics::Wrap(static_cast<std::size_t>(_face), _text, _widthUnits);
    }
  };

  // What a button does when it is pressed.
  enum class ActionKind : std::uint8_t
  {
    // Arms placing a structure for the selected Constructors.
    Build,
    // Turns the command card's build mode on and off: the structures the selected Constructors build in place of its
    // commands (ADR-088 decision 2).
    OpenBuild,
    // Queues a job at a Shipyard or the Command Station.
    Queue,
    // Queues a topic at the Research Lab.
    Research,
    // The designer: picks a component for a slot, or saves the design.
    PickHull,
    PickDrive,
    PickWeapon,
    // The module, or none (ADR-058 decision 7).
    PickModule,
    SaveDesign,
    // Queues the designer's picks at a Shipyard when they are no saved design yet: saves them first (ADR-023).
    SaveAndQueue,
    // The main menu's and the match end's (ADR-020 decisions 9 and 10): start a match against the AI at a difficulty
    // (ADR-065), leave the game, or leave the match; and the menu's join of a universe on another machine (ADR-078).
    StartSkirmish,
    Quit,
    BackToMenu,
    JoinUniverse,
    // The leave window's Stay, which closes it; its Leave universe is BackToMenu (ADR-078 decision 11).
    Stay,
    // The designer's window (ADR-017 decision 7): open it aimed at a Shipyard, step its target Shipyard, ask for fewer or
    // more ships, load a saved design, and scroll the saved designs.
    OpenDesigner,
    PreviousShipyard,
    NextShipyard,
    FewerShips,
    MoreShips,
    LoadDesign,
    PreviousDesigns,
    NextDesigns,
    // The production and research windows (Game §interface.windows): open them, step the production window's producer, and
    // scroll the research window's topics.
    OpenProduction,
    OpenResearch,
    PreviousProducer,
    NextProducer,
    PreviousTopics,
    NextTopics,
    // Upgrades the selected structure by one level (ADR-064 decisions 2 and 7).
    Upgrade,
    // The designer's retreat is set to one, and a selection of ships is set to one (ADR-075 decision 7), each from a
    // row of three (ADR-088 decision 3).
    DesignRetreat,
    SetRetreat,
    // The orders window's form steps a field, or picks its value from its drop-down, and gives its order to the selection,
    // and the window cancels one of the seat's scheduled orders (ADR-080 decision 9).
    StepOrder,
    PickOrder,
    GiveOrder,
    CancelOrder,
    // Selects one of the player's own entities on its own and moves the camera to it (ADR-066 decision 1; ADR-088 decision 4).
    Select,
    // A warship's orders, as their keys give them (ADR-088 decision 2): arm attack-move, holding a sector or a
    // patrol for the next left-click, or stop the selected ships at once.
    AttackMove,
    HoldSector,
    Patrol,
    Stop
  };

  struct Action
  {
    ActionKind kind = ActionKind::Build;
    StructureKind structure = StructureKind::CommandStation;
    EntityId producer;
    // The entity Select selects, or the target PickOrder picks.
    EntityId entity;
    // The design a Shipyard builds; no design for the Command Station's Constructor.
    DesignId design;
    ResearchTopicId topic;
    HullId hull;
    DriveId drive;
    WeaponId weapon;
    // A module to pick, or none.
    ModuleId module;
    // How many ships a Queue asks for (ADR-017 decision 7).
    std::uint32_t count = 1;
    // The AI a skirmish is against (ADR-065).
    Difficulty difficulty = Difficulty::Normal;
    // What SetRetreat sets the selected ships to.
    RetreatThreshold retreat = DEFAULT_RETREAT;
    // The field StepOrder steps, and which way: forward for a positive step; or the field PickOrder sets, and to what, an
    // hour, a minute or a sector's id.
    OrderField field = OrderField::Trigger;
    std::int32_t step = 0;
    std::int32_t value = 0;
    // The scheduled order CancelOrder cancels.
    std::uint32_t order = 0;

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
    // (ADR-043 decision 4).
    std::string note;
    // How far the work the button started has come, in thousandths, drawn as a bar along its foot from left to right; none
    // while no such work is under way.
    std::optional<std::int32_t> progressPermille;
    // The key that does what the button does, drawn as a cap at its right end (ADR-068 decision 4); none for most buttons.
    std::string key;
    // On the command card, the cell it takes, numbered left to right and top to bottom, and whether it takes its row's
    // every cell (ADR-088 decision 2). A window's buttons take neither.
    std::uint8_t cell = 0;
    bool wide = false;
  };

  // The command card's cells (ADR-088 decision 2): CARD_COLUMNS to a row, CARD_CELLS in all.
  static constexpr std::uint8_t CARD_COLUMNS = 2;
  static constexpr std::uint8_t CARD_CELLS = 6;

  // A component as the designer shows it on its card (ADR-017 decision 7): its name, cost and numbers, a note under them,
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
  // components included, so that the scale holds as research unlocks parts. With a part hovered, the design it would make:
  // its figure with the change as a signed number, "242 (+44)" (ADR-017 decision 7).
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

  // Damage per second after armor against one hull (ADR-017 decision 7), per ship and per 100 Ore: the hovered part's design
  // while one is hovered, how it compares to the picked one, and its change per ship as a signed number, "+4.5".
  struct DamageCard
  {
    std::string hull;
    std::string armor;
    std::string perShip;
    std::string perShipChange;
    // How many ships of a formation of the hull one hit reaches, "×5 ships", for a splash that reaches more than its target.
    std::string reach;
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

  // The designer's window, after the owner's mockup (Game §interface.windows.designer; Docs/Mockups/ShipDesigner.png).
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
    // Whether the server takes the name (IsValidDesignName); it is drawn as a warning when not.
    bool nameValid = true;
    // SAVE for a new design, SAVED when the name and the parts are a saved design; RENAME for a saved design's new name.
    Button save;
    Button rename;
    // The design's retreat, drawn as a row of three, its setting lit (ADR-088 decision 3).
    RetreatThreshold retreat = DEFAULT_RETREAT;
    // The saved designs, and the first shown of those that do not all fit.
    std::vector<DesignChip> chips;
    std::size_t firstChip = 0;
    // Hull, drive, weapon and module (ADR-058 decision 7).
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
    // A waiting warship's wait is for its player's fleet cap, not for Ore (ADR-071 decision 4).
    bool capped = false;
  };

  // How a design does against one hull, rated as the designer rates its damage (ADR-017 decision 7): the hull's initial,
  // and the rating.
  struct HullRating
  {
    std::string hull;
    Rating rating = Rating::Poor;
  };

  // A button of a window that adds to a queue: what it adds, a line under the name, such as a design's abbreviation, its
  // cost, and whether it can be pressed now; and under them how long one takes to build and, for a design, how it does
  // against each hull (ADR-068).
  struct QueueOption
  {
    std::string name;
    std::string detail;
    std::int32_t cost = 0;
    Action action;
    bool enabled = true;
    std::string time;
    std::vector<HullRating> strengths;
  };

  // The production window (ADR-043 decision 6; owner, 2026-10-03): the producer it shows, such as "COMMAND STATION" or
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

  // A research topic not researched or queued yet, as the research window shows it (ADR-017 decision 9): what it does, its
  // cost, its tier and time, and in capitals what it needs: the Research Lab's level, while its tier is not open
  // (ADR-033 decision 5), and each prerequisite neither researched nor queued. A topic that needs any is dim.
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

  // The research window (Game §interface.windows.research): the player's Research Lab, such as "RESEARCH LAB" or
  // "NO RESEARCH LAB", its queue, and the topics, tier by tier, of which TOPICS_SHOWN are shown from firstTopic.
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

  // Whose a minimap mark is to the player. A player's marks are in its color, and the pirates' in theirs.
  enum class Side : std::uint8_t
  {
    Own,
    Enemy,
    // The pirates' (ADR-073): a sector they guard, and their ships and structures.
    Pirate,
    Neutral
  };

  // A player's color on the HUD: its set's in Models.json, the color the world draws its ships and structures in, whichever
  // seat views them (Game §look.models.sets).
  struct SideColor
  {
    PlayerId player;
    DirectX::XMFLOAT4 color{};
  };

  // One entity on the minimap: a ship as a dot, a structure as a larger square, an asteroid or field at its size.
  struct Mark
  {
    PlanePosition position;
    float radiusMeters = 0.0f;
    Side side = Side::Neutral;
    PlayerId owner;
    EntityKind kind = EntityKind::Ship;
    // An ore asteroid that has run out, as far as the player knows: drawn darker (ADR-035 decision 5).
    bool dry = false;
    // A structure or derelict the player only remembers (ADR-024): drawn over the fog as a cross, so that it differs from
    // what is seen by its shape as well as from an ore asteroid's outline (ADR-081 decision 4).
    bool remembered = false;
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
    PlayerId holder;
    bool suppressed = false;
    // Cut off from its holder's home, which its outline's dashes say (ADR-081 decision 7).
    bool cutOff = false;
    // Whether the player is placing a Relay and could claim the sector's node now, by the Relay ghost's rule (CanClaim).
    bool claimable = false;
  };

  // How many nodes each side holds, of how many the map has, and each side's tickets (ADR-057 decision 8). With them,
  // how many the player's Command Station lets it hold (ADR-064 decision 11), shown against its own; zero for none.
  struct Territory
  {
    std::int32_t ownNodes = 0;
    std::int32_t enemyNodes = 0;
    std::int32_t nodes = 0;
    std::int32_t cap = 0;
    std::optional<std::int32_t> ownTickets;
    std::optional<std::int32_t> enemyTickets;
    // With the tickets, who the drain takes them from, how fast, and when they run out, "Enemy -20 a minute · out in 32:06",
    // or "No drain" while both hold as many nodes; a warning chip when it is the player's (ADR-057 decision 8).
    std::string drain;
    bool drainWarns = false;
    // The player the enemy's figures are; their color is its.
    PlayerId enemy;
  };

  // Where the match runs, as the tag in the top-right corner says it (ADR-086): on the game's own server, or on a dedicated
  // server at its address; and how long no snapshot has come, which the tag warns of from SILENT_SECONDS.
  struct Connection
  {
    // The dedicated server's address as ServerName writes it; empty for a match on the game's own server.
    std::string server;
    // Whole seconds since the last snapshot came.
    std::int32_t silentSeconds = 0;
    // The connection's smoothed round trip in whole milliseconds, a universe's once it has one (ADR-086 decision 1).
    std::optional<std::int32_t> roundTripMilliseconds;
    // While a universe's lost connection is joined again, whole seconds since it was lost (ADR-078 decision 10).
    std::optional<std::int32_t> reconnectingSeconds;
  };
  // How long no snapshot may come, at 20 a second, before the tag warns of it (ADR-086).
  static constexpr std::int32_t SILENT_SECONDS = 2;

  // A run of a status line's words: plain, in the text's color, or a warning chip, dark on a tag of the warning's color
  // (ADR-085 decision 1).
  struct StatusRun
  {
    std::string text;
    bool chip = false;

    friend bool operator==(const StatusRun&, const StatusRun&) = default;
  };

  // A status line's bar (ADR-066 decision 3): the share of it filled, from 0 to 1, and the figure at its end, such
  // as "+3" queued or "29 / 30". Under its line's words, the width of the panel, or beside them, from their end.
  struct StatusBar
  {
    float share = 0.0f;
    std::string figure;
    bool under = false;

    friend bool operator==(const StatusBar&, const StatusBar&) = default;
  };

  // A line of the status panel under the Ore (ADR-066): what the Research Lab or the Shipyards are doing, an idle one's
  // IDLE a chip, a bar for how far a job has come or how full the fleet is, and what a click on it does.
  struct StatusLine
  {
    std::vector<StatusRun> runs;
    Action action;
    std::optional<StatusBar> bar;

    // Its runs' words, one after another.
    [[nodiscard]] std::string Text() const;
    // Whether a run of it is a chip.
    [[nodiscard]] bool Warns() const noexcept;
  };

  // A row of the orders window's form: what it sets, its value between the arrows that step it, and the field they step;
  // and, for a field picked from a drop-down, its choices and the one it holds (ADR-080 decision 9).
  struct OrderRow
  {
    std::string label;
    std::string value;
    OrderField field = OrderField::Trigger;
    std::vector<OrderChoice> choices;
    std::optional<std::size_t> picked;
  };

  // One of the seat's scheduled orders as the orders window lists it, with the order its cancel button cancels.
  struct ScheduledLine
  {
    std::string text;
    std::uint32_t order = 0;
  };

  // The orders window (ADR-080 decision 9): the seat's scheduled orders, the first ORDERS_SHOWN of them, each
  // with how many ships still wait on it and a button that cancels it, and how many more there are; and the form that gives
  // the selection a new one, its rows, the line that says when it fires, and the button that gives it, with why it cannot
  // when it cannot.
  struct OrdersPanel
  {
    std::vector<ScheduledLine> scheduled;
    std::size_t more = 0;
    std::vector<OrderRow> rows;
    std::string fires;
    Button give;
  };

  static constexpr std::size_t ORDERS_SHOWN = 6;
  // The most rows the form has: the trigger, its hour and minute, the action, its sector and its target, and the condition.
  static constexpr std::size_t ORDER_ROWS = 7;

  // What happened while the player was away (ADR-080 decision 7): how long the deputy played its seat, and a line for each
  // of what was built and lost, the sectors gained and lost, and each order that fired.
  struct AwayPanel
  {
    std::string since;
    std::vector<std::string> lines;
  };

  // The command card in build mode (ADR-088 decision 2): a button for each structure the selected Constructors build, in its
  // cell, with its cost, or why it cannot be built in its cost's place (ADR-043 decision 4).
  struct BuildPanel
  {
    std::vector<Button> structures;
  };

  // The away window's lines at most: built, lost, gained and lost sectors, and every order its report keeps.
  static constexpr std::size_t AWAY_LINES = 4 + AwayReport::REPORTED_ORDERS;

  // How the match ended for the player (ADR-020 decision 9): "Victory", "Defeat" or "Draw", and how long it lasted.
  struct Outcome
  {
    std::string title;
    std::string detail;
  };

  // A selection's retreat, as the row of three under its buttons shows it (ADR-088 decision 3): the first ship's
  // setting, and whether the others differ, which lights none and says MIXED.
  struct RetreatChoice
  {
    RetreatThreshold setting = DEFAULT_RETREAT;
    bool mixed = false;
  };

  // A ship's own bar under a selection of two to SHIP_BARS_MOST ships (ADR-088 decision 4): the ship a click on
  // it selects alone, the share of its hit points it has left, and whether it is going back to be repaired, which its
  // outline marks.
  struct ShipBar
  {
    EntityId ship;
    float share = 1.0f;
    bool retreating = false;
  };

  static constexpr std::size_t SHIP_BARS_MOST = 24;

  // What a fleet's plate wears on its disc (ADR-102 decision 2): its commonest kind of ship, a warship's hull by its size
  // among the match's hulls, smallest first, or a Constructor.
  enum class Insignia : std::uint8_t
  {
    OneChevron,
    TwoChevrons,
    ThreeChevrons,
    Constructor
  };

  // A plate over a fleet on the screen (ADR-102 decision 2): the fleet a click on it selects, whose it is, the point it
  // stands on, as ScreenFleets finds it, in back-buffer pixels; how many ships it holds, its insignia, the share of its hit
  // points left, which its ring's arc shows, its side's color, and whether it is the player's own, which a click selects.
  struct Plate
  {
    std::uint32_t fleet = 0;
    PlayerId owner;
    DirectX::XMFLOAT2 footPixels{};
    std::uint32_t count = 0;
    Insignia insignia = Insignia::OneChevron;
    float health = 1.0f;
    DirectX::XMFLOAT4 color{};
    bool own = false;
  };

  // A plate's size, in reference units at the interface's scale, which its count's face is drawn at: its disc and the ring
  // round it, and the count's line over them (ADR-102 decision 2).
  static constexpr float PLATE_WIDTH_UNITS = 28.0f;
  static constexpr float PLATE_COUNT_UNITS = 18.0f;
  static constexpr float PLATE_HEIGHT_UNITS = PLATE_WIDTH_UNITS + PLATE_COUNT_UNITS;
  // Plates stand over the fleets while the camera's view is wider than this; closer in, each ship shows its own bar as a
  // structure does (ADR-102 decision 3).
  static constexpr float PLATES_FROM_VIEW_WIDTH_METERS = 300.0f;

  // What shows a bar over a ship or a structure (ADR-102 decision 3): Alt held, which shows every bar; the camera close in,
  // where a hurt ship shows its bar as a structure always does; and the ship under the pointer, which shows its own.
  struct BarRule
  {
    bool every = false;
    bool closeIn = true;
    bool pointed = false;
  };

  // The bars over an entity on the screen (ADR-088 decision 5): the point they stand a little above, the top of the
  // entity's model on the screen in its column, which GameClient finds with the camera, in back-buffer pixels; the share of
  // hit points left, while the health bar shows; the share built, while the build bar shows; and their back's color, the
  // side's darkened.
  struct EntityBar
  {
    DirectX::XMFLOAT2 footPixels{};
    std::optional<float> health;
    std::optional<float> built;
    DirectX::XMFLOAT4 back{};
  };

  // What the HUD shows, in words and marks.
  struct Content
  {
    std::int32_t ore = 0;
    std::int32_t oreIncomeHundredthsPerSecond = 0;
    // The selection panel's lines, first to last; none when nothing is selected. With them, under the lines, a bar for
    // each ship of a selection of two to SHIP_BARS_MOST, or else the share of its hit points the whole selection has
    // left, for one bar (ADR-088 decision 4).
    std::vector<std::string> selection;
    std::optional<float> selectionHealth;
    std::vector<ShipBar> shipBars;
    // The command card's buttons, each in its cell (ADR-088 decision 2).
    std::vector<Button> buttons;
    // The selection's retreat, at the foot of the command card, while it holds a ship (ADR-088 decision 3).
    std::optional<RetreatChoice> retreat;
    // A line at the top while a structure's placement is armed.
    std::string hint;
    // The status panel's lines under the Ore (ADR-066): the Research Lab's once the player has a finished one, and the
    // Shipyards' and the fleet's once the first Shipyard is finished.
    std::vector<StatusLine> status;
    // The match's research topics' names, which the column under the Ore is made wide enough for, so that its edge stands
    // still whatever the Lab researches (ADR-066 decision 3).
    std::vector<std::string> researchNames;
    std::optional<DesignerPanel> designer;
    // The production and research windows' content, while they are open; GameClient fills them.
    std::optional<ProductionPanel> production;
    std::optional<ResearchPanel> laboratory;
    // Whether the Controls window is open (ADR-068 decision 4); its lines are KeyBindings'.
    bool controls = false;
    // The orders window's content and the away window's, while they are open (ADR-080 decision 9); GameClient fills them.
    std::optional<OrdersPanel> orders;
    std::optional<AwayPanel> away;
    // The structures the command card shows in place of its buttons and the retreat while it is in build mode; GameClient
    // fills it (DescribeBuild).
    std::optional<BuildPanel> build;
    // Whether the leave window is open, in a universe (ADR-078 decision 11).
    bool leave = false;
    // No minimap when the map's size is not known.
    float mapSizeMeters = 0.0f;
    std::vector<Mark> marks;
    // The player the HUD is for, and each player's color, which GameClient fills (SideColorsOf). A player with no color is
    // drawn in the neutral marks' gray.
    PlayerId player;
    std::vector<SideColor> sideColors;
    // The map's sectors, and the nodes each side holds; none on a map without sectors.
    std::vector<SectorMark> sectors;
    std::optional<Territory> territory;
    // The alerts to show, newest first, each with where it happened, if anywhere, for the minimap (ADR-059, ADR-094);
    // GameClient fills them.
    std::vector<std::pair<std::string, std::optional<PlanePosition>>> alerts;
    // Under fog of war, the fog is drawn over the marks (ADR-024): one panel over the minimap, which the client fills
    // from the fog's own texture (ADR-052).
    bool fog = false;
    // Once the match is over, a banner with a button back to the menu; the simulation runs on behind it (owner, 2026-10-01).
    std::optional<Outcome> outcome;
    // Where the match runs; GameClient fills it (ADR-086).
    std::optional<Connection> connection;
    // The bars over the entities that show one; GameClient fills them (ADR-088 decision 5).
    std::vector<EntityBar> entityBars;
    // The plates over the fleets on the screen; GameClient fills them (ADR-102 decision 2).
    std::vector<Plate> plates;
  };

  // How a panel is filled, as the engine's display list has it (ADR-100): solid; with diagonal stripes, as a window's title
  // bar is (ADR-030); or with the image, the fog's shades over the minimap, the color as opaque as each point's shade
  // (ADR-052).
  using Fill = Neuron::CanvasFill;

  // A hatched panel's stripes: this wide, this far apart, in reference units.
  static constexpr float HATCH_STRIPE_UNITS = 2.0f;
  static constexpr float HATCH_PERIOD_UNITS = 7.0f;

  // A window's title bar, which it is dragged by, and its close box at the bar's right end; and how much of a window's
  // width stays on the screen however far it is dragged, so that its title bar can always be taken hold of again.
  static constexpr float TITLE_BAR_UNITS = 36.0f;
  static constexpr float WINDOW_KEPT_ON_SCREEN_UNITS = 120.0f;

  // The display list's parts are the engine's (ADR-100), in pixels: a rectangle; a line of text, in a typeface by its index
  // in Typeface; a sprite, by its index in Sprite, drawn into a rectangle in the rectangle's color and mirrored when asked
  // (ADR-030); a solid line at any angle, such as the minimap's view (ADR-015); and a layer's share of one of the lists.
  using Rect = Neuron::CanvasRect;
  using Text = Neuron::CanvasText;
  using SpriteMark = Neuron::CanvasSprite;
  using Line = Neuron::CanvasLine;
  using Span = Neuron::CanvasSpan;

  // A floating window as laid out (ADR-031): its kind; where it stands, its title bar and its close box, in pixels; its
  // top-left corner in reference units, as WindowManager::Grab takes it; and where its buttons start among the layout's
  // actions, running to where the next window's start, or to the list's end. Its panels, lines, texts, sprites and
  // regions are its layer's in the display list.
  struct Window
  {
    WindowKind kind = WindowKind::Designer;
    Rect frame;
    Rect titleBar;
    Rect closeBox;
    WindowManager::Point corner;
    std::size_t firstAction = 0;
  };

  // What the designer's name field did in a frame (ADR-017 decision 8): its text, and whether typing goes on or ended,
  // keeping it or dropping it.
  struct TypedName
  {
    std::string name;
    Neuron::FieldEnd end = Neuron::FieldEnd::Typing;
  };

  // The HUD on a back buffer of one size, in its pixels: the engine's display list (ADR-100), in which layer 0 is the HUD
  // and layer i + 1 the window windows[i], and what the game keeps beside it. It is drawn in layers: the HUD, then each
  // window back to front, each layer's panels, then its lines, then its sprites, then its texts. A click belongs to the
  // front layer under it.
  struct Layout : Neuron::DrawList
  {
    float fontPixels = 0.0f;
    // Where each enabled button is, for clicks: what a region that takes a press names by its payload.
    std::vector<std::pair<Rect, Action>> actions;
    // Where each locked part's card is, which answers the pointer with its preview and takes no click (ADR-017 decision 7):
    // what a region that answers the pointer only names by its payload.
    std::vector<std::pair<Rect, Action>> previews;
    // The minimap's drawing area, the map's square; empty when there is no minimap.
    Rect minimap;
    float mapSizeMeters = 0.0f;
    // Back to front, the order they are drawn in.
    std::vector<Window> windows;
    // The bars over the entities, drawn before every layer, so under every panel and window (ADR-088 decision 5).
    // They take no click and cover nothing, so that a click on one is the world's.
    std::vector<Rect> bars;
    // The plates over the fleets, drawn after the bars and before every layer, so under every panel and window; and where
    // each of the player's own stands, with the fleet a click on it selects (ADR-102 decision 2).
    Neuron::DrawList plates;
    std::vector<std::pair<Rect, std::uint32_t>> plateHits;
    // The name field's text while it has the keyboard and as typing ends, for GameClient to give the designer; none
    // otherwise, when the name follows the designer.
    std::optional<TypedName> typedName;

    [[nodiscard]] Span ActionsOf(std::size_t _layer) const noexcept;
    // The front window under a point, if any.
    [[nodiscard]] const Window* WindowAt(float _xPixels, float _yPixels) const noexcept;

    // The enabled button of the front layer under a point, if any: a button of the HUD under a window takes no click.
    [[nodiscard]] std::optional<Action> ActionAt(float _xPixels, float _yPixels) const noexcept;
    // The fleet of the player's own plate under a point, if any, where no panel or window covers it.
    [[nodiscard]] std::optional<std::uint32_t> PlateAt(float _xPixels, float _yPixels) const noexcept;
    // What the pointer is over in the front layer: its enabled button, as ActionAt finds it, or else a locked part's card,
    // whose pick the designer previews and a click does not make.
    [[nodiscard]] std::optional<Action> HoverAt(float _xPixels, float _yPixels) const noexcept;
    // The point on the map under a point on the minimap, if it is on the minimap.
    [[nodiscard]] std::optional<PlanePosition> MapPointAt(float _xPixels, float _yPixels) const noexcept;
    // Where a point on the map shows on the minimap, in pixels, held to the minimap's edge.
    [[nodiscard]] DirectX::XMFLOAT2 MinimapPixelOf(PlanePosition _point) const noexcept;
    // Where a point falls on the minimap's plane, in pixels, past its edge when it is past the map's, so that a line to it
    // can be cut at the edge rather than bent there.
    [[nodiscard]] DirectX::XMFLOAT2 MinimapPixelAt(PlanePosition _point) const noexcept;
  };

  // The content for _player: its Ore and income from the newest snapshot, its research, a description of _selected, by
  // design name from the snapshot's designs, the buttons the selection offers, and the minimap's marks. _placing is the
  // structure being placed, if any. With a _designer, which GameClient gives while its window is open, the designer;
  // _hovered is the button under the pointer, and a part's previews the design it would make.
  // _ticksPerSecond is the server's rate, which tells how long ago a remembered structure was last seen; with none, its
  // age is not told. _armed is the order the controls have armed for the next left-click, AttackMove, HoldSector or
  // Patrol, whose button is lit (ADR-088 decision 2). A selection that holds a Constructor offers one Build button, which
  // turns the command card's build mode on.
  // Each player's color, from the set _catalog draws it with.
  [[nodiscard]] static std::vector<SideColor> SideColorsOf(const ModelCatalog& _catalog);

  // The minimap's drawing (ADR-100 decision 7) into _square of _layout, in pixels, which becomes its minimap: _content's
  // sectors, marks and fog at _scale, without the camera's view, as the overseer draws its map (ADR-103 decision 7).
  static void DrawMinimap(Layout& _layout, const Rect& _square, const Content& _content, float _scale);

  [[nodiscard]] static Content Describe(const Snapshot& _newest, std::span<const EntityView> _entities, std::span<const EntityId> _selected,
                                        std::optional<StructureKind> _placing = std::nullopt, const Designer* _designer = nullptr,
                                        std::optional<Action> _hovered = std::nullopt, std::uint32_t _ticksPerSecond = 0,
                                        std::optional<ActionKind> _armed = std::nullopt);

  // Build mode's content: a button for each structure the player's Constructors build, each in its cell, enabled while
  // _selected among _entities holds a Constructor and the structure can be built (ADR-088 decision 2).
  [[nodiscard]] static BuildPanel DescribeBuild(const Snapshot& _newest, std::span<const EntityView> _entities,
                                                std::span<const EntityId> _selected);

  // The production window's content for _producer, one of the player's finished producers, or nullptr while it has none
  // (Game §interface.windows.production).
  [[nodiscard]] static ProductionPanel DescribeProduction(const Snapshot& _newest, const EntityView* _producer);

  // The research window's content: the player's Research Lab among _entities, its queue, and the topics from _firstTopic
  // (Game §interface.windows.research).
  [[nodiscard]] static ResearchPanel DescribeResearch(const Snapshot& _newest, std::span<const EntityView> _entities,
                                                      std::size_t _firstTopic = 0);

  // The first topic shown after scrolling a row forward, or back with a negative _step, among _topics: never before the
  // first, nor past where the last row is shown.
  [[nodiscard]] static std::size_t StepTopics(std::size_t _firstTopic, int _step, std::size_t _topics) noexcept;

  // The orders window's content (ADR-080 decision 9): _newest's scheduled orders, and _form's rows for the ships of _selected
  // among _entities, its time of day read on _clock and counted from _now.
  [[nodiscard]] static OrdersPanel DescribeOrders(const Snapshot& _newest, std::span<const EntityView> _entities,
                                                  std::span<const EntityId> _selected, const OrderForm& _form, const PlayerClock& _clock,
                                                  std::chrono::sys_seconds _now);

  // The away window's content (ADR-080 decision 9): _report, which reached the player with the snapshot of _untilTick, its time
  // counted at _ticksPerSecond and its sectors named from _newest.
  [[nodiscard]] static AwayPanel DescribeAway(const Snapshot& _newest, const AwayReport& _report, std::uint64_t _untilTick,
                                              std::uint32_t _ticksPerSecond);

  // What a derelict holds, as the hint under the pointer shows it (ADR-074 decision 10): its Ore, the topic whose time
  // it recovers, if any, and how far its salvage has come; and, for one the player only remembers, how long ago it was
  // last seen, at _ticksPerSecond.
  [[nodiscard]] static std::string DescribeDerelict(const Snapshot& _newest, const EntityView& _derelict, std::uint32_t _ticksPerSecond);

  // A structure the player only remembers, as the hint under the pointer shows it (ADR-081 decision 4): what it
  // is, how long ago it was last seen at _ticksPerSecond, and how far it was built if it was not finished.
  [[nodiscard]] static std::string DescribeMemory(const Snapshot& _newest, const EntityView& _structure, std::uint32_t _ticksPerSecond);

  // The bars over _entity, standing on _footPixels with a back of _back (ADR-088 decision 5): a structure's health while it
  // is hurt, a ship's while it is hurt and the camera is close in, and either's whole or not while Alt is held or a ship's
  // while the pointer is over it, as _rule says (ADR-102 decision 3); and its build while it is unfinished. Nothing when
  // neither shows, or when the player only remembers it, since its hit points and its building are as they were when it was
  // seen (ADR-081 decision 4).
  [[nodiscard]] static std::optional<EntityBar> BarOver(const EntityView& _entity, DirectX::XMFLOAT2 _footPixels,
                                                        const DirectX::XMFLOAT4& _back, BarRule _rule) noexcept;

  // The plate over _fleet, which ScreenFleets found among _entities, for _newest's player (ADR-102 decision 2): its count,
  // the insignia of its commonest kind of ship, ties going to the larger hull and a warship before a Constructor, the share
  // of its hit points left, and its side's color from _sideColors, the pirates' their own.
  [[nodiscard]] static Plate PlateOver(const ScreenFleets::Fleet& _fleet, const Snapshot& _newest, std::span<const EntityView> _entities,
                                       std::span<const SideColor> _sideColors);

  // How the match in _newest ended for its player, the length counted at _ticksPerSecond; nothing while it runs. In a universe,
  // which does not end, when the player's seat restarts after its loss (ADR-084 decision 7).
  [[nodiscard]] static std::optional<Outcome> DescribeOutcome(const Snapshot& _newest, std::uint32_t _ticksPerSecond);

  // A dedicated server's address as the tag in the top-right corner writes it (ADR-086): _host and _port, "203.0.113.5:4433",
  // and an IPv6 host in brackets, "[2001:db8::5]:4433".
  [[nodiscard]] static std::string ServerName(std::string_view _host, std::uint16_t _port);

  // What the main menu offers and says besides its skirmishes (ADR-078): a universe to join, when a join file is there, and a
  // notice, such as why the last game ended.
  struct MenuState
  {
    bool joinUniverse = false;
    std::string notice;
    // While a join connects, the server it connects to, as ServerName writes it (ADR-078 decision 9).
    std::string connecting;
  };

  // The main menu on a back buffer of this size, its text measured with _metrics: the game's name, a notice in the
  // warning's color when _state has one, and buttons to start a skirmish against the AI, to join a universe when _state
  // offers one, and to quit (ADR-020 decision 10). While a join connects, it says so under the subtitle, and only Quit
  // can be pressed (ADR-078 decision 9). _factor is the interface's own scale (ADR-070). The plain menu is an
  // overload rather than a default argument, which could not use MenuState's member initializers before Hud is complete.
  [[nodiscard]] static Layout LayMenu(const TextMetrics& _metrics, std::uint32_t _widthPixels, std::uint32_t _heightPixels,
                                      float _factor = 1.0f)
  {
    return LayMenu(_metrics, _widthPixels, _heightPixels, _factor, MenuState{});
  }
  [[nodiscard]] static Layout LayMenu(const TextMetrics& _metrics, std::uint32_t _widthPixels, std::uint32_t _heightPixels, float _factor,
                                      const MenuState& _state);

  // Where everything goes on a back buffer of this size, its text measured with _metrics, which are the fonts at this
  // size's scale. _view is the ground the camera shows, its corners in order, outlined on the minimap; empty when the
  // camera sees past the horizon. The floating windows (ADR-031) are those _windows has open, in its order and where it
  // left them; without a manager, every window the content has, at its default place. _hovered is the action under the
  // pointer, which the HUD's button and status line that take it are lit for (ADR-088 decision 1). The windows built on
  // the canvas take what is under the pointer from _interaction (ADR-100), or have nothing under it without one.
  [[nodiscard]] static Layout Lay(const Content& _content, const TextMetrics& _metrics, std::uint32_t _widthPixels,
                                  std::uint32_t _heightPixels, std::span<const PlanePosition> _view = {},
                                  const WindowManager* _windows = nullptr, float _factor = 1.0f,
                                  std::optional<Action> _hovered = std::nullopt, Neuron::Interaction* _interaction = nullptr);

  // Where a window _widthUnits wide may stand with its top-left corner at _corner on a screen of this size, in reference
  // units: moved only as far as keeps its title bar on the screen, and WINDOW_KEPT_ON_SCREEN_UNITS of its width.
  [[nodiscard]] static WindowManager::Point KeepOnScreen(WindowManager::Point _corner, float _widthUnits, float _screenWidthUnits,
                                                         float _screenHeightUnits) noexcept;

  // The scale from reference units to pixels: the largest at which the whole reference frame fits (ADR-006), times the
  // interface's own scale, _factor, which the player sets (ADR-070).
  [[nodiscard]] static float Scale(std::uint32_t _widthPixels, std::uint32_t _heightPixels, float _factor = 1.0f) noexcept;

  // The interface's own scale steps, a factor on the screen's (ADR-070): Ctrl+= takes the next, and Ctrl+- the one before.
  static constexpr std::array<float, 6> INTERFACE_STEPS{1.0f, 1.1f, 1.25f, 1.5f, 1.75f, 2.0f};

  // Whether the windows of a fixed size, the designer laid out for _newest's components, the Controls window, and the orders
  // and away windows at their tallest, fit a screen of this size between its margins at _factor (ADR-070). Without a
  // snapshot, as on the menu, the Controls window alone.
  [[nodiscard]] static bool WindowsFit(const Snapshot* _newest, std::uint32_t _widthPixels, std::uint32_t _heightPixels, float _factor);

  // The step _step away from _factor among INTERFACE_STEPS, up for a positive _step and down for a negative one, as far as
  // the windows fit; and with no step, the largest that fits at or below _factor, as a new screen or match needs.
  [[nodiscard]] static float StepInterface(float _factor, int _step, const Snapshot* _newest, std::uint32_t _widthPixels,
                                           std::uint32_t _heightPixels);
};

// _value with a comma between each group of three digits, such as "12,000".
[[nodiscard]] std::string WithThousands(std::int64_t _value);

// A length of time as minutes and seconds, "6:13", with hours in front once there are any, "1:02:03".
[[nodiscard]] std::string MinutesAndSeconds(std::uint64_t _seconds);
} // namespace Outpost