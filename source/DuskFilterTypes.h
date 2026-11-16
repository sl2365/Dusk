#pragma once

#include <array>
#include <cmath>

namespace dusk::filtertype
{
inline constexpr int count = 9;
inline constexpr int equaliserFirstChoice = 5;

inline bool usesGain(int choice) noexcept
{
    return choice >= equaliserFirstChoice && choice < count;
}

template <typename Value>
std::array<Value, count> outputs(
    const Value& input, const Value& low, const Value& band,
    const Value& high, float gainDb)
{
    const float gain = std::pow(10.0f, gainDb / 20.0f);
    const float lowTilt = std::pow(10.0f, -gainDb / 40.0f);
    const float highTilt = std::pow(10.0f, gainDb / 40.0f);
    return {
        low,
        band,
        high,
        low + high,
        low - high,
        input + Value(gain - 1.0f) * low,
        input + Value(gain - 1.0f) * high,
        input + Value(gain - 1.0f) * band,
        input + Value(lowTilt - 1.0f) * low
              + Value(highTilt - 1.0f) * high
    };
}

template <typename Value>
Value select(const std::array<Value, count>& values,
             float position) noexcept
{
    const float safePosition = juce::jlimit(
        0.0f, static_cast<float>(count - 1), position);
    const int left = juce::jlimit(
        0, count - 1, static_cast<int>(std::floor(safePosition)));
    const int right = juce::jmin(count - 1, left + 1);
    const float fraction = safePosition - static_cast<float>(left);
    return values[static_cast<size_t>(left)]
        + (values[static_cast<size_t>(right)]
            - values[static_cast<size_t>(left)]) * Value(fraction);
}
}
