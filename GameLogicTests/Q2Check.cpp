#include "pch.h"
#include "Q2Check.h"

#include <atomic>
#include <cmath>
#include <deque>
#include <functional>
#include <map>
#include <numbers>
#include <set>
#include <thread>

namespace
{
using GameLogicTests::CheckDesign;
using GameLogicTests::CheckParts;
using GameLogicTests::FireMode;

constexpr std::uint32_t TICKS_PER_SECOND = 20;
// Each battle's Ore is drawn from within 15% of the nominal budget, the same for both sides.
constexpr double BUDGET_SPREAD = 0.15;
// Both sides' front rows start this far apart, out of range of every weapon.
constexpr float START_GAP_METERS = 400.0f;
// Room between neighbors in a side's starting grid beyond its ships' footprints, as in a starting fleet.
constexpr float GRID_MARGIN_METERS = 8.0f;
// Open ground, wide enough that no side reaches the edge.
constexpr float ARENA_METERS = 12000.0f;
constexpr double TIME_LIMIT_SECONDS = 600.0;

// The model's thresholds (Tools/BattleModel.py).
constexpr double COUNTER_WIN_RATE = 0.8;
constexpr double ROBUST_WIN_RATE = 0.5;
constexpr double PERTURBATION = 0.05;
constexpr double WORTH_BUILDING = 0.05;
constexpr double CONFIDENCE_Z = 1.96;
constexpr int EQUILIBRIUM_ITERATIONS = 40000;

constexpr Outpost::PlayerId FIRST{1};
constexpr Outpost::PlayerId SECOND{2};

// The model's short names: a hull's or drive's initial, a weapon's initials or its first two letters.
std::string ShortName(std::string_view _name, bool _initials)
{
  if (!_initials)
    return std::string(1, _name.front());
  if (_name.find(' ') == std::string_view::npos)
    return std::string(_name.substr(0, 2));
  std::string initials;
  bool wordStart = true;
  for (const char letter : _name)
  {
    if (wordStart && letter != ' ')
      initials += letter;
    wordStart = letter == ' ';
  }
  return initials;
}

// FNV-1a, so that every battle has its own seed and the same battle always the same one.
std::uint64_t SeedFor(const CheckDesign& _a, const CheckDesign& _b, double _budgetOre, FireMode _mode, std::uint32_t _battle)
{
  std::uint64_t hash = 14695981039346656037ULL;
  const auto mix = [&hash](std::string_view _bytes)
  {
    for (const char byte : _bytes)
    {
      hash ^= static_cast<unsigned char>(byte);
      hash *= 1099511628211ULL;
    }
  };
  mix(_a.code);
  mix("|");
  mix(_b.code);
  mix(std::format("|{}|{}|{}", _budgetOre, static_cast<int>(_mode), _battle));
  return hash;
}

// ---- Running many battles on every hardware thread ----------------------------------------------------------------

struct Pairing
{
  const CheckDesign* a = nullptr;
  const CheckDesign* b = nullptr;
  double budgetOre = 0.0;
  FireMode mode = FireMode::Spread;
  std::uint32_t battles = 0;
};

struct Record
{
  std::uint32_t wins = 0;
  std::uint32_t losses = 0;
};

// Each pairing's wins and losses for its first design. Every battle is one job, so the threads stay busy.
std::vector<Record> RunPairings(const std::vector<Pairing>& _pairings, const GameLogicTests::CheckOptions& _options)
{
  std::vector<std::pair<size_t, std::uint32_t>> jobs;
  for (size_t pairing = 0; pairing < _pairings.size(); ++pairing)
  {
    for (std::uint32_t battle = 0; battle < _pairings[pairing].battles; ++battle)
      jobs.emplace_back(pairing, battle);
  }
  std::vector<int> outcomes(jobs.size());
  std::atomic<size_t> next{0};
  std::atomic<bool> failed{false};
  std::string failure;
  const auto work = [&]
  {
    try
    {
      for (size_t job = next++; job < jobs.size() && !failed; job = next++)
      {
        const Pairing& pairing = _pairings[jobs[job].first];
        outcomes[job] =
          GameLogicTests::Fight(*pairing.a, *pairing.b, pairing.budgetOre, pairing.mode, jobs[job].second, pairing.battles, _options.fog);
      }
    }
    catch (const std::exception& error)
    {
      if (!failed.exchange(true))
        failure = error.what();
    }
  };
  const std::uint32_t threads = _options.threads != 0 ? _options.threads : std::max(1u, std::thread::hardware_concurrency());
  {
    std::vector<std::jthread> workers;
    workers.reserve(threads);
    for (std::uint32_t i = 0; i < threads; ++i)
      workers.emplace_back(work);
  }
  if (failed)
    throw Neuron::Exception(failure);

  std::vector<Record> records(_pairings.size());
  for (size_t job = 0; job < jobs.size(); ++job)
  {
    Record& record = records[jobs[job].first];
    record.wins += outcomes[job] > 0 ? 1 : 0;
    record.losses += outcomes[job] < 0 ? 1 : 0;
  }
  return records;
}

// ---- Verdicts ------------------------------------------------------------------------------------------------------

// The 95% confidence interval of a win rate of _wins in _battles (Wilson).
std::pair<double, double> Wilson(std::uint32_t _wins, std::uint32_t _battles)
{
  const double z = CONFIDENCE_Z;
  const double n = _battles;
  const double p = _wins / n;
  const double denominator = 1.0 + (z * z / n);
  const double center = (p + (z * z / (2.0 * n))) / denominator;
  const double half = z * std::sqrt((p * (1.0 - p) / n) + (z * z / (4.0 * n * n))) / denominator;
  return {center - half, center + half};
}

enum class Verdict : std::uint8_t
{
  Pass,
  Fail,
  Unsure
};

Verdict Judge(std::uint32_t _wins, std::uint32_t _battles, double _threshold)
{
  const auto [low, high] = Wilson(_wins, _battles);
  return low >= _threshold ? Verdict::Pass : high < _threshold ? Verdict::Fail : Verdict::Unsure;
}

// One way a design could meet a job: a counter to a design, an answer to a researched design, or a counter under a
// perturbation.
struct Candidate
{
  Pairing pairing;
  std::uint32_t wins = 0;

