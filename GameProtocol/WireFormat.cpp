#include "pch.h"
#include "WireFormat.h"

#include <algorithm>
#include <bit>
#include <concepts>
#include <iterator>
#include <utility>

#include "ByteFormat.h"
#include "WireFields.h"

namespace Outpost
{
namespace
{
// 64-bit FNV-1a, as a save's layout is hashed (ADR-077).
constexpr std::uint64_t FNV_OFFSET = 0xCBF29CE484222325ull;
constexpr std::uint64_t FNV_PRIME = 0x100000001B3ull;

// Where the hello's version and layout hash stand: after the byte that says which message it is.
constexpr std::size_t HELLO_VERSION_START = 1;
constexpr std::size_t HELLO_LAYOUT_START = HELLO_VERSION_START + sizeof(std::uint32_t);

// Where T is among Message's alternatives, which is the first byte of every message.
template <typename T, typename... Ts> consteval std::uint8_t KindOf(std::type_identity<std::variant<Ts...>>)
{
  constexpr std::array<bool, sizeof...(Ts)> MATCHES{std::same_as<T, Ts>...};
  return static_cast<std::uint8_t>(std::ranges::find(MATCHES, true) - MATCHES.begin());
}

template <typename T> std::vector<std::byte> Encode(const T& _message)
{
  ByteWriter writer;
  writer.Put(KindOf<T>(std::type_identity<Message>{}));
  writer.Put(_message);
  return writer.Take();
}

// PutCatalog for a catalogue to copy from or one to move from.
template <typename Catalog> void Put(Catalog&& _catalog, Snapshot& _snapshot)
{
  constexpr bool MOVES = std::is_rvalue_reference_v<Catalog&&>;
  // A part of the catalogue, to move from when the caller gives the catalogue up, and to copy from when it keeps it.
  const auto part = [](auto& _part) -> decltype(auto)
  {
    if constexpr (std::is_rvalue_reference_v<Catalog&&>)
      return std::move(_part);
    else
      return std::as_const(_part);
  };
  _snapshot.designs = part(_catalog.designs);
  _snapshot.structureTypes = part(_catalog.structureTypes);
  _snapshot.hulls = part(_catalog.hulls);
  _snapshot.drives = part(_catalog.drives);
  _snapshot.weapons = part(_catalog.weapons);
  _snapshot.modules = part(_catalog.modules);
  _snapshot.research = part(_catalog.research);
  // The two list the map's sectors in the map's order, so a sector's place finds it.
  for (std::size_t i = 0; i < _snapshot.sectors.size() && i < _catalog.sectors.size(); ++i)
  {
    SectorView& sector = _snapshot.sectors[i];
    auto& shape = _catalog.sectors[i];
    if (shape.id != sector.id)
      continue;
    sector.nameUtf8 = part(shape.nameUtf8);
    sector.minXMeters = shape.minXMeters;
    sector.maxXMeters = shape.maxXMeters;
    sector.minZMeters = shape.minZMeters;
    sector.maxZMeters = shape.maxZMeters;
    sector.node = shape.node;
    sector.adjacent = part(shape.adjacent);
  }
  const auto middle = static_cast<std::ptrdiff_t>(_snapshot.entities.size());
  if constexpr (MOVES)
    std::ranges::move(_catalog.fields, std::back_inserter(_snapshot.entities));
  else
    std::ranges::copy(_catalog.fields, std::back_inserter(_snapshot.entities));
  std::ranges::inplace_merge(_snapshot.entities, _snapshot.entities.begin() + middle, {}, &EntityView::id);
}

template <typename T>
concept Vector = std::same_as<T, std::vector<typename T::value_type>>;

template <typename T>
concept Optional = std::same_as<T, std::optional<typename T::value_type>>;

// A record with an identifier, which a list of them, in increasing order of it, changes by (ChangeOf).
template <typename T>
concept Keyed = Record<T> && requires(const T& _value) { _value.id; } && std::totally_ordered<decltype(T::id)>;

template <typename T>
concept KeyedList = Vector<T> && Keyed<typename T::value_type>;

template <typename T> constexpr bool IS_ID = false;
template <typename Tag> constexpr bool IS_ID<Id<Tag>> = true;

// Whether _a and _b are the same bit for bit: a float by its bits, so that a change keeps a zero's sign, and a record, a list
// or an optional value by what it holds. A kind of value this does not name does not compile, rather than be compared some
// other way.
template <typename T> [[nodiscard]] bool Same(const T& _a, const T& _b)
{
  if constexpr (std::same_as<T, float>)
    return std::bit_cast<std::uint32_t>(_a) == std::bit_cast<std::uint32_t>(_b);
  else if constexpr (std::same_as<T, double>)
    return std::bit_cast<std::uint64_t>(_a) == std::bit_cast<std::uint64_t>(_b);
  else if constexpr (Record<T>)
  {
    const auto a = Fields(_a);
    const auto b = Fields(_b);
    return [&a, &b]<std::size_t... Indices>(std::index_sequence<Indices...>)
    { return (Same(std::get<Indices>(a), std::get<Indices>(b)) && ...); }(std::make_index_sequence<std::tuple_size_v<decltype(a)>>{});
  }
  else if constexpr (Vector<T>)
    return std::ranges::equal(_a, _b, [](const auto& _x, const auto& _y) { return Same(_x, _y); });
  else if constexpr (Optional<T>)
    return _a.has_value() == _b.has_value() && (!_a.has_value() || Same(*_a, *_b));
  else
  {
    static_assert(std::integral<T> || std::is_enum_v<T> || IS_ID<T> || std::same_as<T, std::string>, "Same does not know this value");
    return _a == _b;
  }
}

// Whether _list's identifiers increase, each past the one before it.
template <Keyed T> [[nodiscard]] bool Increasing(std::span<const T> _list)
{
  return std::ranges::adjacent_find(_list, std::ranges::greater_equal{}, &T::id) == _list.end();
}

// A record's fields that a change carries, a bit each in the order Fields lists them.
template <std::size_t Count> using FieldBits = std::array<std::uint8_t, (Count + 7) / 8>;

// Writes what changed from one state to the next (ChangeOf).
class ChangeWriter
{
public:
  explicit ChangeWriter(ByteWriter& _writer) noexcept
    : m_writer(_writer)
  {
  }

