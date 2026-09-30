#pragma once

// The server engine: what only the server needs, such as the tick host and transports.
// It builds on NeuronCore and knows no game concept (R9). No renderer, XAML or WinRT API (ADR-002).

#include "NeuronCore.h"