  [[nodiscard]] double Rate() const
  {
    return static_cast<double>(wins) / pairing.battles;
  }
};

using Contest = std::vector<Candidate>;

struct Settled
{
  Candidate best;
  Verdict verdict = Verdict::Fail;
};

// The best candidate and the verdict of each contest, re-running the uncertain ones with more battles. A contest passes
// if any of its candidates does.
std::vector<Settled> Settle(const std::vector<Contest>& _contests, double _threshold, const GameLogicTests::CheckOptions& _options)
{
  std::vector<Settled> results(_contests.size());
  std::vector<std::pair<size_t, Pairing>> rerun;
  const auto best = [](const Contest& _contest) { return *std::ranges::max_element(_contest, {}, &Candidate::Rate); };
  for (size_t c = 0; c < _contests.size(); ++c)
  {
    bool pass = false;
    bool unsure = false;
    for (const Candidate& candidate : _contests[c])
    {
      const Verdict verdict = Judge(candidate.wins, candidate.pairing.battles, _threshold);
      pass |= verdict == Verdict::Pass;
      unsure |= verdict == Verdict::Unsure;
    }
    results[c] = {best(_contests[c]), pass ? Verdict::Pass : unsure ? Verdict::Unsure : Verdict::Fail};
    if (!pass && unsure && _options.maxBattles > _contests[c].front().pairing.battles)
    {
      for (const Candidate& candidate : _contests[c])
      {
        if (Judge(candidate.wins, candidate.pairing.battles, _threshold) == Verdict::Unsure)
        {
          Pairing more = candidate.pairing;
          more.battles = _options.maxBattles;
          rerun.emplace_back(c, more);
        }
      }
    }
  }
  if (rerun.empty())
    return results;

  std::vector<Pairing> pairings;
  pairings.reserve(rerun.size());
  for (const auto& [contest, pairing] : rerun)
    pairings.push_back(pairing);
  const std::vector<Record> records = RunPairings(pairings, _options);
  std::map<size_t, Contest> again;
  for (size_t i = 0; i < rerun.size(); ++i)
    again[rerun[i].first].push_back({rerun[i].second, records[i].wins});
  for (const auto& [contest, candidates] : again)
  {
    bool pass = false;
    bool unsure = false;
    for (const Candidate& candidate : candidates)
    {
      const Verdict verdict = Judge(candidate.wins, candidate.pairing.battles, _threshold);
      pass |= verdict == Verdict::Pass;
      unsure |= verdict == Verdict::Unsure;
    }
    results[contest] = {best(candidates), pass ? Verdict::Pass : unsure ? Verdict::Unsure : Verdict::Fail};
  }
  return results;
}

// ---- The win-rate matrix and the equilibrium mix --------------------------------------------------------------------

std::vector<std::vector<double>> WinMatrix(const std::vector<CheckDesign>& _designs, double _budgetOre, FireMode _mode,
                                           const GameLogicTests::CheckOptions& _options)
{
  std::vector<Pairing> pairings;
  std::vector<std::pair<size_t, size_t>> pairs;
  for (size_t i = 0; i < _designs.size(); ++i)
  {
    for (size_t j = i + 1; j < _designs.size(); ++j)
    {
      pairs.emplace_back(i, j);
      pairings.push_back({&_designs[i], &_designs[j], _budgetOre, _mode, _options.battles});
    }
  }
  const std::vector<Record> records = RunPairings(pairings, _options);
  std::vector<std::vector<double>> matrix(_designs.size(), std::vector<double>(_designs.size(), 0.5));
  for (size_t k = 0; k < pairs.size(); ++k)
  {
    matrix[pairs[k].first][pairs[k].second] = static_cast<double>(records[k].wins) / _options.battles;
    matrix[pairs[k].second][pairs[k].first] = static_cast<double>(records[k].losses) / _options.battles;
  }
  return matrix;
}

// The mix of designs neither side can improve on, found by fictitious play on the win-minus-loss payoff, as the model
// finds it.
std::vector<double> Equilibrium(const std::vector<std::vector<double>>& _matrix)
{
  const size_t n = _matrix.size();
  std::vector<int> counts(n, 0);
  std::vector<double> payoff(n, 0.0);
  size_t best = 0;
  for (int iteration = 0; iteration < EQUILIBRIUM_ITERATIONS; ++iteration)
  {
    ++counts[best];
    for (size_t i = 0; i < n; ++i)
      payoff[i] += _matrix[i][best] - _matrix[best][i];
    best = static_cast<size_t>(std::ranges::max_element(payoff) - payoff.begin());
  }
  std::vector<double> mix(n);
  for (size_t i = 0; i < n; ++i)
    mix[i] = static_cast<double>(counts[i]) / EQUILIBRIUM_ITERATIONS;
  return mix;
}

// ---- Research --------------------------------------------------------------------------------------------------------

std::vector<const Outpost::ResearchTopicTuning*> WithPrerequisites(const Outpost::ResearchTopicTuning& _topic,
                                                                   const std::vector<Outpost::ResearchTopicTuning>& _topics)
{
  std::vector<const Outpost::ResearchTopicTuning*> chain;
  const std::function<void(const Outpost::ResearchTopicTuning&)> visit = [&](const Outpost::ResearchTopicTuning& _visited)
  {
    for (const Outpost::ResearchTopicId required : _visited.prerequisites)
      visit(*std::ranges::find(_topics, required, &Outpost::ResearchTopicTuning::id));
    if (std::ranges::find(chain, &_visited) == chain.end())
      chain.push_back(&_visited);
  };
  visit(_topic);
  return chain;
}

template <typename IdType> bool Unlocks(const Outpost::ResearchTopicTuning& _topic, IdType _id)
{
  const IdType* unlocked = std::get_if<IdType>(&_topic.effect);
  return unlocked != nullptr && *unlocked == _id;
}

// The parts a player can build once _researched is done, with every upgrade in it applied.
CheckParts Researched(const CheckParts& _parts, const Outpost::Tuning& _tuning,
                      const std::vector<const Outpost::ResearchTopicTuning*>& _researched)
{
  const auto locked = [&](auto _id)
  {
    const bool unlockedByAny =
      std::ranges::any_of(_tuning.research, [_id](const Outpost::ResearchTopicTuning& _topic) { return Unlocks(_topic, _id); });
    const bool unlockedHere =
      std::ranges::any_of(_researched, [_id](const Outpost::ResearchTopicTuning* _topic) { return Unlocks(*_topic, _id); });
    return unlockedByAny && !unlockedHere;
  };
  CheckParts parts;
  std::ranges::copy_if(_parts.hulls, std::back_inserter(parts.hulls),
                       [&](const GameLogicTests::CheckHull& _hull) { return !locked(_hull.id); });
  std::ranges::copy_if(_parts.drives, std::back_inserter(parts.drives),
                       [&](const GameLogicTests::CheckDrive& _drive) { return !locked(_drive.id); });
  std::ranges::copy_if(_parts.weapons, std::back_inserter(parts.weapons),
                       [&](const GameLogicTests::CheckWeapon& _weapon) { return !locked(_weapon.id); });

  // Upgrades of one stat add their percentages (ADR-033). Those a clump's battle cannot feel, its economy, its structures,
  // its speed and its Constructors, change nothing here; a gateway does nothing at all.
  std::int32_t hullPercent = 0;
  std::vector<std::pair<Outpost::WeaponId, std::int32_t>> weaponPercents;
  for (const Outpost::ResearchTopicTuning* topic : _researched)
  {
    const auto* upgrade = std::get_if<Outpost::UpgradeEffect>(&topic->effect);
    if (upgrade == nullptr)
      continue;
    if (upgrade->target == Outpost::UpgradeTarget::AllHulls && upgrade->stat == Outpost::UpgradeStat::HitPoints)
      hullPercent += upgrade->percent;
    else if (upgrade->target == Outpost::UpgradeTarget::Weapon && upgrade->stat == Outpost::UpgradeStat::FireRate)
    {
      const auto found = std::ranges::find(weaponPercents, upgrade->weapon, &std::pair<Outpost::WeaponId, std::int32_t>::first);
      if (found != weaponPercents.end())
        found->second += upgrade->percent;
      else
        weaponPercents.emplace_back(upgrade->weapon, upgrade->percent);
    }
  }
  for (GameLogicTests::CheckHull& hull : parts.hulls)
    hull.hitPoints *= 1.0 + (hullPercent / 100.0);
  for (const auto& [id, percent] : weaponPercents)
  {
    for (GameLogicTests::CheckWeapon& weapon : parts.weapons)
    {
      if (weapon.id == id)
        weapon.fireIntervalSeconds /= 1.0 + (percent / 100.0);
    }
  }
  return parts;
}

bool AffectsBattles(const Outpost::ResearchTopicTuning& _topic)
{
  if (_topic.IsGateway())
    return false;
  const auto* upgrade = std::get_if<Outpost::UpgradeEffect>(&_topic.effect);
  return upgrade == nullptr || upgrade->target == Outpost::UpgradeTarget::AllHulls || upgrade->target == Outpost::UpgradeTarget::Weapon;
}

// ---- The check ---------------------------------------------------------------------------------------------------------

struct Leg
{
  std::string counter;
  std::string target;
  double budgetOre = 0.0;
  FireMode mode = FireMode::Spread;