  // A bit for each of the record's fields, set where it differs, and then each that differs.
  template <Record T> void PutRecord(const T& _base, const T& _next)
  {
    const auto base = Fields(_base);
    const auto next = Fields(_next);
    constexpr std::size_t COUNT = std::tuple_size_v<decltype(next)>;
    std::array<bool, COUNT> differs{};
    [&]<std::size_t... Indices>(std::index_sequence<Indices...>)
    { ((differs[Indices] = !Same(std::get<Indices>(base), std::get<Indices>(next))), ...); }(std::make_index_sequence<COUNT>{});
    FieldBits<COUNT> bits{};
    for (std::size_t i = 0; i < COUNT; ++i)
    {
      if (differs[i])
        bits[i / 8] = static_cast<std::uint8_t>(bits[i / 8] | (1u << (i % 8)));
    }
    m_writer.Put(bits);
    [&]<std::size_t... Indices>(std::index_sequence<Indices...>)
    { ((differs[Indices] ? PutField(std::get<Indices>(base), std::get<Indices>(next)) : void()), ...); }(std::make_index_sequence<COUNT>{});
  }

private:
  template <typename T> void PutField(const T& _base, const T& _next)
  {
    if constexpr (KeyedList<T>)
      PutList(std::span(_base), std::span(_next));
    else
      m_writer.Put(_next);
  }

