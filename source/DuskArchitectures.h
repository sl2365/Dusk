#pragma once

#include <array>
#include <cstddef>

namespace dusk::architecture
{
enum class Id : int
{
    cleanSvf = 0,
    ladder,
    ota
};

struct Descriptor
{
    Id id;
    const char* displayName;
    const char* designerName;
    const char* summaryLineOne;
    const char* summaryLineTwo;
    const char* summaryLineThree;
};

inline constexpr std::array<Descriptor, 3> descriptors
{{
    {
        Id::cleanSvf,
        "Clean SVF",
        "CLEAN SVF DESIGNER",
        "TPT STATE-VARIABLE CORE",
        "LPF  BPF  HPF  NOTCH  PEAK",
        "8 CONTEXT DESIGNER CONTROLS"
    },
    {
        Id::ladder,
        "Ladder",
        "LADDER DESIGNER",
        "NONLINEAR CASCADED-POLE CORE",
        "LPF  BPF  HPF  NOTCH  PEAK",
        "STAGE SAT + FB SAT + RES COMP"
    },
    {
        Id::ota,
        "OTA",
        "OTA DESIGNER",
        "NONLINEAR TRANSCONDUCTANCE CELLS",
        "LPF  BPF  HPF  NOTCH  PEAK",
        "CELL DRIVE + FB DAMP + RES COMP"
    }
}};

inline int normaliseChoice(int choice) noexcept
{
    return choice >= 0 && choice < static_cast<int>(descriptors.size())
        ? choice : 0;
}

inline const Descriptor& descriptorForChoice(int choice) noexcept
{
    return descriptors[static_cast<std::size_t>(normaliseChoice(choice))];
}
}
