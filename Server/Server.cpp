#include "pch.h"

#include <cstdio>
#include <string>

// The host, and the whole of it: a console executable that names what it was linked against and
// exits. The match loop arrives with the simulation.

namespace
{
void PrintLibraryName(std::string_view _name)
{
  std::string line{_name};
  line.push_back('\n');
  std::fputs(line.c_str(), stdout);
}
} // namespace

int main()
{
  PrintLibraryName(Neuron::CoreLibraryName());
  PrintLibraryName(Neuron::ServerLibraryName());
  PrintLibraryName(Outpost::CoreLibraryName());
  PrintLibraryName(Outpost::LogicLibraryName());
  return 0;
}