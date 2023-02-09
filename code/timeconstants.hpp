#pragma once
constexpr int timeUnitsPerSecond = 10;

constexpr int timeConstHour = 3600 * timeUnitsPerSecond;
constexpr int timeConstMinute = 60 * timeUnitsPerSecond;
constexpr int timeConstSecond = timeUnitsPerSecond;

// For second based times
constexpr int timeConstSecPerMin = 60;
constexpr int timeConstMinPerHour = 60;
constexpr int timeConstSecPerHour = timeConstSecPerMin * timeConstMinPerHour;