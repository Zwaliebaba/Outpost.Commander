#pragma once

#include <cstdint>

namespace Outpost
{

/// `GameDesign.md` section 6's size class, which is an axis of section 7's damage table rather than
/// a dimension. It says what a `MassDriver` does to a small hull; it says nothing about how long a
/// `Scout` is. **That number is Q37 and is not in this file.**
///
/// === THE ENUMERATORS CANNOT BE SPELLED `Small` OR `Large`. ======================================
///
/// `<windows.h>` defines `small` as a macro -- it is a typedef helper left over from RPC headers --
/// and the preprocessor rewrites the identifier before the compiler ever sees it. The failure is
/// not a nice one: the error lands on whatever the macro expanded to, several lines from anything
/// the reader wrote. `TechnicalDesign.md` section 1 names this trap and this is the file that meets
/// it first.
///
/// **`Light`, `Medium`, `Heavy`, and here is which column of section 7's table each one is:**
///
/// | This enumerator | `GameDesign.md` section 6 and 7 call it |
/// |---|---|
/// | `Light`  | Small  |
/// | `Medium` | Medium |
/// | `Heavy`  | Large  |
///
/// The design's prose and this code will not match, and a reader who does not know why would
/// reasonably wonder whether it is a bug. It is not: it is a macro.
enum class SizeClass : std::uint8_t
{
  Light,
  Medium,
  Heavy
};

} // namespace Outpost
