#include "pch.h"
#include "JoinTicket.h"

#include <limits>

namespace
{
// Reads _reader's member _name, a string of hexadecimal, into _bytes.
void ReadHex(Neuron::JsonObjectReader& _reader, std::string_view _name, std::span<std::uint8_t> _bytes)
{
  const std::string text = _reader.String(_name);
  if (!Outpost::FromHex(text, _bytes))
    Neuron::JsonFail(_reader.PathOf(_name), std::format("expected {} hexadecimal digits", 2 * _bytes.size()));
}
} // namespace

std::string Outpost::WriteJoinTicket(const JoinTicket& _ticket)
{
  return std::format("{{\n  \"host\": {},\n  \"port\": {},\n  \"certificate\": \"{}\",\n  \"player\": {},\n  \"token\": \"{}\"\n}}\n",
                     Neuron::QuoteJson(_ticket.address.host), _ticket.address.port, ToHex(_ticket.address.certificate),
                     _ticket.player.value, ToHex(_ticket.address.token));
}

Outpost::JoinTicket Outpost::ReadJoinTicket(std::string_view _text)
{
  try
  {
    const Neuron::JsonValue document = Neuron::ParseJson(_text);
    Neuron::JsonObjectReader reader(document, "");
    JoinTicket ticket;
    ticket.address.host = reader.String("host");
    if (ticket.address.host.empty())
      Neuron::JsonFail(reader.PathOf("host"), "expected the server's host name or address");
    const std::int32_t port = reader.Integer("port", 1);
    if (port > std::numeric_limits<std::uint16_t>::max())
      Neuron::JsonFail(reader.PathOf("port"), "expected a port no higher than 65535");
    ticket.address.port = static_cast<std::uint16_t>(port);
    ReadHex(reader, "certificate", ticket.address.certificate);
    ticket.player = reader.Identifier<PlayerId>("player");
    ReadHex(reader, "token", ticket.address.token);
    reader.Finish();
    return ticket;
  }
  catch (const Neuron::Exception& exception)
  {
    throw Neuron::Exception(std::format("The join file is not one: {}", exception.what()));
  }
}
