The review's harness: the unmodified simulation driven from Linux.

GameCore and GameLogic have no Windows dependency beyond the <windows.h> include in NeuronCore.h, so they
compile under g++ 13 with -std=c++23 against an empty stub windows.h. The GameLogic sources include "pch.h",
which is resolved relative to the source file first, so they are compiled from a directory that holds a
stub pch.h (stub-pch.h.txt) beside unmodified copies of the GameLogic .cpp files. Nothing in the
repository is modified by any of this; the sources here are the drivers, saved with a .txt suffix so that
neither the design checker nor the clang-format gate reads them.

Recipe, from the repository root, with the driver as sim.cpp in a scratch directory S:

  mkdir -p S/stub S/logic
  cp Design/Reviews/2026-09-23-mid-implementation-review/harness/stub-windows.h.txt S/stub/windows.h
  cp Design/Reviews/2026-09-23-mid-implementation-review/harness/stub-pch.h.txt     S/logic/pch.h
  for f in World Tick StateHash UniformGrid UnloadTarget MiningSystem Economy ModuleEffects BuildSystem CommandIntake RingAssignment Accumulator; do cp GameLogic/$f.cpp S/logic/; done
  g++ -std=c++23 -O2 -w -I S/stub -I NeuronCore -I GameCore -I GameLogic S/sim.cpp \
      NeuronCore/{FixedPoint,Pcg32,SineTable,ByteReader,ByteWriter,PacketHeader,ProbePacket}.cpp \
      GameCore/{Catalog,Design,DerivedStats,EntityRecord,Command,Update,Join,Layout,Generator,ModuleSite}.cpp \
      S/logic/*.cpp -o S/sim && S/sim

For the client's replica store (lead-forget.cpp.txt) add GameClient/ReplicaStore.cpp and Interpolation.cpp
compiled from a directory holding stub-client-pch.h.txt as pch.h, with -I GameClient. For the session table
(r1-tokens.cpp.txt) add GameLogic/Sessions.cpp with a stub Neuron::Endpoint, as that file shows.

Files:
  lead-sim.cpp.txt, lead-results.txt          the Lead's harness: the field, income per rock, an opening, host cost
  lead-forget.cpp.txt, lead-forget-results.txt  the client's forget rule under burst loss
  r1-sim.cpp.txt, r1-tokens.cpp.txt           rules: hash blindness, mining edge cases, the token collision
  r2-sim.cpp.txt, r2-results.txt              balance: fifteen openings, finite ore, the remapped field, the depot, the raid table
  r3-sim.cpp.txt, r3-results.txt              exploits: the token collision, the standoff geometry, opening lines, the ring clamp
  r4-sim.cpp.txt                              core loop: rush timelines, command counts, the standoff ring geometry
  r5-sim.cpp.txt, r5-results.txt              feasibility: the accumulator and its fix, the targeting pass, fire-event backlog, removals

Every figure quoted in the review comes from one of these outputs or is arithmetic on the tuning table;
all of it is from a cloud VM with g++, not from MSVC or the device.
