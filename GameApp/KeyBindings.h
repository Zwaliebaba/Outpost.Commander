#pragma once

namespace Outpost
{
// The keys the game reads, in one place, so that what a button's cap or the Controls window says cannot disagree with what
// the input code does (tasks 15.2 and 16.4). Each is a Windows virtual-key code; a letter's is its capital.

// The orders (design §9): attack-move and stop, and the standing orders, hold a sector and patrol (ADR-059). Control
// groups are the digits, assigned with Ctrl.
inline constexpr std::uint8_t KEY_ATTACK_MOVE = 'A';
inline constexpr std::uint8_t KEY_STOP = 'S';
inline constexpr std::uint8_t KEY_HOLD_SECTOR = 'H';
inline constexpr std::uint8_t KEY_PATROL = 'T';
inline constexpr std::uint8_t KEY_FIRST_GROUP = '0';
inline constexpr std::uint8_t KEY_LAST_GROUP = '9';

// The camera (ADR-012): the arrows pan, and Q and E turn it, Q counterclockwise seen from above.
inline constexpr std::uint8_t KEY_PAN_UP = VK_UP;
inline constexpr std::uint8_t KEY_PAN_LEFT = VK_LEFT;
inline constexpr std::uint8_t KEY_PAN_DOWN = VK_DOWN;
inline constexpr std::uint8_t KEY_PAN_RIGHT = VK_RIGHT;
inline constexpr std::uint8_t KEY_TURN_COUNTERCLOCKWISE = 'Q';
inline constexpr std::uint8_t KEY_TURN_CLOCKWISE = 'E';

// The keys that open and close the designer, the production window and the research window (Phase 1 design §12), clear of
// the orders' A and S, the camera's Q and E and arrows, and the control groups' digits; and the Controls window (task 16.4).
inline constexpr std::uint8_t KEY_DESIGNER = 'D';
inline constexpr std::uint8_t KEY_PRODUCTION = 'P';
inline constexpr std::uint8_t KEY_RESEARCH = 'R';
inline constexpr std::uint8_t KEY_CONTROLS = VK_F1;
// Closes the front window, and with none open cancels what the controls have armed.
inline constexpr std::uint8_t KEY_CANCEL = VK_ESCAPE;
// Moves the camera to the newest alert (ADR-059).
inline constexpr std::uint8_t KEY_LATEST_ALERT = VK_SPACE;
// Held, shows a bar over every ship and structure, whole or not (ADR-047).
inline constexpr std::uint8_t KEY_EVERY_HEALTH_BAR = VK_MENU;

// A letter key as a button's cap shows it: the letter.
[[nodiscard]] inline std::string KeyCap(std::uint8_t _key)
{
  return std::string(1, static_cast<char>(_key));
}

// A key's name as the Controls window writes it: a letter or a digit, or the name on the key.
[[nodiscard]] inline std::string KeyName(std::uint8_t _key)
{
  switch (_key)
  {
  case VK_UP:
    return "Up";
  case VK_LEFT:
    return "Left";
  case VK_DOWN:
    return "Down";
  case VK_RIGHT:
    return "Right";
  case VK_F1:
    return "F1";
  case VK_ESCAPE:
    return "Esc";
  case VK_SPACE:
    return "Space";
  case VK_MENU:
    return "Alt";
  default:
    return KeyCap(_key);
  }
}

// A line of the Controls window (task 16.4): the keys or the mouse action, and what it does; or, with nothing it does, the
// heading of the lines under it.
struct KeyBinding
{
  std::string keys;
  std::string does;
};

// Every key and mouse action the game reads in a match, as the Controls window lists them, under three headings.
[[nodiscard]] inline std::vector<KeyBinding> KeyBindings()
{
  const auto then = [](std::uint8_t _key) { return std::format("{}, then click", KeyName(_key)); };
  return {
    {.keys = "SELECTING AND ORDERS", .does = ""},
    {.keys = "Click", .does = "Select a ship, or a structure of yours"},
    {.keys = "Drag", .does = "Select your ships in the box"},
    {.keys = "Shift+click", .does = "Add to the selection"},
    {.keys = "Double-click", .does = "Select every ship of that design in view"},
    {.keys = "Right-click", .does = "Move, attack, or build and repair with Constructors"},
    {.keys = then(KEY_ATTACK_MOVE), .does = "Attack-move"},
    {.keys = then(KEY_HOLD_SECTOR), .does = "Hold the sector clicked in"},
    {.keys = then(KEY_PATROL), .does = "Patrol to the point clicked"},
    {.keys = KeyName(KEY_STOP), .does = "Stop"},
    {.keys = std::format("Ctrl+{}-{}", KeyName(KEY_FIRST_GROUP), KeyName(KEY_LAST_GROUP)), .does = "Make a control group"},
    {.keys = std::format("{}-{}", KeyName(KEY_FIRST_GROUP), KeyName(KEY_LAST_GROUP)), .does = "Select a group; twice, look at it"},
    {.keys = "Shift+click", .does = "Place a structure and keep placing"},
    {.keys = "CAMERA", .does = ""},
    {.keys = std::format("{} {} {} {}", KeyName(KEY_PAN_UP), KeyName(KEY_PAN_DOWN), KeyName(KEY_PAN_LEFT), KeyName(KEY_PAN_RIGHT)),
     .does = "Pan"},
    {.keys = "Screen edge", .does = "Pan, while the pointer is held in the window"},
    {.keys = "Middle-drag", .does = "Pan"},
    {.keys = "Wheel", .does = "Zoom"},
    {.keys = std::format("{} and {}", KeyName(KEY_TURN_COUNTERCLOCKWISE), KeyName(KEY_TURN_CLOCKWISE)), .does = "Turn"},
    {.keys = "Minimap click", .does = "Look there"},
    {.keys = "Minimap right-click", .does = "Send the selection there"},
    {.keys = KeyName(KEY_LATEST_ALERT), .does = "Look at the newest alert"},
    {.keys = "WINDOWS", .does = ""},
    {.keys = KeyName(KEY_DESIGNER), .does = "Ship designer"},
    {.keys = KeyName(KEY_PRODUCTION), .does = "Production"},
    {.keys = KeyName(KEY_RESEARCH), .does = "Research"},
    {.keys = KeyName(KEY_CONTROLS), .does = "These controls"},
    {.keys = KeyName(KEY_CANCEL), .does = "Close the front window, or cancel an order"},
    {.keys = std::format("{}, held", KeyName(KEY_EVERY_HEALTH_BAR)), .does = "Every health bar"},
  };
}
} // namespace Outpost