  friend auto operator<=>(const Leg&, const Leg&) = default;
};

struct Failures
{
  std::map<std::string, std::vector<std::string>> lines;
};

std::string ModeName(FireMode _mode)
{
  return _mode == FireMode::Focus ? "focus" : "spread";
}

std::string Ore(double _budgetOre)
{
  const auto whole = static_cast<long long>(_budgetOre);
  return whole >= 1000 ? std::format("{},{:03}", whole / 1000, whole % 1000) : std::format("{}", whole);
}

std::string Percent(const Candidate& _candidate)
{
  return std::format("{:.0f}% of {}", 100.0 * _candidate.Rate(), _candidate.pairing.battles);
}

// The hulls, drives and weapons that no design in _used has, as "Large hull, Fusion drive"; empty when every one is used.
std::string UnusedComponents(const CheckParts& _parts, const std::array<std::set<std::string>, 3>& _used)
{
  std::string unused;
  const auto add = [&unused](const std::string& _name, std::string_view _kind)
  { unused += std::format("{}{} {}", unused.empty() ? "" : ", ", _name, _kind); };
  for (const GameLogicTests::CheckHull& hull : _parts.hulls)
  {
    if (!_used[0].contains(hull.name))
      add(hull.name, "hull");
  }
  for (const GameLogicTests::CheckDrive& drive : _parts.drives)
  {
    if (!_used[1].contains(drive.name))
      add(drive.name, "drive");
  }
  for (const GameLogicTests::CheckWeapon& weapon : _parts.weapons)
  {
    if (!_used[2].contains(weapon.name))
      add(weapon.name, "weapon");
  }
  return unused;
}

void RunStage(std::string_view _label, const std::vector<CheckDesign>& _designs, const CheckParts& _parts,
              const std::vector<double>& _budgets, const GameLogicTests::CheckOptions& _options, Failures& _failures, std::set<Leg>& _legs,
              std::string& _report)
{
  _report += std::format("\n==== {}: {} designs ====\n", _label, _designs.size());
  // (b) is judged over the stage: every component is worth building at one budget or more, in each fire mode (owner,
  // 2026-10-01). A heavy hull need not pay at the smallest budget, nor a medium one at the largest.
  std::map<FireMode, std::array<std::set<std::string>, 3>> usedInStage;
  for (const double budget : _budgets)
  {
    for (const FireMode mode : {FireMode::Spread, FireMode::Focus})
    {
      const std::string where = std::format("{}, {} Ore {}", _label, Ore(budget), ModeName(mode));
      const std::vector<std::vector<double>> matrix = WinMatrix(_designs, budget, mode, _options);
      const std::vector<double> mix = Equilibrium(matrix);
      std::vector<size_t> built;
      for (size_t i = 0; i < mix.size(); ++i)
      {
        if (mix[i] >= WORTH_BUILDING)
          built.push_back(i);
      }
      std::ranges::sort(built, [&mix](size_t _a, size_t _b) { return mix[_a] > mix[_b]; });
      std::string worth;
      for (const size_t i : built)
        worth += std::format("{}{} {:.0f}%", worth.empty() ? "" : ", ", _designs[i].code, 100.0 * mix[i]);
      _report += std::format("\n{} Ore, {} fire. Worth building: {}\n", Ore(budget), ModeName(mode), worth);

      std::vector<Contest> contests;
      for (size_t j = 0; j < _designs.size(); ++j)
      {
        Contest contest;
        for (size_t i = 0; i < _designs.size(); ++i)
        {
          if (i != j)
          {
            contest.push_back({{&_designs[i], &_designs[j], budget, mode, _options.battles},
                               static_cast<std::uint32_t>(std::lround(matrix[i][j] * _options.battles))});
          }
        }
        contests.push_back(std::move(contest));
      }
      const std::vector<Settled> settled = Settle(contests, COUNTER_WIN_RATE, _options);
      for (size_t j = 0; j < settled.size(); ++j)
      {
        const Candidate& best = settled[j].best;
        if (settled[j].verdict != Verdict::Pass)
        {
          _failures.lines[settled[j].verdict == Verdict::Fail ? "a" : "a?"].push_back(
            std::format("{}: the best counter to {} is {} at {}", where, _designs[j].code, best.pairing.a->code, Percent(best)));
        }
        if (std::ranges::find(built, j) != built.end())
        {
          _legs.insert({best.pairing.a->code, _designs[j].code, budget, mode});
          _report += std::format("  {:>9} is countered by {} ({})\n", _designs[j].code, best.pairing.a->code, Percent(best));
        }
      }

      std::array<std::set<std::string>, 3>& used = usedInStage[mode];
      std::array<std::set<std::string>, 3> here;
      for (const size_t i : built)
      {
        for (std::array<std::set<std::string>, 3>* sets : {&used, &here})
        {
          (*sets)[0].insert(_designs[i].hull);
          (*sets)[1].insert(_designs[i].drive);
          (*sets)[2].insert(_designs[i].weapon);
        }
      }
      const std::string unused = UnusedComponents(_parts, here);
      if (!unused.empty())
        _report += std::format("  Not worth building at this budget: {}\n", unused);
    }
  }
  for (const auto& [mode, used] : usedInStage)
  {
    const std::string unused = UnusedComponents(_parts, used);
    if (!unused.empty())
      _failures.lines["b"].push_back(
        std::format("{}, {} fire: no design worth building at any budget uses the {}", _label, ModeName(mode), unused));
  }
}

void RunResearchCheck(const Outpost::Tuning& _tuning, const CheckParts& _parts, const GameLogicTests::CheckOptions& _options,
                      Failures& _failures, std::string& _report)
{
  const std::vector<CheckDesign> answers = GameLogicTests::DesignsFrom(_tuning, Researched(_parts, _tuning, {}));
  _report += std::format("\n==== One-sided research (d): each topic against the {} starting designs ====\n", answers.size());
  for (const Outpost::ResearchTopicTuning& topic : _tuning.research)
  {
    // The MVP's (d), over tier 1's topics; the later tiers get theirs with their stages (task 10.3).
    if (topic.tier != 1)
      continue;
    if (!AffectsBattles(topic))
    {
      _report += std::format("  {}: no effect on a battle\n", topic.name);
      continue;
    }
    const std::vector<const Outpost::ResearchTopicTuning*> researched = WithPrerequisites(topic, _tuning.research);
    const std::vector<CheckDesign> designs = GameLogicTests::DesignsFrom(_tuning, Researched(_parts, _tuning, researched));
    const bool unlocksUnmodelled = std::visit(
      [&designs]<typename Effect>([[maybe_unused]] const Effect& _effect)
      {
        if constexpr (std::is_same_v<Effect, Outpost::UpgradeEffect> || std::is_same_v<Effect, Outpost::GatewayEffect>)
          return false;
        else
        {
          return std::ranges::none_of(designs,
                                      [&_effect](const CheckDesign& _design)
                                      {
                                        if constexpr (std::is_same_v<Effect, Outpost::HullId>)
                                          return _design.components.hull == _effect;
                                        else if constexpr (std::is_same_v<Effect, Outpost::DriveId>)
                                          return _design.components.drive == _effect;
                                        else
                                          return _design.components.weapon == _effect;
                                      });
        }
      },
      topic.effect);
    if (unlocksUnmodelled)
    {
      _report += std::format("  {}: not modelled\n", topic.name);
      continue;
    }

    std::vector<Pairing> pairings;
    for (const double budget : _options.earlyBudgets)
    {
      for (const FireMode mode : {FireMode::Spread, FireMode::Focus})
      {
        for (const CheckDesign& design : designs)
        {
          for (const CheckDesign& answer : answers)
            pairings.push_back({&answer, &design, budget, mode, _options.battles});
        }
      }
    }
    const std::vector<Record> records = RunPairings(pairings, _options);
    std::vector<Contest> contests;
    for (size_t k = 0; k < pairings.size(); k += answers.size())
    {
      Contest contest;
      for (size_t y = 0; y < answers.size(); ++y)
        contest.push_back({pairings[k + y], records[k + y].wins});
      contests.push_back(std::move(contest));
    }
    const std::vector<Settled> settled = Settle(contests, ROBUST_WIN_RATE, _options);

    std::string chain;
    for (const Outpost::ResearchTopicTuning* step : researched)
      chain += std::format("{}{}", chain.empty() ? "" : " + ", step->name);
    const Settled& weakest = *std::ranges::min_element(settled, {}, [](const Settled& _settled) { return _settled.best.Rate(); });
    _report += std::format("  {}: weakest answer is {} to {}* at {} ({} Ore {})\n", chain, weakest.best.pairing.a->code,
                           weakest.best.pairing.b->code, Percent(weakest.best), Ore(weakest.best.pairing.budgetOre),
                           ModeName(weakest.best.pairing.mode));
    for (const Settled& result : settled)
    {
      if (result.verdict != Verdict::Pass)
      {
        _failures.lines[result.verdict == Verdict::Fail ? "d" : "d?"].push_back(
          std::format("{}, {} Ore {}: the best answer to {}* is {} at {}", chain, Ore(result.best.pairing.budgetOre),
                      ModeName(result.best.pairing.mode), result.best.pairing.b->code, result.best.pairing.a->code, Percent(result.best)));
      }
    }
  }
}

// Every set of parts with one number the model moves changed by 5% up or down, and its label.
std::vector<std::pair<std::string, CheckParts>> Perturbed(const CheckParts& _parts)
{
  std::vector<std::pair<std::string, CheckParts>> changed;
  const auto add = [&](const std::string& _name, std::string_view _field, auto _member, auto _part, auto _list)
  {
    for (const double sign : {-1.0, 1.0})
    {
      CheckParts parts = _parts;
      auto& entry = *std::ranges::find((parts.*_list), _part->id, [](const auto& _each) { return _each.id; });
      entry.*_member *= 1.0 + (sign * PERTURBATION);
      changed.emplace_back(std::format("{}.{} {}5%", _name, _field, sign < 0.0 ? "-" : "+"), std::move(parts));
    }
  };
  for (const GameLogicTests::CheckHull& hull : _parts.hulls)
  {
    add(hull.name, "hp", &GameLogicTests::CheckHull::hitPoints, &hull, &CheckParts::hulls);
    add(hull.name, "armour", &GameLogicTests::CheckHull::armor, &hull, &CheckParts::hulls);
    add(hull.name, "speed", &GameLogicTests::CheckHull::speedMetersPerSecond, &hull, &CheckParts::hulls);
    add(hull.name, "cost", &GameLogicTests::CheckHull::cost, &hull, &CheckParts::hulls);
  }
  for (const GameLogicTests::CheckDrive& drive : _parts.drives)
  {
    add(drive.name, "speed_factor", &GameLogicTests::CheckDrive::speedFactor, &drive, &CheckParts::drives);
    add(drive.name, "hp_factor", &GameLogicTests::CheckDrive::hitPointsFactor, &drive, &CheckParts::drives);
    add(drive.name, "cost", &GameLogicTests::CheckDrive::cost, &drive, &CheckParts::drives);
  }
  for (const GameLogicTests::CheckWeapon& weapon : _parts.weapons)
  {
    add(weapon.name, "damage", &GameLogicTests::CheckWeapon::damage, &weapon, &CheckParts::weapons);
    add(weapon.name, "interval", &GameLogicTests::CheckWeapon::fireIntervalSeconds, &weapon, &CheckParts::weapons);
    add(weapon.name, "range", &GameLogicTests::CheckWeapon::rangeMeters, &weapon, &CheckParts::weapons);
    add(weapon.name, "cost", &GameLogicTests::CheckWeapon::cost, &weapon, &CheckParts::weapons);
    if (weapon.splashRadiusMeters > 0.0)
      add(weapon.name, "splash", &GameLogicTests::CheckWeapon::splashRadiusMeters, &weapon, &CheckParts::weapons);
  }
  return changed;
}

void RunRobustness(const Outpost::Tuning& _tuning, const CheckParts& _parts, const std::set<Leg>& _legs,
                   const GameLogicTests::CheckOptions& _options, Failures& _failures, std::string& _report)
{
  const std::vector<CheckDesign> designs = GameLogicTests::DesignsFrom(_tuning, _parts);
  const auto byCode = [](const std::vector<CheckDesign>& _list, const std::string& _code)
  { return &*std::ranges::find(_list, _code, &CheckDesign::code); };

  // Keeps every perturbed design alive while its pairings run.
  std::deque<std::vector<CheckDesign>> changedDesigns;
  std::vector<Pairing> pairings;
  std::vector<std::string> labels;
  for (auto& [label, parts] : Perturbed(_parts))
  {
    const std::vector<CheckDesign>& changed = changedDesigns.emplace_back(GameLogicTests::DesignsFrom(_tuning, parts));
    for (const Leg& leg : _legs)
    {
      const CheckDesign* counter = byCode(changed, leg.counter);
      const CheckDesign* target = byCode(changed, leg.target);
      if (counter->stats == byCode(designs, leg.counter)->stats && counter->cost == byCode(designs, leg.counter)->cost &&
          target->stats == byCode(designs, leg.target)->stats && target->cost == byCode(designs, leg.target)->cost)
        continue;
      pairings.push_back({counter, target, leg.budgetOre, leg.mode, _options.robustBattles});
      labels.push_back(label);
    }
  }
  const std::vector<Record> records = RunPairings(pairings, _options);
  std::vector<Contest> contests;
  contests.reserve(pairings.size());
  for (size_t k = 0; k < pairings.size(); ++k)
    contests.push_back({{pairings[k], records[k].wins}});
  const std::vector<Settled> settled = Settle(contests, ROBUST_WIN_RATE, _options);
  for (size_t k = 0; k < settled.size(); ++k)
  {
    if (settled[k].verdict != Verdict::Pass)
    {
      const Candidate& best = settled[k].best;
      _failures.lines[settled[k].verdict == Verdict::Fail ? "c" : "c?"].push_back(
        std::format("{}: {} no longer beats {} at {} Ore {} fire ({})", labels[k], best.pairing.a->code, best.pairing.b->code,
                    Ore(best.pairing.budgetOre), ModeName(best.pairing.mode), Percent(best)));
    }
  }
  _report += std::format("\nRobustness: {} counter checks under one-number changes of +/-5%.\n", pairings.size());
}
} // namespace

