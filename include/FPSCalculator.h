/*******************************************************************************************
*
*   Utility class to calculate FPS, since if Raylib is compiled with SUPPORT_CUSTOM_FRAME_CONTROL,
*   DrawFPS doesn't work.
*
*   This example has been created using raylib 5.5 (www.raylib.com)
*   raylib is licensed under an unmodified zlib/libpng license (View raylib.h for details)
*
*   Copyright (c) 2025 Rui Figueira (https://github.com/ruifig)
*
********************************************************************************************/

#pragma once

#include <chrono>

struct FPSCalculator
{
    constexpr static inline int MaxSamples = 30;
    int tickIndex=0;
    // Instead of keep hold of the floats, we use fixed point calculations, to avoid accumulating errors
    std::chrono::microseconds tickSum = {};
    std::chrono::microseconds tickList[MaxSamples] = {};
    float fps = 0;
    float avgMsPerFrame = 0;
    uint64_t numTicks = 0;
    double variance = 0;

    void tick(float deltaSeconds)
    {
        numTicks++;
        std::chrono::microseconds deltaMicroseconds(int32_t(deltaSeconds * 1000 * 1000));

        tickSum -= tickList[tickIndex];			 /* subtract value falling off */
        tickSum += deltaMicroseconds;			 /* add new value */
        tickList[tickIndex] = deltaMicroseconds; /* save new value so it can be subtracted later */
        if (++tickIndex == MaxSamples)			 /* inc buffer index */
        {
            tickIndex = 0;
        }

        avgMsPerFrame = float(static_cast<double>(tickSum.count()) / (MaxSamples * 1000));
        fps = 1000.0f / avgMsPerFrame;

        calculateVariance();
    }

    // Calculate Sample Variance : https://www.calculatorsoup.com/calculators/statistics/variance-calculator.php
    void calculateVariance()
    {
        double meanMs = static_cast<double>(tickSum.count()) / (MaxSamples * 1000);

        double tmp = 0;
        for (const std::chrono::microseconds& t : tickList)
        {
            double ms = static_cast<double>(t.count()) / 1000;
            tmp += std::pow(ms - meanMs, 2);
        }

        variance = tmp / (MaxSamples - 1);
    }

    bool isValid() const
    {
        return numTicks >= MaxSamples;
    }
};

