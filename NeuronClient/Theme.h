#pragma once

namespace Neuron
{
// The look the canvas's widgets are drawn in (ADR-100): the Style Guide's roles as one struct, which the game fills from
// its own colors, fonts and sizes. Colors are linear; fonts and sprites are the atlas's indices; sizes are in reference
// units.
struct Theme
{
  // A window's body and a panel's, and a chip's words on its tag.
  DirectX::XMFLOAT4 windowColor{};
  // A window's hatched title bar, and its close box.
  DirectX::XMFLOAT4 titleColor{};
  DirectX::XMFLOAT4 closeBoxColor{};
  // The bracket at each corner of a window or a panel.
  DirectX::XMFLOAT4 cornerColor{};
  // A button's face, a picked one's, and a field's, which a disabled button and a key cap take too.
  DirectX::XMFLOAT4 cardColor{};
  DirectX::XMFLOAT4 pickedColor{};
  DirectX::XMFLOAT4 fieldColor{};
  // A small button's face while it is disabled.
  DirectX::XMFLOAT4 disabledColor{};
  // A card's edge: as it is, under the pointer, and picked.
  DirectX::XMFLOAT4 edgeColor{};
  DirectX::XMFLOAT4 hoverEdgeColor{};
  DirectX::XMFLOAT4 pickedEdgeColor{};
  // The text; a disabled small button's mark; a label, and a disabled button's reason; a key cap's edge and letter; a
  // disabled button's label and figure; and a button's figure.
  DirectX::XMFLOAT4 textColor{};
  DirectX::XMFLOAT4 dimTextColor{};
  DirectX::XMFLOAT4 labelColor{};
  DirectX::XMFLOAT4 keyColor{};
  DirectX::XMFLOAT4 lockedTextColor{};
  DirectX::XMFLOAT4 figureColor{};
  // A chip's tag, which nothing else fills (ADR-085 decision 1), and a text that is refused, plain in a field of its own.
  DirectX::XMFLOAT4 warningColor{};
  // A progress bar's track and its fill.
  DirectX::XMFLOAT4 barTrackColor{};
  DirectX::XMFLOAT4 barFillColor{};

  // A window's title and its close mark; a button's label and a field's text; a key's letter, a small button's mark and a
  // reason; and a figure.
  std::size_t titleFont = 0;
  std::size_t nameFont = 0;
  std::size_t labelFont = 0;
  std::size_t figureFont = 0;
  // The bracket at a window's top-left corner, mirrored for the others; and a checkbox's tick.
  std::size_t cornerSprite = 0;
  std::size_t checkSprite = 0;

  float titleBarUnits = 0.0f;
  // The title's left from the window's edge, the title's and the close mark's top in the title bar, and the close mark's
  // left in its box, which the close mark, in UTF-8, stands in.
  float titleLeftUnits = 0.0f;
  float titleTopUnits = 0.0f;
  float closeMarkLeftUnits = 0.0f;
  std::string closeMark;
  float cornerUnits = 0.0f;
  // How thick an edge is.
  float edgeUnits = 0.0f;
  // A button's label, figure and cap from its sides; the gap between a label cut short and what follows it; and the lines
  // a button sets its label and its figure on, in its middle.
  float buttonInsetUnits = 0.0f;
  float fitGapUnits = 0.0f;
  float nameLineUnits = 0.0f;
  float figureLineUnits = 0.0f;
  // The mark before a figure, as Ore's gem: its size, its top on the figure's line, and the gap after it.
  float figureMarkUnits = 0.0f;
  float figureMarkTopUnits = 0.0f;
  float figureMarkGapUnits = 0.0f;
  // A key cap's side and its letter's top; a button's progress bar, how thick and how far in from its foot and sides.
  float keyCapUnits = 0.0f;
  float keyCapTextTopUnits = 0.0f;
  float barUnits = 0.0f;
  float barInsetUnits = 0.0f;
  // A small button's side, its mark's top, and the gap between a stepper's arrows and its subject.
  float smallButtonUnits = 0.0f;
  float smallButtonTextTopUnits = 0.0f;
  float arrowGapUnits = 0.0f;
  // How far a chip's tag reaches beyond its words on either side.
  float chipPadUnits = 0.0f;
  // A checkbox's side, its tick's inset, and the gap before its label.
  float checkUnits = 0.0f;
  float checkInsetUnits = 0.0f;
  float checkGapUnits = 0.0f;
};
} // namespace Neuron
