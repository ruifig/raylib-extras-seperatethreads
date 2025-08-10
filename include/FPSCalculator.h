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


/*!
 * Utility class to calculate average fps and frametime variance.
 * It can also obviously be used to average other units of work.
 */

template<int MaxSamples = 30, bool CalcVariance = false>
class FPSCalculator
{
public:

    /*!
     * Add another data point.
     */
    void Tick(float deltaSeconds)
    {
        NumTicks++;
        std::chrono::microseconds deltaMicroseconds(int32_t(deltaSeconds * 1000 * 1000));

        TickSum -= TickList[TickIndex];			 /* subtract value falling off */
        TickSum += deltaMicroseconds;			 /* add new value */
        TickList[TickIndex] = deltaMicroseconds; /* save new value so it can be subtracted later */
        if (++TickIndex == MaxSamples)			 /* inc buffer index */
        {
            TickIndex = 0;
        }

        AvgMsPerFrame = float(static_cast<double>(TickSum.count()) / (MaxSamples * 1000));
        Fps = 1000.0f / AvgMsPerFrame;

        if constexpr(CalcVariance)
        {
            CalculateVariance();
        }
    }

    int GetFps() const
    {
        return static_cast<int>(Fps + 0.5f);
    }

    /*!
     * Returns the average time per data point, in milliseconds
     */
    float GetAvgMs() const
    {
        return AvgMsPerFrame;
    }

    /*!
     * Returns the variance, in milliseconds
     */
    float GetVariance() const
    {
        return Variance;
    }

private:
    int TickIndex=0;
    // Instead of keep hold of the floats, we use fixed point calculations, to avoid accumulating errors
    std::chrono::microseconds TickSum = {};
    std::chrono::microseconds TickList[MaxSamples] = {};
    float Fps = 0;
    float AvgMsPerFrame = 0;
    uint64_t NumTicks = 0;
    double Variance = 0;

    // Calculate Sample Variance : https://www.calculatorsoup.com/calculators/statistics/variance-calculator.php
    void CalculateVariance()
    {
        double meanMs = static_cast<double>(TickSum.count()) / (MaxSamples * 1000);

        double tmp = 0;
        for (const std::chrono::microseconds& t : TickList)
        {
            double ms = static_cast<double>(t.count()) / 1000;
            tmp += std::pow(ms - meanMs, 2);
        }

        Variance = tmp / (MaxSamples - 1);
    }

};

