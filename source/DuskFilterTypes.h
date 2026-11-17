#pragma once

#include <array>
#include <cmath>

namespace dusk::filtertype
{
inline constexpr int count = 9;
inline constexpr int equaliserFirstChoice = 5;

inline constexpr bool usesGain(int choice) noexcept
{
    return choice >= equaliserFirstChoice && choice < count;
}

struct Gains
{
    float gainMinusOne = 0.0f;
    float lowTiltMinusOne = 0.0f;
    float highTiltMinusOne = 0.0f;
};

inline Gains gainsForDecibels(float gainDb) noexcept
{
    // Gain is zero for the non-EQ filter types and for the default EQ
    // setting.  Avoid three comparatively expensive power functions in
    // that overwhelmingly common case.
    if (gainDb == 0.0f)
        return {};

    return {
        std::pow(10.0f, gainDb / 20.0f) - 1.0f,
        std::pow(10.0f, -gainDb / 40.0f) - 1.0f,
        std::pow(10.0f, gainDb / 40.0f) - 1.0f
    };
}

template <typename Value>
std::array<Value, count> outputs(
    const Value& input, const Value& low, const Value& band,
    const Value& high, const Gains& gains)
{
    return {
        low,
        band,
        high,
        low + high,
        low - high,
        input + Value(gains.gainMinusOne) * low,
        input + Value(gains.gainMinusOne) * high,
        input + Value(gains.gainMinusOne) * band,
        input + Value(gains.lowTiltMinusOne) * low
              + Value(gains.highTiltMinusOne) * high
    };
}

template <typename Value>
std::array<Value, count> outputs(
    const Value& input, const Value& low, const Value& band,
    const Value& high, float gainDb)
{
    return outputs(input, low, band, high, gainsForDecibels(gainDb));
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