  // Whether it changes by its elements, which it does where both lists are in order; then the identifiers of the elements
  // gone, each element changed with its identifier and its change, and each element new, all in order of identifier.
  template <Keyed T> void PutList(std::span<const T> _base, std::span<const T> _next)
  {
    const bool byElement = Increasing(_base) && Increasing(_next);
    m_writer.Put(byElement);
    if (!byElement)
    {
      m_writer.Put(std::vector<T>(_next.begin(), _next.end()));
      return;
    }
    std::vector<std::remove_cvref_t<decltype(T::id)>> gone;
    std::vector<std::pair<const T*, const T*>> changed;
    std::vector<const T*> added;
    auto base = _base.begin();
    for (const T& next : _next)
    {
      for (; base != _base.end() && base->id < next.id; ++base)
        gone.push_back(base->id);
      if (base != _base.end() && base->id == next.id)
      {
        if (!Same(*base, next))
          changed.emplace_back(&*base, &next);
        ++base;
      }
      else
        added.push_back(&next);
    }
    for (; base != _base.end(); ++base)
      gone.push_back(base->id);
    m_writer.Put(gone);
    m_writer.Put(static_cast<std::uint32_t>(changed.size()));
    for (const auto& [from, to] : changed)
    {
      m_writer.Put(to->id);
      PutRecord(*from, *to);
    }
    m_writer.Put(static_cast<std::uint32_t>(added.size()));
    for (const T* element : added)
      m_writer.Put(*element);
  }

  ByteWriter& m_writer;
};

// Reads what ChangeWriter wrote into the state it was written from, and refuses what it could not have written from it.
class ChangeReader
{
public:
  explicit ChangeReader(ByteReader& _reader) noexcept
    : m_reader(_reader)
  {
  }

  template <Record T> void GetRecord(T& _value)
  {
    const auto fields = Fields(_value);
    constexpr std::size_t COUNT = std::tuple_size_v<decltype(fields)>;
    FieldBits<COUNT> bits{};
    m_reader.Get(bits);
    if constexpr (COUNT % 8 != 0)
    {
      if ((bits.back() >> (COUNT % 8)) != 0)
        m_reader.Malformed("it changes a field its record does not have");
    }
    const auto differs = [&bits](std::size_t _field) { return ((bits[_field / 8] >> (_field % 8)) & 1u) != 0; };
    [&]<std::size_t... Indices>(std::index_sequence<Indices...>)
    { ((differs(Indices) ? GetField(std::get<Indices>(fields)) : void()), ...); }(std::make_index_sequence<COUNT>{});
  }

private:
  template <typename T> void GetField(T& _value)
  {
    if constexpr (KeyedList<T>)
      GetList(_value);
    else
      m_reader.Get(_value);
  }

  template <Keyed T> void GetList(std::vector<T>& _list)
  {
    bool byElement = false;
    m_reader.Get(byElement);
    if (!byElement)
    {
      m_reader.Get(_list);
      return;
    }
    if (!Increasing(std::span<const T>(_list)))
      m_reader.Malformed("it changes by its elements a list that is out of order");
    using Key = std::remove_cvref_t<decltype(T::id)>;
    // The element of _list with identifier _id, which the change says _list holds.
    const auto find = [this, &_list](const Key& _id)
    {
      auto found = std::ranges::lower_bound(_list, _id, {}, &T::id);
      if (found == _list.end() || found->id != _id)
        m_reader.Malformed("it changes an element the state does not hold");
      return found;
    };
    std::vector<Key> gone;
    m_reader.Get(gone);
    for (const Key& id : gone)
      _list.erase(find(id));
    std::uint32_t changed = 0;
    m_reader.Get(changed);
    for (std::uint32_t i = 0; i < changed; ++i)
    {
      Key id{};
      m_reader.Get(id);
      GetRecord(*find(id));
    }
    std::uint32_t added = 0;
    m_reader.Get(added);
    // Each element takes a byte at least, so a count the bytes cannot back runs out of them before memory runs out.
    if (added > m_reader.Left())
      m_reader.Malformed(std::format("it adds {} elements with {} bytes left", added, m_reader.Left()));
    const std::size_t kept = _list.size();
    for (std::uint32_t i = 0; i < added; ++i)
      m_reader.Get(_list.emplace_back());
    const auto middle = _list.begin() + static_cast<std::ptrdiff_t>(kept);
    if (!Increasing(std::span<const T>(_list.begin(), middle)) || !Increasing(std::span<const T>(middle, _list.end())))
      m_reader.Malformed("it leaves a list out of order");
    std::ranges::inplace_merge(_list, middle, {}, &T::id);
    if (!Increasing(std::span<const T>(_list)))
      m_reader.Malformed("it adds an element the state holds already");
  }

