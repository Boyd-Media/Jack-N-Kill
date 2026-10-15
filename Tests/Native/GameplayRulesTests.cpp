#include "Systems/GameplayRules.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{
    int Checks = 0;
    void Require(bool Condition, const char* Message)
    {
        ++Checks;
        if (!Condition) throw std::runtime_error(Message);
    }
    bool Near(double A, double B) { return std::abs(A - B) < 1e-10; }

    void CombatWindows()
    {
        using JNK::Rules::IsWindowLive;
        Require(!IsWindowLive(99.9, 100, 0.1), "No hit before Active");
        Require(IsWindowLive(100, 100, 0.1), "Opening pose is live");
        Require(IsWindowLive(100.05, 100, 0.1), "Mid-window pose is live");
        Require(!IsWindowLive(100.1, 100, 0.1), "Exact deadline is not live");
        Require(!IsWindowLive(101, 100, 0.1), "Hitch cannot cause late damage");
        Require(!IsWindowLive(100, 100, 0), "Zero duration is not live");
        Require(!IsWindowLive(100, 100, -1), "Negative duration rejected");
        Require(!IsWindowLive(std::numeric_limits<double>::quiet_NaN(), 0, 1), "NaN time rejected");
        Require(!IsWindowLive(0, 0, std::numeric_limits<double>::infinity()), "Infinite duration rejected");
        Require(!IsWindowLive(1, std::numeric_limits<double>::max(), std::numeric_limits<double>::max()), "Overflow rejected");
    }

    void Admission()
    {
        using JNK::Rules::CanReserveAgents;
        Require(CanReserveAgents(6, 5, 1, 2, false, 1), "Two-member squad fits");
        Require(!CanReserveAgents(7, 6, 1, 2, false, 1), "Squad rejected atomically when only one slot remains");
        Require(!CanReserveAgents(7, 7, 0, 1, false, 1), "Human cannot take reserved Alien slot");
        Require(CanReserveAgents(7, 7, 0, 1, true, 1), "Alien can use eighth slot");
        Require(!CanReserveAgents(1, 0, 1, 1, true, 1), "Default heavy-agent limit enforced");
        Require(!CanReserveAgents(0, 0, 0, std::numeric_limits<int>::max(), false, 1), "Huge count rejected");
        Require(!CanReserveAgents(0, std::numeric_limits<int>::max(), 1, 1, false, 1), "Invalid counters rejected without overflow");
        Require(!CanReserveAgents(0, 0, 0, -1, false, 1), "Negative request rejected");
        for (int Occupied = 0; Occupied <= 8; ++Occupied)
        for (int Humans = 0; Humans <= Occupied; ++Humans)
        for (int Count = 0; Count <= 9; ++Count)
        for (int Limit = 1; Limit <= 2; ++Limit)
        for (bool Alien : {false, true})
        {
            const int Aliens = Occupied - Humans;
            if (CanReserveAgents(Occupied, Humans, Aliens, Count, Alien, Limit))
            {
                Require(Count > 0 && Occupied + Count <= 8, "Every admitted batch respects total budget");
                Require(Alien ? Aliens + Count <= Limit : Humans + Count <= 7,
                    "Every admitted batch respects its faction budget");
            }
        }
    }

    void LoadIsolation()
    {
        using JNK::Rules::CanMutateDuringLoad;
        for (bool Loading : {false, true})
        for (bool Restoring : {false, true})
        for (bool Applying : {false, true})
        {
            const bool Allowed = CanMutateDuringLoad(Loading, Restoring, Applying);
            if (Loading && !Restoring) Require(!Allowed, "Food/loot/clock callbacks cannot race a load");
            if (Loading && Restoring && Applying) Require(Allowed, "Manager can apply the validated snapshot");
            if (Loading && Restoring && !Applying) Require(!Allowed, "External restore cannot bypass loading gate");
            if (!Loading) Require(Allowed, "Normal gameplay remains mutable outside load");
        }
    }

    void Metabolism()
    {
        using namespace JNK::Rules;
        Require(Near(SmoothingAlpha(2, 2), 0.5), "Half-life halves the remaining difference");
        for (int Steps : {1, 2, 30, 1440})
        {
            const double Alpha = SmoothingAlpha(1.0 / Steps, 2);
            double Weight = 0.2;
            for (int Step = 0; Step < Steps; ++Step) Weight += (0.8 - Weight) * Alpha;
            const double Once = 0.2 + (0.8 - 0.2) * SmoothingAlpha(1, 2);
            Require(Near(Weight, Once), "Clock cadence does not change smoothing");
        }
        Require(SmoothingAlpha(-1, 2) == 0, "Negative time cannot reverse morphs");
        Require(SmoothingAlpha(1, 0) == 0, "Zero half-life is bounded");
        Require(SprintMultiplier(0.7, 0.75) == 1, "Exactly 0.7 has no sprint penalty");
        Require(Near(SprintMultiplier(0.71, 0.75) * 650, 487.5), "Sprint penalty uses base speed");
        Require(MeleeMultiplier(0.7, 1.25) == 1, "Exactly 0.7 has no damage bonus");
        Require(Near(MeleeMultiplier(0.71, 1.25) * 20, 25), "Muscular damage bonus");
    }

    void TensionAndWeather()
    {
        using namespace JNK::Rules;
        Require(Near(Stress(1, 1, 0), 0.2), "Recent combat contributes stress");
        Require(Stress(1, 1, 60) < Stress(1, 1, 10), "Rest lowers stress");
        Require(Stress(0.2, 0.1, 10) > Stress(1, 1, 10), "Scarcity increases stress");
        Require(Stress(std::numeric_limits<double>::quiet_NaN(), 1, 0) == 1, "Invalid telemetry fails closed");
        for (int I = 0; I <= 100; ++I)
        {
            const double Storm = SevereStormFraction(I / 100.0);
            const double Radius = 1 - 0.5 * Storm;
            const double Aggression = 1 + 0.5 * Storm;
            Require(Radius >= 0.5 && Radius <= 1 && Aggression >= 1 && Aggression <= 1.5, "Weather gameplay remains bounded");
            if (I < 100) Require(Storm <= SevereStormFraction((I + 1) / 100.0), "Increasing storm has monotonic effects");
        }
        Require(SevereStormFraction(0.7) == 0, "Light weather has no severe modifier");
        Require(Near(SevereStormFraction(1), 1), "Full storm reaches intended modifiers");
        Require(WorstCaseSavePayloadBytes < MaxSavePayloadBytes, "Maximum allowed records fit compact payload");
        Require(MaxSerializedSaveBytes < 5000000, "Save cap meets requested five-megabyte limit");
    }
}

int main()
{
    try
    {
        CombatWindows(); Admission(); LoadIsolation(); Metabolism(); TensionAndWeather();
        std::cout << "PASS: " << Checks << " production-rule checks across combat, admission, load isolation, metabolism, tension, weather, save budgets\n";
        return 0;
    }
    catch (const std::exception& Error)
    {
        std::cerr << "FAIL: " << Error.what() << '\n';
        return 1;
    }
}
