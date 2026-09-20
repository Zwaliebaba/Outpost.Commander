#pragma once

#if defined(_MSC_VER)
#   include <intrin.h>
#endif

namespace Neuron
{

/// Writes a failed assertion to the debugger output and to stderr, then returns; the macro that
/// called it breaks into the debugger next.
void ReportAssertionFailure(const char* _expression, const char* _file, int _line) noexcept;

} // namespace Neuron

// OUTPOST_ASSERT checks in Debug and compiles to nothing in Release; OUTPOST_VERIFY checks in
// Debug and still evaluates its expression in Release, for a call whose side effect is needed.
#if defined(_DEBUG)
#   define OUTPOST_ASSERT(expression)                                       \
     do                                                                     \
     {                                                                      \
       if (!(expression))                                                   \
       {                                                                    \
         ::Neuron::ReportAssertionFailure(#expression, __FILE__, __LINE__); \
         __debugbreak();                                                    \
       }                                                                    \
     } while (false)
#   define OUTPOST_VERIFY(expression) OUTPOST_ASSERT(expression)
#else
#   define OUTPOST_ASSERT(expression) ((void)0)
#   define OUTPOST_VERIFY(expression) ((void)(expression))
#endif