bool GameLogicTests::CheckResult::Passed() const
{
  return std::ranges::none_of(verdicts, [](const std::string& _verdict) { return _verdict == "FAIL" || _verdict == "UNSURE"; });
}

CheckParts GameLogicTests::PartsFrom(const Outpost::Tuning& _tuning)
{
  CheckParts parts;
  for (const Outpost::HullTuning& hull : _tuning.hulls)
  {
    parts.hulls.push_back({hull.id, hull.name, static_cast<double>(hull.hitPoints), static_cast<double>(hull.armor),
                           hull.speedMetersPerSecond, static_cast<double>(hull.cost)});
  }
  for (const Outpost::DriveTuning& drive : _tuning.drives)
    parts.drives.push_back({drive.id, drive.name, drive.speedFactor, drive.hitPointsFactor, static_cast<double>(drive.cost)});
  for (const Outpost::WeaponTuning& weapon : _tuning.weapons)
  {
    parts.weapons.push_back({weapon.id, weapon.name, static_cast<double>(weapon.damage), weapon.fireIntervalSeconds, weapon.rangeMeters,
                             static_cast<double>(weapon.cost), weapon.splashRadiusMeters});
  }
  return parts;
}

CheckParts GameLogicTests::PartsThrough(const Outpost::Tuning& _tuning, std::int32_t _tier)
{
  const auto inTier = [&_tuning, _tier](auto _id)
  {
    return std::ranges::none_of(_tuning.research, [_id, _tier](const Outpost::ResearchTopicTuning& _topic)
                                { return _topic.tier > _tier && Unlocks(_topic, _id); });
  };
  CheckParts parts = PartsFrom(_tuning);
  std::erase_if(parts.hulls, [&](const CheckHull& _hull) { return !inTier(_hull.id); });
  std::erase_if(parts.drives, [&](const CheckDrive& _drive) { return !inTier(_drive.id); });
  std::erase_if(parts.weapons, [&](const CheckWeapon& _weapon) { return !inTier(_weapon.id); });
  return parts;
}

