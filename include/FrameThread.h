/*******************************************************************************************
*
*   Frame based thread.
*	This makes it easier to add N threads that should sync at the end of the frame.
* 
*   This example has been created using raylib 5.5 (www.raylib.com)
*   raylib is licensed under an unmodified zlib/libpng license (View raylib.h for details)
*
*   Copyright (c) 2025 Rui Figueira (https://github.com/ruifig)
*
********************************************************************************************/

#pragma once

#include "Common.h"
#include "FPSCalculator.h"

#include <thread>
#include <string_view>
#include <string>

#ifdef __has_cpp_attribute
    #if __has_include(<barrier>)
        #include <barrier>
    #else
        #error This sample requires support for std::barrier
    #endif
#else
    #error This sample requires C++20
#endif

/*!
 * Puts together something things that the threads need to use in order
 * to synchronize.
 */
struct FrameThreadControl
{
    /*!
     * \param numThreads
     *      Number of threads that will be synchronizing (including your main thread).
     *      **IMPORTANT** : This needs to match the number you will be using, otherwise your game will freeze.
     */
    explicit FrameThreadControl(int numThreads)
        : FrameStartBarrier(numThreads)
        , FrameEndBarrier(numThreads)
    {
    }

    std::atomic<bool> ShouldFinish = false;

    // Used by all threads to wait until every other thread finishes its frame work.
    std::barrier<> FrameEndBarrier;

    // Used by all threads to wait for the main thread to prepare the next frame.
    std::barrier<> FrameStartBarrier;

    // seconds from the last frame
    std::chrono::high_resolution_clock::time_point FrameStartTime;
    float DeltaSeconds = 0;
};


/*!
 * A thread wrapper to make it easier to add more threads to the frame synchronization.
 * New threads only have to provide the `Update` method, which gets call each frame.
 */
class FrameThread
{
public:

    /*
     * \param control
     *      Control struct to use for synchronization. The number of thread need to be correct.
     */
    explicit FrameThread(FrameThreadControl& control, std::string_view name)
        : Control(control)
        , Name(name)
    {
    }

    virtual ~FrameThread()
    {
        Th.join();
    }

    void Start()
    {
        Th = std::thread([this]()
        {
            OnStart();
            while (!Control.ShouldFinish)
            {
                // We can only start our work once all threads are ready to start (aka: arrive at the frameStart barrier)
                DOLOG("%s: Arrived at frameStartBarrier.\n", Name.c_str());
                Control.FrameStartBarrier.arrive_and_wait();

                // Do the work for the current frame
                auto start = std::chrono::high_resolution_clock::now();
                Update();
                DOLOG("%s: Work done\n", Name.c_str());
                WorkCalc.Tick(std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - start).count());;

                // We are done with our work, so now wait for all other threads to finish  (aka: arrive at the frameEnd barrier)
                DOLOG("%s: Arrived at frameEndBarrier.\n", Name.c_str());
                Control.FrameEndBarrier.arrive_and_wait();
            }

            OnEnd();
        });
    }

    /*!
     * Returns the average time (in ms) that the work is taking each frame for this thread.
     */
    float GetAvgWorkTimeMs() const
    {
        return WorkCalc.GetAvgMs();
    }

protected:

    /*!
     * Called when the thread starts
     */
    virtual void OnStart() {}

    /*!
     * Called when the thread finishes
     */
    virtual void OnEnd() {}

    /*!
     * Called each frame, to perform the thread's work.
     */
    virtual void Update() = 0;

    // Abusing the fps calculator to calculate how long the work takes
    FPSCalculator<> WorkCalc;

    FrameThreadControl& Control;
    std::string Name;

private:

    std::thread Th;
};

