#include "pch.h"

#include <array>
#include <cmath>
#include <numbers>
#include <vector>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace GameClientTests
{

namespace
{
constexpr float ASPECT = 1440.0f / 960.0f;
constexpr Outpost::PlayerId MINE = 1;
constexpr Outpost::PlayerId THEIRS = 2;

[[nodiscard]] Outpost::CameraPose Looking(float _headingRadians) noexcept
{
  return Outpost::CameraPose{.focusX = 0.0f, .focusY = 0.0f, .headingRadians = _headingRadians, .distance = 1200.0f};
}

/// A point _units along the camera's own right (or forward) from its focus, for a camera at _headingRadians.
void Relative(float _headingRadians, float _rightUnits, float _forwardUnits, float& _outX, float& _outY) noexcept
{
  const float forwardX = std::cos(_headingRadians);
  const float forwardY = std::sin(_headingRadians);
  _outX = (forwardX * _forwardUnits) + (forwardY * _rightUnits);
  _outY = (forwardY * _forwardUnits) - (forwardX * _rightUnits);
}

[[nodiscard]] Outpost::EntityRecord RecordAt(std::uint16_t _index, float _x, float _y, Outpost::PlayerId _owner, std::uint8_t _hull = 100)
{
  Outpost::EntityRecord record;
  record.identity = Outpost::PackIdentity(_index, 1);
  record.owner = _owner;
  record.positionX = Outpost::QuantizePosition(static_cast<Neuron::Fixed>(_x * Neuron::FIXED_ONE));
  record.positionY = Outpost::QuantizePosition(static_cast<Neuron::Fixed>(_y * Neuron::FIXED_ONE));
  record.designIdentity = static_cast<std::uint8_t>(Outpost::DesignId::Fighter);
  record.hullPercentRemaining = _hull;
  return record;
}

[[nodiscard]] std::vector<Outpost::AlertPlacement> PlaceOne(float _x, float _y, const Outpost::CameraPose& _camera,
                                                            std::span<const Outpost::HudRect> _panels = {})
{
  Outpost::DamageAlerts alerts;
  alerts.Note(Outpost::PackIdentity(1, 1), _x, _y, 1000);
  return Outpost::PlaceAlerts(alerts, _camera, ASPECT, _panels, 1000);
}
} // namespace

/// M3.3b, ADR-020. **"You are being attacked somewhere you cannot see."**
TEST_CLASS(TheDamageAlert)
{
public:
  /// `TechnicalDesign.md` section 8: a fire event naming one of yours raises an alert, and one naming theirs does not.
  TEST_METHOD(OnlyAShotAtYoursRaisesOne)
  {
    Outpost::ClientFrame frame;
    static_cast<void>(frame.MutableJoin().Accept(
      Outpost::JoinReply{.result = Outpost::JoinResult::Accepted, .player = MINE, .token = 0x1ull, .matchSeed = 99}));

    Outpost::Update update;
    update.sequence = 1;
    update.tick = 1;
    update.liveEntityCount = 2;
    update.records = {RecordAt(1, 5000.0f, 0.0f, MINE), RecordAt(2, -5000.0f, 0.0f, THEIRS)};
    update.fires = {Outpost::FireEvent{.shooter = Outpost::PackIdentity(9, 1), .target = Outpost::PackIdentity(1, 1), .weapon = 2},
                    Outpost::FireEvent{.shooter = Outpost::PackIdentity(9, 1), .target = Outpost::PackIdentity(2, 1), .weapon = 2}};
    std::vector<std::byte> bytes(Outpost::EncodedSize(update));
    Neuron::ByteWriter writer{bytes};
    Assert::IsTrue(Outpost::Encode(update, writer));
    Neuron::PacketQueue queue{4, 1500};
    queue.Push(bytes);
    static_cast<void>(frame.DrainPackets(queue, 1000));

    Assert::AreEqual(std::size_t{1}, frame.Alerts().Clusters().size());
    Assert::AreEqual(Outpost::PackIdentity(1, 1), frame.Alerts().Clusters().front().hit.front(), L"an alert for somebody else's ship");
  }

  /// Rule 1: nothing for what is already on screen.
  TEST_METHOD(AnAttackOnScreenRaisesNone)
  {
    Assert::IsTrue(PlaceOne(100.0f, 50.0f, Looking(0.0f)).empty());
  }

  /// **THE BEARING IS RIGHT AT SEVERAL HEADINGS, INCLUDING BEHIND THE CAMERA**, which is the case that gets the sign
  /// wrong: right is the right edge, ahead the top, behind the bottom, left the left, whichever way the camera faces.
  TEST_METHOD(TheEdgeFollowsTheBearingAtEveryHeading)
  {
    for (const float heading : {0.0f, std::numbers::pi_v<float> * 0.5f, std::numbers::pi_v<float>, -std::numbers::pi_v<float> / 3.0f})
    {
      const Outpost::CameraPose camera = Looking(heading);
      struct Case
      {
        float right;
        float forward;
        Outpost::AlertEdge edge;
      };
      for (const Case& check : {Case{6000.0f, 0.0f, Outpost::AlertEdge::Right}, Case{-6000.0f, 0.0f, Outpost::AlertEdge::Left},
                                Case{0.0f, 9000.0f, Outpost::AlertEdge::Top}, Case{0.0f, -6000.0f, Outpost::AlertEdge::Bottom}})
      {
        float x = 0.0f;
        float y = 0.0f;
        Relative(heading, check.right, check.forward, x, y);
        const std::vector<Outpost::AlertPlacement> placed = PlaceOne(x, y, camera);
        Assert::AreEqual(std::size_t{1}, placed.size());
        Assert::IsTrue(placed.front().edge == check.edge, L"the alert is on the wrong edge");
      }

      // And the convention agrees with the projection's: a point just right of the focus projects right of center.
      float x = 0.0f;
      float y = 0.0f;
      Relative(heading, 200.0f, 0.0f, x, y);
      float screenX = 0.0f;
      float screenY = 0.0f;
      Assert::IsTrue(Outpost::PlaneToScreen(camera, ASPECT, x, y, screenX, screenY));
      Assert::IsTrue(screenX > 0.0f, L"right on the plane is not right on the screen");
    }
  }

  /// Rule 5: several hits in one place are one indicator, counting the ships hit rather than the shots.
  TEST_METHOD(HitsInOnePlaceClusterWithTheRightCount)
  {
    Outpost::DamageAlerts alerts;
    for (std::uint16_t ship = 0; ship < 5; ++ship)
    {
      alerts.Note(Outpost::PackIdentity(ship, 1), 6000.0f + (ship * 40.0f), 0.0f, 1000);
      alerts.Note(Outpost::PackIdentity(ship, 1), 6000.0f + (ship * 40.0f), 0.0f, 1100);
    }
    Assert::AreEqual(std::size_t{1}, alerts.Clusters().size());
    const std::vector<Outpost::AlertPlacement> placed = Outpost::PlaceAlerts(alerts, Looking(0.0f), ASPECT, {}, 1100);
    Assert::AreEqual(std::size_t{1}, placed.size());
    Assert::AreEqual(5u, placed.front().count);
  }

  /// Rule 6: **a fourth simultaneous cluster replaces the oldest** rather than drawing a fourth indicator.
  TEST_METHOD(AFourthClusterReplacesTheOldest)
  {
    Outpost::DamageAlerts alerts;
    alerts.Note(Outpost::PackIdentity(1, 1), 6000.0f, 0.0f, 1000);
    alerts.Note(Outpost::PackIdentity(2, 1), -6000.0f, 0.0f, 1100);
    alerts.Note(Outpost::PackIdentity(3, 1), 0.0f, 9000.0f, 1200);
    alerts.Note(Outpost::PackIdentity(4, 1), 0.0f, -9000.0f, 1300);
    Assert::AreEqual(Outpost::MAX_ALERTS, alerts.Clusters().size());
    for (const Outpost::AlertCluster& cluster : alerts.Clusters())
    {
      Assert::AreNotEqual(Outpost::PackIdentity(1, 1), cluster.hit.front(), L"the oldest survived a fourth");
    }
  }

  /// Rule 4: two that would collide on one edge merge and sum their counts.
  TEST_METHOD(TwoThatWouldCollideMerge)
  {
    Outpost::DamageAlerts alerts;
    alerts.Note(Outpost::PackIdentity(1, 1), 0.0f, -8000.0f, 1000);
    alerts.Note(Outpost::PackIdentity(2, 1), 0.0f, -8000.0f + 2000.0f, 1100);
    alerts.Note(Outpost::PackIdentity(3, 1), 300.0f, -9500.0f, 1200);
    const std::vector<Outpost::AlertPlacement> placed = Outpost::PlaceAlerts(alerts, Looking(0.0f), ASPECT, {}, 1200);
    Assert::AreEqual(std::size_t{1}, placed.size(), L"two alerts overlapped on one edge");
    Assert::AreEqual(3u, placed.front().count);
  }

  /// Rule 3: the body stays clear of every panel -- here the credits panel in the top-left corner.
  TEST_METHOD(TheClampClearsThePanels)
  {
    const std::array<Outpost::HudRect, 1> panels{Outpost::CREDITS_PANEL};
    float x = 0.0f;
    float y = 0.0f;
    Relative(0.0f, -9000.0f, 5990.0f, x, y);
    const std::vector<Outpost::AlertPlacement> placed = PlaceOne(x, y, Looking(0.0f), panels);
    Assert::AreEqual(std::size_t{1}, placed.size());
    const Outpost::HudRect& body = placed.front().body;
    const bool clear = (body.y >= (Outpost::CREDITS_PANEL.y + Outpost::CREDITS_PANEL.h + Outpost::ALERT_PANEL_CLEAR_PIXELS)) ||
                       (body.x >= (Outpost::CREDITS_PANEL.x + Outpost::CREDITS_PANEL.w + Outpost::ALERT_PANEL_CLEAR_PIXELS));
    Assert::IsTrue(clear, L"an alert sat on the credits panel");
  }

  /// The target is at least the combat tier's 64 x 64, and a tap on it recenters there.
  TEST_METHOD(TheTargetIsLargeAndRecenters)
  {
    Outpost::HudState state;
    state.alerts = PlaceOne(6000.0f, 0.0f, Looking(0.0f));
    const Outpost::HudFrame frame = Outpost::BuildHud(state);
    bool found = false;
    for (const Outpost::HudTarget& target : frame.hits.Targets())
    {
      if (target.action == Outpost::HudAction::RecenterOnAlert)
      {
        found = true;
        Assert::IsTrue((target.hit.w >= 64) && (target.hit.h >= 64));
        Assert::IsTrue(target.tier == Outpost::TouchTier::Combat);
      }
    }
    Assert::IsTrue(found, L"the alert has no target");
  }

  /// It fades over its lifetime and then goes.
  TEST_METHOD(ItFadesAndExpires)
  {
    Outpost::DamageAlerts alerts;
    alerts.Note(Outpost::PackIdentity(1, 1), 6000.0f, 0.0f, 1000);
    const float late =
      Outpost::PlaceAlerts(alerts, Looking(0.0f), ASPECT, {}, 1000 + Outpost::ALERT_LIFETIME_MILLISECONDS - 100).front().alpha;
    Assert::IsTrue(late < 0.1f);
    alerts.Expire(1000 + Outpost::ALERT_LIFETIME_MILLISECONDS);
    Assert::IsTrue(alerts.Clusters().empty());
  }
};

/// M3.3b. **A hull bar only on what is damaged, at a fixed size.**
TEST_CLASS(TheHullBars)
{
public:
  TEST_METHOD(OnlyTheDamagedDrawABar)
  {
    const std::vector<Outpost::EntityRecord> drawn{RecordAt(1, 0.0f, 0.0f, MINE, 100), RecordAt(2, 100.0f, 0.0f, THEIRS, 40)};
    const std::vector<Outpost::HullBarPlacement> bars = Outpost::PlaceHullBars(drawn, Looking(0.0f), ASPECT);
    Assert::AreEqual(std::size_t{1}, bars.size(), L"an undamaged ship drew a bar");
    Assert::AreEqual(std::uint8_t{40}, bars.front().percentRemaining);
  }

  /// The same 18 x 6 near and far: only its position follows the camera.
  TEST_METHOD(ItSitsAboveTheShipAtAnyDistance)
  {
    const std::vector<Outpost::EntityRecord> drawn{RecordAt(1, 0.0f, 0.0f, THEIRS, 50)};
    for (const float distance : {800.0f, 4000.0f})
    {
      Outpost::CameraPose camera = Looking(0.0f);
      camera.distance = distance;
      const std::vector<Outpost::HullBarPlacement> bars = Outpost::PlaceHullBars(drawn, camera, ASPECT);
      Assert::AreEqual(std::size_t{1}, bars.size());
      Assert::IsTrue(bars.front().y < 480, L"the bar is not above the ship");
      Assert::IsTrue(std::abs((bars.front().x + (Outpost::HULL_BAR_WIDTH_PIXELS / 2)) - 720) <= 1, L"the bar is not centered on it");
    }
  }
};

} // namespace GameClientTests