  ByteReader& m_reader;
};
} // namespace

std::string WireLayout()
{
  std::string layout;
  ByteLayout::Describe<Message>(layout);
  return layout;
}

std::uint64_t WireLayoutHash()
{
  static const std::uint64_t LAYOUT_HASH = []
  {
    std::uint64_t hash = FNV_OFFSET;
    for (const char character : WireLayout())
      hash = (hash ^ static_cast<std::uint8_t>(character)) * FNV_PRIME;
    return hash;
  }();
  return LAYOUT_HASH;
}

std::vector<std::byte> EncodeMessage(const HelloMessage& _hello)
{
  return Encode(_hello);
}

std::vector<std::byte> EncodeMessage(const WelcomeMessage& _welcome)
{
  return Encode(_welcome);
}

std::vector<std::byte> EncodeMessage(const Command& _command)
{
  return Encode(_command);
}

std::vector<std::byte> EncodeMessage(const StateMessage& _state)
{
  return Encode(_state);
}

std::vector<std::byte> EncodeMessage(const EventsMessage& _events)
{
  return Encode(_events);
}

std::vector<std::byte> EncodeMessage(const StateChangeMessage& _change)
{
  return Encode(_change);
}

std::vector<std::byte> EncodeMessage(const HeldMessage& _held)
{
  return Encode(_held);
}

std::vector<std::byte> ChangeOf(const Snapshot& _base, const Snapshot& _next)
{
  ByteWriter writer;
  ChangeWriter(writer).PutRecord(_base, _next);
  return writer.Take();
}

void PutChange(std::span<const std::byte> _change, Snapshot& _state)
{
  ByteReader reader(_change, "A state's change");
  ChangeReader(reader).GetRecord(_state);
  reader.Finish();
}

EventsMessage TakeEvents(Snapshot& _snapshot)
{
  return {.tick = _snapshot.tick,
          .shots = std::exchange(_snapshot.shots, {}),
          .destroyed = std::exchange(_snapshot.destroyed, {}),
          .events = std::exchange(_snapshot.events, {}),
          .away = std::exchange(_snapshot.away, std::nullopt)};
}

bool HasAny(const EventsMessage& _events) noexcept
{
  return !_events.shots.empty() || !_events.destroyed.empty() || !_events.events.empty() || _events.away.has_value();
}

void PutEvents(EventsMessage&& _events, Snapshot& _snapshot)
{
  std::ranges::move(_events.shots, std::back_inserter(_snapshot.shots));
  std::ranges::move(_events.destroyed, std::back_inserter(_snapshot.destroyed));
  std::ranges::move(_events.events, std::back_inserter(_snapshot.events));
  // A report of a time away is in the first snapshot after the seat is taken again, so one never meets another.
  if (_events.away.has_value())
    _snapshot.away = std::move(_events.away);
}

std::vector<std::byte> EncodeMessage(const CatalogMessage& _catalog)
{
  return Encode(_catalog);
}

CatalogMessage TakeCatalog(Snapshot& _snapshot)
{
  CatalogMessage catalog{.designs = std::exchange(_snapshot.designs, {}),
                         .structureTypes = std::exchange(_snapshot.structureTypes, {}),
                         .hulls = std::exchange(_snapshot.hulls, {}),
                         .drives = std::exchange(_snapshot.drives, {}),
                         .weapons = std::exchange(_snapshot.weapons, {}),
                         .modules = std::exchange(_snapshot.modules, {}),
                         .research = std::exchange(_snapshot.research, {})};
  // What a sector is, out of it; who holds it, and the rest that changes in play, stays.
  catalog.sectors.reserve(_snapshot.sectors.size());
  for (SectorView& sector : _snapshot.sectors)
  {
    catalog.sectors.push_back({.id = sector.id,
                               .nameUtf8 = std::exchange(sector.nameUtf8, {}),
                               .minXMeters = std::exchange(sector.minXMeters, 0.0f),
                               .maxXMeters = std::exchange(sector.maxXMeters, 0.0f),
                               .minZMeters = std::exchange(sector.minZMeters, 0.0f),
                               .maxZMeters = std::exchange(sector.maxZMeters, 0.0f),
                               .node = std::exchange(sector.node, {}),
                               .adjacent = std::exchange(sector.adjacent, {})});
  }
  const auto fields =
    std::ranges::stable_partition(_snapshot.entities, [](const EntityView& _entity) { return _entity.kind != EntityKind::AsteroidField; });
  catalog.fields.assign(std::make_move_iterator(fields.begin()), std::make_move_iterator(fields.end()));
  _snapshot.entities.erase(fields.begin(), fields.end());
  return catalog;
}

void PutCatalog(const CatalogMessage& _catalog, Snapshot& _snapshot)
{
  Put(_catalog, _snapshot);
}

void PutCatalog(CatalogMessage&& _catalog, Snapshot& _snapshot)
{
  Put(std::move(_catalog), _snapshot);
}

std::optional<std::string_view> DescribeClose(std::uint64_t _errorCode) noexcept
{
  switch (_errorCode)
  {
  case static_cast<std::uint64_t>(CloseReason::Finished):
    return "The server ended the game.";
  case static_cast<std::uint64_t>(CloseReason::WrongVersion):
    return "The server runs another build of the game.";
  case static_cast<std::uint64_t>(CloseReason::SeatRefused):
    return "The server refused the seat: it is not open there, or the join file's token is not the seat's.";
  case static_cast<std::uint64_t>(CloseReason::MalformedMessage):
    return "The server could not read what the game sent it.";
  case static_cast<std::uint64_t>(CloseReason::SeatTaken):
    return "The seat was taken by another connection with its token.";
  case static_cast<std::uint64_t>(CloseReason::DroppedByOwner):
    return "The server's owner closed the connection. Join again from the menu.";
  default:
    return std::nullopt;
  }
}

std::optional<std::uint32_t> PeekHelloVersion(std::span<const std::byte> _bytes) noexcept
{
  if (_bytes.size() < HELLO_LAYOUT_START || static_cast<std::uint8_t>(_bytes[0]) != KindOf<HelloMessage>(std::type_identity<Message>{}))
    return std::nullopt;
  std::uint32_t version = 0;
  for (std::size_t i = 0; i < sizeof(std::uint32_t); ++i)
    version |= static_cast<std::uint32_t>(_bytes[HELLO_VERSION_START + i]) << (8 * i);
  return version;
}

std::optional<std::uint64_t> PeekHelloLayout(std::span<const std::byte> _bytes) noexcept
{
  constexpr std::size_t LAYOUT_END = HELLO_LAYOUT_START + sizeof(std::uint64_t);
  if (_bytes.size() < LAYOUT_END || static_cast<std::uint8_t>(_bytes[0]) != KindOf<HelloMessage>(std::type_identity<Message>{}))
    return std::nullopt;
  std::uint64_t hash = 0;
  for (std::size_t i = 0; i < sizeof(std::uint64_t); ++i)
    hash |= static_cast<std::uint64_t>(_bytes[HELLO_LAYOUT_START + i]) << (8 * i);
  return hash;
}

Message DecodeMessage(std::span<const std::byte> _bytes)
{
  ByteReader reader(_bytes, "A message from the network");
  Message message;
  reader.Get(message);
  reader.Finish();
  return message;
}
} // namespace Outpost
