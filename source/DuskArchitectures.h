#pragma once

#include <array>
#include <cstddef>

namespace dusk::architecture
{
enum class Id : int
{
    cleanSvf = 0,
    ladder,
    ota,
    comb,
    formant,
    phaser
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

inline constexpr std::array<Descriptor, 6> descriptors
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
    },
    {
        Id::comb,
        "Comb",
        "COMB DESIGNER",
        "FRACTIONAL-DELAY FILTER CORE",
        "FEEDBACK  FEEDFORWARD  ALL-PASS",
        "6 CONTEXT DESIGNER CONTROLS"
    },
    {
        Id::formant,
        "Formant",
        "FORMANT DESIGNER",
        "THREE-BAND VOCAL RESONATOR",
        "A  E  I  O  U  CONTINUOUS MORPH",
        "6 CONTEXT DESIGNER CONTROLS"
    },
    {
        Id::phaser,
        "Phaser",
        "PHASER DESIGNER",
        "MULTI-STAGE ALL-PASS CASCADE",
        "CONTINUOUS NOTCH DEPTH + FEEDBACK",
        "6 CONTEXT DESIGNER CONTROLS"
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
