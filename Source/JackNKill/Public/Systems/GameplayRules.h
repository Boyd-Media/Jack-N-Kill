#pragma once

// Engine-independent production rules, shared by Unreal gameplay and native tests.
#include <algorithm>
#include <cmath>

namespace JNK::Rules
{
    inline constexpr int MaxInventoryStacks = 64;
    inline constexpr int MaxItemIdBytes = 64;
    inline constexpr int MaxItemQuantity = 10000;
    inline constexpr int MaxDoorRecords = 16384;
    inline constexpr int MaxHouseRecords = 4096;
    inline constexpr int MaxSavePayloadBytes = 512 * 1024;
    inline constexpr int MaxSerializedSaveBytes = 1000000;
    inline constexpr int WorstCaseSavePayloadBytes = 8 + 3 * 4 + MaxDoorRecords * 17
        + MaxHouseRecords * 16 + MaxInventoryStacks * (1 + MaxItemIdBytes + 2) + 7 * 4;
    static_assert(WorstCaseSavePayloadBytes <= MaxSavePayloadBytes);

    inline bool CanMutateDuringLoad(bool Loading, bool Restoring, bool ApplyingPlayerRestore)
    {
        return !Loading || (Restoring && ApplyingPlayerRestore);
    }
    inline bool IsWindowLive(double Now, double Start, double Duration)
    {
        const double End = Start + Duration;
        return std::isfinite(Now) && std::isfinite(Start) && std::isfinite(Duration)
            && std::isfinite(End) && Duration > 0.0 && Now >= Start && Now < End;
    }

    inline double SmoothingAlpha(double Days, double HalfLife)
    {
        if (!std::isfinite(Days) || !std::isfinite(HalfLife) || Days <= 0.0 || HalfLife <= 0.0)
        {
            return 0.0;
        }
        return -std::expm1(-0.6931471805599453 * Days / HalfLife);
    }

    inline bool CanReserveAgents(int Occupied, int Humans, int Aliens, int Count,
        bool IsAlien, int AlienLimit)
    {
        if (Occupied < 0 || Humans < 0 || Aliens < 0 || Humans > 8 || Aliens > 8 || Humans + Aliens != Occupied
            || Occupied > 8 || Count < 1 || Count > 8 || AlienLimit < 1 || AlienLimit > 8)
        {
            return false;
        }
        return Count <= 8 - Occupied
            && (IsAlien ? Count <= AlienLimit - Aliens : Count <= 7 - Humans);
    }

    inline double Stress(double Health, double Ammo, double SecondsSinceEncounter)
    {
        if (!std::isfinite(Health) || !std::isfinite(Ammo) || !std::isfinite(SecondsSinceEncounter))
        {
            return 1.0;
        }
        return 0.5 * (1.0 - std::clamp(Health, 0.0, 1.0))
            + 0.3 * (1.0 - std::clamp(Ammo, 0.0, 1.0))
            + 0.2 * std::exp(-std::max(SecondsSinceEncounter, 0.0) / 30.0);
    }

    inline double SevereStormFraction(double Intensity)
    {
        return std::isfinite(Intensity) ? std::clamp((Intensity - 0.7) / 0.3, 0.0, 1.0) : 0.0;
    }

    inline double SprintMultiplier(double Fat, double Penalty)
    {
        return std::isfinite(Fat) && Fat > 0.7
            ? (std::isfinite(Penalty) ? std::clamp(Penalty, 0.1, 1.0) : 0.75) : 1.0;
    }

    inline double MeleeMultiplier(double Muscular, double Bonus)
    {
        return std::isfinite(Muscular) && Muscular > 0.7
            ? (std::isfinite(Bonus) ? std::clamp(Bonus, 1.0, 3.0) : 1.25) : 1.0;
    }
}
