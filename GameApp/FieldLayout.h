#pragma once

namespace Outpost
{
// A rock of an asteroid field as the client draws it: where it stands from the field's center, its radius, which picks
// its mesh and scales it (ADR-011), and how it is turned.
struct FieldRock
{
  float xMeters = 0.0f;
  float zMeters = 0.0f;
  float radiusMeters = 0.0f;
  float turnRadians = 0.0f;
};

// How the client lays out an asteroid field's rocks (interface plan 2, task UI5.2): a rock in the middle and a ring of five
// to eight around it, their distances, sizes and turns drawn from the field's identifier, so that no two fields look
// alike and one field looks the same in every frame and on every machine. Where two of the ring's rocks stand further
// apart than the gap a ship could pass, smaller rocks fill between them, so that the ring reads as closed, as the circle
// the server blocks is (MVP design §4). Every rock lies inside the field's circle. Pure, so that it is tested without a GPU;
// the server blocks the whole circle whatever the client draws.
struct FieldLayout
{
  FieldRock center;
  // The ring's rocks in order around the field, each filler between the two it fills between.
  std::vector<FieldRock> ring;

  // The layout of the field _field of _radiusMeters. Every rock reaches at least _reachShare of its radius from its center
  // on the ground, whichever way it is turned (NarrowestReach), and no two neighbors on the ring leave more than
  // _gapMeters between them by that reach.
  [[nodiscard]] static FieldLayout Of(EntityId _field, float _radiusMeters, float _reachShare, float _gapMeters);

  // How far a rock mesh reaches on the ground from its origin in the direction it reaches least, in its own units: the
  // nearest of its outline's supporting lines, over a whole turn.
  [[nodiscard]] static float NarrowestReach(std::span<const Neuron::MeshVertex> _vertices) noexcept;
};
} // namespace Outpost