std::vector<CheckDesign> GameLogicTests::DesignsFrom(const Outpost::Tuning& _tuning, const CheckParts& _parts)
{
  std::vector<CheckDesign> designs;
  for (const CheckHull& hull : _parts.hulls)
  {
    for (const CheckDrive& drive : _parts.drives)
    {
      for (const CheckWeapon& weapon : _parts.weapons)
      {
        CheckDesign design;
        design.code = std::format("{}+{}+{}", ShortName(hull.name, false), ShortName(drive.name, false), ShortName(weapon.name, true));
        design.hull = hull.name;
        design.drive = drive.name;
        design.weapon = weapon.name;
        design.components = {hull.id, drive.id, weapon.id};
        // The game's stats, with every number the check may move taken from the parts. Turn rate, footprint and build
        // time stay the tuning data's.
        design.stats = Outpost::DesignStatsFor(_tuning, hull.id, drive.id, weapon.id);
        design.stats.movement.speedMetersPerSecond = static_cast<float>(hull.speedMetersPerSecond * drive.speedFactor);
        design.stats.hitPointsHundredths =
          static_cast<std::int32_t>(std::llround(hull.hitPoints * drive.hitPointsFactor * Outpost::HUNDREDTHS));
        design.stats.armorHundredths = static_cast<std::int32_t>(std::llround(hull.armor * Outpost::HUNDREDTHS));
        design.cost = hull.cost + drive.cost + weapon.cost;
        design.stats.cost = static_cast<std::int32_t>(std::llround(design.cost));
        design.stats.damageHundredths = static_cast<std::int32_t>(std::llround(weapon.damage * Outpost::HUNDREDTHS));
        design.stats.fireIntervalSeconds = weapon.fireIntervalSeconds;
        design.stats.rangeMeters = static_cast<float>(weapon.rangeMeters);
        design.stats.splashRadiusMeters = static_cast<float>(weapon.splashRadiusMeters);
        designs.push_back(std::move(design));
      }
    }
  }
  return designs;
}

