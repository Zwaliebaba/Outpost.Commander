#pragma once

// GameProtocolTests drives what the client and the server share on its own (ADR-089): identifiers, the wire
// format and placement, with neither the client nor the server behind them (ADR-002).

#include "GameProtocol.h"

#include <CppUnitTest.h>
