#pragma once
/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2024 Melin Software HB

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License fro more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu
    Eksoppsvägen 16, SE-75646 UPPSALA, Sweden

************************************************************************/

constexpr int timeUnitsPerSecond = 10;

constexpr int timeConstHour = 3600 * timeUnitsPerSecond;
constexpr int timeConstMinute = 60 * timeUnitsPerSecond;
constexpr int timeConstSecond = timeUnitsPerSecond;

// For second based times
constexpr int timeConstSecPerMin = 60;
constexpr int timeConstMinPerHour = 60;
constexpr int timeConstSecPerHour = timeConstSecPerMin * timeConstMinPerHour;