int GameLogicTests::Fight(const CheckDesign& _a, const CheckDesign& _b, double _budgetOre, FireMode _mode, std::uint32_t _battle,
                          std::uint32_t _battles, const std::optional<Outpost::SightTuning>& _fog)
{
  const double budget =
    _budgetOre * (1.0 - BUDGET_SPREAD + (2.0 * BUDGET_SPREAD * (static_cast<double>(_battle % _battles) + 0.5) / _battles));
  const std::array<const CheckDesign*, 2> sides{&_a, &_b};

  Outpost::Simulation simulation(SeedFor(_a, _b, _budgetOre, _mode, _battle), TICKS_PER_SECOND);
  simulation.PlaceMap({.sizeMeters = ARENA_METERS, .minimumGapMeters = 60.0f, .starts = {}, .oreAsteroids = {}, .asteroidFields = {}});
  simulation.SetTargetRule(_mode == FireMode::Spread ? Outpost::TargetRule::Random : Outpost::TargetRule::Weakest);
  if (_fog.has_value())
  {
    // Fog needs only the sight of the tuning data; the designs carry their own numbers.
    Outpost::Tuning sightOnly;
    sightOnly.sight = *_fog;
    simulation.UseTuning(sightOnly);
    simulation.AddPlayer(FIRST, 0);
    simulation.AddPlayer(SECOND, 0);
    simulation.UseFog();
  }

  // Each side a square-ish grid, front row first, facing the other across the gap. The second side is the first turned
  // half a turn about the middle, as the map's starts are (design §4): mirrored instead, a partial last row would sit on
  // the same flank of both, and the formation, which is the same under a turn but not under a reflection, gives the
  // second side every battle at 9,000 Ore.
  std::array<std::vector<Outpost::EntityId>, 2> ships;
  std::array<Outpost::PlanePosition, 2> centers{};
  for (size_t side = 0; side < 2; ++side)
  {
    const CheckDesign& design = *sides[side];
    const auto count = static_cast<size_t>(std::floor((budget / design.cost) + 1e-9));
    if (count == 0)
      throw Neuron::Exception(std::format("{} Ore does not buy one {}.", budget, design.code));
    const Outpost::PlayerId owner = side == 0 ? FIRST : SECOND;
    const Outpost::DesignId id = simulation.SaveDesign(owner, design.code, design.components, design.stats);
    const float facing = side == 0 ? 1.0f : -1.0f;
    const float spacing = (2.0f * design.stats.movement.radiusMeters) + GRID_MARGIN_METERS;
    const auto columns = static_cast<size_t>(std::ceil(std::sqrt(static_cast<float>(count))));
    const size_t rows = (count + columns - 1) / columns;
    for (size_t i = 0; i < count; ++i)
    {
      const size_t row = i / columns;
      const size_t column = i % columns;
      const float back = static_cast<float>(row) * spacing;
      const float across = (static_cast<float>(column) - (static_cast<float>(columns - 1) / 2.0f)) * spacing;
      const Outpost::PlanePosition position{.xMeters = -facing * ((START_GAP_METERS / 2.0f) + back), .zMeters = -facing * across};
      ships[side].push_back(simulation.SpawnShip(owner, id, position, side == 0 ? 0.0f : std::numbers::pi_v<float>));
    }
    centers[side] = {.xMeters = -facing * ((START_GAP_METERS / 2.0f) + (static_cast<float>(rows - 1) * spacing / 2.0f)), .zMeters = 0.0f};
  }

  // Both sides attack-move on the other's center: each closes until its own weapon reaches, then stands and fires.
  (void)simulation.Tick({{.player = FIRST, .order = Outpost::AttackMoveCommand{.ships = ships[0], .destination = centers[1]}},
                         {.player = SECOND, .order = Outpost::AttackMoveCommand{.ships = ships[1], .destination = centers[0]}}});
  // An attack-move ends where it was sent. Once a second each side sends any ship that has stopped there out of range
  // after what is left of the enemy, so that, as in the model, a battle ends only when one side is gone.
  const auto limit = static_cast<std::uint64_t>(TIME_LIMIT_SECONDS * TICKS_PER_SECOND);
  std::array<size_t, 2> alive{};
  while (true)
  {
    alive = {0, 0};
    std::array<std::vector<Outpost::EntityId>, 2> idle;
    std::array<Outpost::PlanePosition, 2> sum{};
    for (const Outpost::Entity& entity : simulation.Entities())
    {
      if (entity.kind != Outpost::EntityKind::Ship)
        continue;
      const size_t side = entity.owner == FIRST ? 0 : 1;
      ++alive[side];
      sum[side].xMeters += entity.position.xMeters;
      sum[side].zMeters += entity.position.zMeters;
      if (entity.order == Outpost::ShipOrder::None && !entity.target.IsValid())
        idle[side].push_back(entity.id);
    }
    if (alive[0] == 0 || alive[1] == 0 || simulation.CurrentTick() >= limit)
      break;
    std::vector<Outpost::Command> commands;
    if (simulation.CurrentTick() % TICKS_PER_SECOND == 0)
    {
      for (size_t side = 0; side < 2; ++side)
      {
        const size_t enemy = 1 - side;
        const Outpost::PlanePosition enemyCenter{.xMeters = sum[enemy].xMeters / static_cast<float>(alive[enemy]),
                                                 .zMeters = sum[enemy].zMeters / static_cast<float>(alive[enemy])};
        if (!idle[side].empty())
        {
          commands.push_back({.player = side == 0 ? FIRST : SECOND,
                              .order = Outpost::AttackMoveCommand{.ships = std::move(idle[side]), .destination = enemyCenter}});
        }
      }
    }
    (void)simulation.Tick(commands);
  }
  return (alive[0] > 0 ? 1 : 0) - (alive[1] > 0 ? 1 : 0);
}

