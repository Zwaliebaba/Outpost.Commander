#pragma once

#if defined(_DEBUG)
#   include <crtdbg.h>
#   define NEW new (_NORMAL_BLOCK, __FILE__, __LINE__)
#else
#   define NEW new
#endif

namespace Neuron
{
template <class... Types> void DebugTrace([[maybe_unused]] const std::string_view _fmt, [[maybe_unused]] Types&&... _args)
{
#ifdef _DEBUG
  const std::string message = vformat(_fmt, std::make_format_args(_args...));
  OutputDebugStringA(message.c_str());
#endif
}

template <class... Types> void DebugTrace([[maybe_unused]] const std::wstring_view _fmt, [[maybe_unused]] Types&&... _args)
{
#ifdef _DEBUG
  const std::wstring message = vformat(_fmt, std::make_wformat_args(_args...));
  OutputDebugStringW(message.c_str());
#endif
}

// Writes _message where a developer sees it, to the debugger's output and to standard error, and ends the process at once,
// in every configuration (ADR-093). Nothing can catch it: a failed assertion says the code is wrong, which no handler can
// mend, and unwinding would run destructors over the state that broke.
[[noreturn]] void Fail(const char* _message) noexcept;

// Fails with the text _fmt formats from _args, in UTF-8.
template <class... Types> [[noreturn]] void Fatal(std::format_string<Types...> _fmt, Types&&... _args) noexcept
{
  try
  {
    Fail(std::format(_fmt, std::forward<Types>(_args)...).c_str());
  }
  catch (...)
  {
    // Out of memory, or a formatter that threw: the process ends all the same.
    Fail("A failure's message could not be formatted.");
  }
}
} // namespace Neuron

// ASSERT and ASSERT_TEXT are checked in every configuration and DEBUG_ASSERT only in Debug. A failure names its file and
// line, then the expression or the text, and ends the process (ADR-093).
#define ASSERT(expression) ((expression) ? (void)0 : Neuron::Fatal("{}({}): {}", __FILE__, __LINE__, #expression))
#define ASSERT_TEXT(expression, ...) ((expression) ? (void)0 : Neuron::Fatal("{}({}): {}", __FILE__, __LINE__, std::format(__VA_ARGS__)))

#ifdef _DEBUG
#   define DEBUG_ASSERT(expression) ASSERT(expression)
#   define DEBUG_ASSERT_TEXT(expression, ...) ASSERT_TEXT(expression, __VA_ARGS__)
#   define DEBUG_WARNING(expression, ...) (void)((!(expression)) || (Neuron::DebugTrace(__VA_ARGS__), 0))

#else
#   define DEBUG_ASSERT(expression) (__noop(expression))
#   define DEBUG_ASSERT_TEXT(expression, ...) (__noop(expression, __VA_ARGS__))
#   define DEBUG_WARNING(expression, ...) (__noop(expression, __VA_ARGS__))

#endif
