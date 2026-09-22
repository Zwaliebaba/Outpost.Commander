// MeshCatalog.g.h - generated from manifest.json. Do not hand-edit.
// One entry per shipped mesh. The renderer issues ONE INSTANCED DRAW PER ENTRY and the owner's
// colour arrives as per-instance constant data, so this list is also the draw-call budget:
// 13 instanced draws per frame covers every owner at every player count.
#pragma once
#include <cstdint>
#include <array>

namespace oc::content {

struct MeshAsset {
    const wchar_t* name;          // logical name, matches the design handoff
    const wchar_t* packageUri;    // ms-appx:/// URI inside the read-only install location
    uint32_t       triangles;
    uint32_t       vertices;      // face-split; always triangles * 3
    float          extentMin[3];  // world units, asserted against the .cmo at load
    float          extentMax[3];
};

// Vertex colour selector, restated here so a reader never has to go looking:
//   R: 0 = hull palette, 255 = the owning player's team colour. No other value is emitted.
//   G: 0 = HULL.DEEP, 128 = HULL.BASE, 255 = HULL.EDGE. Ignored where R = 255.
//   B: reserved, always 0.   A: always 255.
// albedo = lerp(HULL[G], TEAM[owner], R/255)

inline constexpr std::array<MeshAsset, 13> kMeshAssets = {{
    { L"Scout", L"ms-appx:///Assets/Meshes/Scout.cmo", 158, 474,
      { -27.00f, -8.00f, -30.00f }, { 27.00f, 10.00f, 30.00f } },
    { L"Frigate", L"ms-appx:///Assets/Meshes/Frigate.cmo", 190, 570,
      { -23.20f, -9.00f, -45.00f }, { 23.20f, 10.00f, 45.00f } },
    { L"ModuleFrame", L"ms-appx:///Assets/Meshes/ModuleFrame.cmo", 248, 744,
      { -41.76f, -9.00f, -41.11f }, { 41.76f, 9.00f, 42.11f } },
    { L"ModuleShipyardL1", L"ms-appx:///Assets/Meshes/ModuleShipyardL1.cmo", 204, 612,
      { -32.19f, -10.00f, -45.00f }, { 32.19f, 20.00f, 45.00f } },
    { L"ModuleShipyardL2", L"ms-appx:///Assets/Meshes/ModuleShipyardL2.cmo", 264, 792,
      { -32.19f, -10.00f, -45.00f }, { 32.19f, 34.00f, 45.00f } },
    { L"ModuleOreProcessorL1", L"ms-appx:///Assets/Meshes/ModuleOreProcessorL1.cmo", 212, 636,
      { -41.35f, -11.00f, -37.00f }, { 37.00f, 17.00f, 39.32f } },
    { L"ModuleOreProcessorL2", L"ms-appx:///Assets/Meshes/ModuleOreProcessorL2.cmo", 268, 804,
      { -43.25f, -11.00f, -30.00f }, { 45.07f, 17.00f, 30.00f } },
    { L"Station", L"ms-appx:///Assets/Meshes/Station.cmo", 500, 1500,
      { -110.00f, -12.00f, -110.00f }, { 110.00f, 42.00f, 110.00f } },
    { L"AsteroidA", L"ms-appx:///Assets/Meshes/AsteroidA.cmo", 126, 378,
      { -31.23f, -22.62f, -29.88f }, { 31.21f, 22.62f, 29.64f } },
    { L"AsteroidB", L"ms-appx:///Assets/Meshes/AsteroidB.cmo", 126, 378,
      { -38.52f, -30.30f, -38.83f }, { 43.97f, 30.30f, 38.33f } },
    { L"AsteroidC", L"ms-appx:///Assets/Meshes/AsteroidC.cmo", 126, 378,
      { -51.49f, -40.80f, -46.92f }, { 59.63f, 40.80f, 46.62f } },
    { L"AsteroidD", L"ms-appx:///Assets/Meshes/AsteroidD.cmo", 126, 378,
      { -66.77f, -48.27f, -69.41f }, { 55.29f, 48.27f, 58.46f } },
    { L"AsteroidE", L"ms-appx:///Assets/Meshes/AsteroidE.cmo", 126, 378,
      { -90.91f, -70.68f, -71.09f }, { 75.97f, 70.68f, 69.69f } },
}};

// Hull palette as shader constants. G is quantised to exactly these three slots.
inline constexpr float kHullPalette[3][3] = {
    { 0.106f, 0.129f, 0.165f },   // HULL.DEEP #1B212A
    { 0.255f, 0.294f, 0.345f },   // HULL.BASE #414B58
    { 0.510f, 0.557f, 0.612f },   // HULL.EDGE #828E9C
};

} // namespace oc::content