GameLogicTests::CheckResult GameLogicTests::RunQ2Check(const Outpost::Tuning& _tuning, const CheckOptions& _options)
{
  // Tier 1's components, the MVP's; the later tiers get their stages with task 10.3 (Phase 1 design §7).
  const CheckParts parts = PartsThrough(_tuning, 1);
  const CheckParts starting = Researched(parts, _tuning, {});
  const std::vector<CheckDesign> every = DesignsFrom(_tuning, parts);
  const std::vector<CheckDesign> early = DesignsFrom(_tuning, starting);

  CheckResult result;
  std::string& report = result.report;
  report += std::format("{} designs, {} battles per pairing (up to {} when a verdict is uncertain), a {} Hz tick.\n", every.size(),
                        _options.battles, _options.maxBattles, TICKS_PER_SECOND);

  Failures failures;
  std::set<Leg> legs;
  RunStage("Every component", every, parts, _options.budgets, _options, failures, legs, report);
  RunStage("Starting components", early, starting, _options.earlyBudgets, _options, failures, legs, report);
  RunResearchCheck(_tuning, parts, _options, failures, report);
  if (!_options.quick)
    RunRobustness(_tuning, parts, legs, _options, failures, report);

  report += "\nQ2 check (design §3):\n";
  const std::array<std::pair<std::string, std::string>, 4> criteria{{
    {"a", "every design has a counter that wins at least 80%"},
    {"b", "the designs worth building use every hull, drive and weapon over each stage's budgets"},
    {"c", "no counter stops winning when one number moves 5%"},
    {"d", "no research topic leaves a design without an answer that wins half the time"},
  }};
  for (size_t k = 0; k < criteria.size(); ++k)
  {
    const auto& [key, text] = criteria[k];
    std::string& verdict = result.verdicts[k];
    if (key == "c" && _options.quick)
      verdict = "NOT RUN";
    else if (!failures.lines[key].empty())
      verdict = "FAIL";
    else if (!failures.lines[key + "?"].empty())
      verdict = "UNSURE";
    else
      verdict = "PASS";
    report += std::format("  ({}) {}: {}\n", key, text, verdict);
    for (const std::string& line : failures.lines[key])
      report += std::format("      {}\n", line);
    for (const std::string& line : failures.lines[key + "?"])
      report += std::format("      unsure: {}\n", line);
  }
  return result;
}