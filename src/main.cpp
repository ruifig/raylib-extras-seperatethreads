/*******************************************************************************************
*
*   raylib example - One thread for raylib, and separate threads for game logic, physics, etc.
*
*   IMPORTANT:
*   - This sample requires C++20 and Raylib to be compile with  SUPPORT_CUSTOM_FRAME_CONTROL.
*   - The sample is kept as simple as possible.  A fully fledged solution will have to deal
*     with how to manage assets while they are still needed, and possibly other things, such
*     being able to push more complex operations to the queues.
*   - Raylib's API is single threaded, but with proper care, some systems can be called
*     from different threads. Do not use this in your game unless you know what you are doing
*
*   The purpose of this sample is to demonstrate a possible way to have a thread dedicated
*   for raylib, and another thread for game logic, physics, etc.
*   Using this approach, the game logic thread has thefore more available compute power to
*   dedicate to game logic.
* 
*   The core idea is to have two working sets of render command queues.
*   While the raylib thread processes one render queue, the game logic thread can add commands
*   to the other queue.
*   At the end of the frame, all threads synchronize, the queues are swapped, and a new
*   frame starts.
*
*   This example has been created using raylib 5.5 (www.raylib.com)
*   raylib is licensed under an unmodified zlib/libpng license (View raylib.h for details)
*
*   Copyright (c) 2025 Rui Figueira (https://github.com/ruifig)
*
********************************************************************************************/

#include "Common.h"
#include "FrameThread.h"
#include "RenderQueue.h"
#include "FPSCalculator.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <thread>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <random>

using namespace std::literals::chrono_literals;

//
// This defines the number of threads that we'll be synchronize. This sample has the following
// - Raylib : The thread where all the rendering happens, and Raylib's internals are updated
//   (e.g PollInputEvents is called)
// - GameLogic - The thread where the game logic happens. Some of Raylib's APIs can be called
//   from this thread, but exercise proper care by checking what is allowed or not.
// - Physics - Where game physics can be process. For this sample we don't really have any
//   physics, so it just does a sleep to fake some work.
//
#define NUM_THREADS 3

//
// Do some checks to see if the development environment has all we need
//
#if !defined(SUPPORT_CUSTOM_FRAME_CONTROL)
    #error This sample requires Raylib to be compiled with SUPPORT_CUSTOM_FRAME_CONTROL
#endif

Camera3D camera = {};

FrameThreadControl thControl(NUM_THREADS);

//
// Thread for physics
//
class PhysicsThread : public FrameThread
{
public:
    PhysicsThread(FrameThreadControl& control)
        : FrameThread(control, "Physics")
    {
    }

protected:
    void Update() override
    {
        // Since we don't really have Physics in this sample, we just fake some work with a sleep
        std::this_thread::sleep_for(5ms);
    }
};

PhysicsThread physicsTh(thControl);
float renderAvgWorkTimeMs = 0;

//
// Thread for the gameplay logic.
//
class GameLogicThread : public FrameThread
{
public:
    GameLogicThread(FrameThreadControl& control)
        : FrameThread(control, "GameLogic")
        , Rdgen(std::random_device()())
    {
    }

protected:

    // Generate a random integer number in the [from, to] range
    int GenRd(int from, int to)
    {
        std::uniform_int_distribution distrib(from, to);
        return distrib(Rdgen);
    }

    // Generate a random float number in the [from, to] range
    float GenRd(float from, float to)
    {
        std::uniform_real_distribution<float> distrib(from, to);
        return distrib(Rdgen);
    }

    // Generates a random color (with alpha set to 255)
    Color GenRdColor()
    {
        return {
            static_cast<uint8_t>(GenRd(0, 255)),
            static_cast<uint8_t>(GenRd(0, 255)),
            static_cast<uint8_t>(GenRd(0, 255)),
            255
            };
    }
    
    // Add `count` random cubes
    void AddCube(int count)
    {
        while(count--)
        {
            Cubes.emplace_back();
            Cubes.back().RotationSpeed = GenRd(0.02f, 2.0f);
            Cubes.back().CubeColor = GenRdColor();
            Cubes.back().WireColor = GenRdColor();
            Cubes.back().Width =  GenRd(0.05f, 2.0f);
            Cubes.back().Height = GenRd(0.05f, 2.0f);
            Cubes.back().Length = GenRd(0.05f, 2.0f);
            Cubes.back().Position = {GenRd(-100.0f, 100.0f), GenRd(-100.0f, 100.0f), GenRd(-500.0f, 80.0f)};
            Cubes.back().RotationAxis = Vector3Normalize({GenRd(-1.f, 1.f), GenRd(-1.f, 1.f), GenRd(-1.f, 1.f)});
        }
    }

    void OnStart()
    {
        AddCube(5000);
    }

    void Update() override
    {
        FpsCalc.Tick(Control.DeltaSeconds);

        // Process the cubes
        for(Cube& cube : Cubes)
        {
            cube.RotationDegrees += Control.DeltaSeconds * 360 * cube.RotationSpeed;
            RenderQueue::DrawCubeEx(cube.Position, cube.RotationDegrees, cube.RotationAxis, cube.Width, cube.Height, cube.Height, cube.CubeColor, cube.WireColor);
        }

        constexpr int fontSize = 20;
        auto Line = [&](int l) { return l * fontSize; };

        RenderQueue::DrawRectangle(0, 0, fontSize * 30, 6 * fontSize, {32, 32, 32, 200});
        RenderQueue::DrawText(TextFormat("FPS: %d", FpsCalc.GetFps()), 0, Line(0), fontSize, RED);
        RenderQueue::DrawText(TextFormat("GameLogic frametime: %4.2f ms", GetAvgWorkTimeMs()), 0, Line(1), fontSize, RED);
        RenderQueue::DrawText(TextFormat("Physics frametime: %4.2f ms", physicsTh.GetAvgWorkTimeMs()), 0, Line(2), fontSize, RED);
        RenderQueue::DrawText(TextFormat("Render frametime: %4.2f ms", renderAvgWorkTimeMs), 0, Line(3), fontSize, RED);
        RenderQueue::DrawText(TextFormat("Number of cubes: %d", static_cast<int>(Cubes.size())), 0, Line(4), fontSize, RED);
        RenderQueue::DrawText("Press [ or ] change the number of cubes", 0, Line(5), 20, BROWN);

        constexpr int numCubes = 100;
        if (IsKeyPressed(KEY_LEFT_BRACKET))
        {
            int todo = std::min(numCubes, static_cast<int>(Cubes.size()));
            while(todo--)
            {
                Cubes.pop_back();
            }
        }
        else if (IsKeyPressed(KEY_RIGHT_BRACKET))
        {
            AddCube(numCubes);
        }

        DOLOG("%s: Work done\n", Name.c_str());
    }

    struct Cube
    {
        float RotationSpeed; // Rotation speed in full revolutions per second
        float RotationDegrees; // Rotation in degrees
        Vector3 RotationAxis;
        Vector3 Position;
        float Width;
        float Height;
        float Length;
        Color CubeColor;
        Color WireColor;
    };
    std::vector<Cube> Cubes;
    FPSCalculator<> FpsCalc;
    std::mt19937 Rdgen;
};

GameLogicThread gameLogicTh(thControl);

int main(int argc, char* argv[])
{
    // Initialization
    //--------------------------------------------------------------------------------------
    int screenWidth = 1600;
    int screenHeight = 900;

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE /* | FLAG_VSYNC_HINT */);
    InitWindow(screenWidth, screenHeight, "raylibExtras SeparateThreads example");

    camera.position = {0.0f, 0.0f, 100.0f};  // Camera position
    camera.target = {0.0f, 0.0f, 0.0f};       // Camera looking at point
    camera.up = {0.0f, 1.0f, 0.0f};           // Camera up vector (rotation towards target)
    camera.fovy = 45.0f;                      // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;   // Camera projection type

    RenderQueue renderQueue;
    // Abusing the FPSCalculator to calculate how long the rendering takes.
    FPSCalculator renderWorkCalc;

    gameLogicTh.Start();
    physicsTh.Start();

    uint32_t frameNum = 0;

    //
    // Main game loop
    // This loop behaves very simular to the worker threads, with the extra step to prepare for the next frame
    //
    while (!thControl.ShouldFinish)  // Detect window close button or ESC key
    {
        {
            DOLOG("Starting frame %u\n", frameNum);
            DOLOG("%s: Arrived at frameStartBarrier.\n", "MainTread");
            thControl.FrameStartBarrier.arrive_and_wait();
        }

        //
        // The "frame work" for the main thread is to render all the commands and update
        // Raylib's internals
        //
        {
            auto start = std::chrono::high_resolution_clock::now();
            BeginDrawing();
                ClearBackground(WHITE);
                // Execute the render commands
                RenderQueue::Get().Render();
            EndDrawing();
            SwapScreenBuffer();

            DOLOG("%s: Work done\n", "MainThread");

            if (WindowShouldClose())
                thControl.ShouldFinish = true;

            renderWorkCalc.Tick(std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - start).count());;
            renderAvgWorkTimeMs = renderWorkCalc.GetAvgMs();
        }

        // Signal that we are finished with our work.
        // This waits for all other threads to finish, so that then we can prepare for the next
        // frame.
        DOLOG("%s: Arrived at frameEndBarrier.\n", "MainThread");
        thControl.FrameEndBarrier.arrive_and_wait();
        
        // At this point all threads are done with their work for the frame and are waiting for this thread to
        // kickstart the next frame. In this step, we update whatever Raylib internals we need, such as polling input.
        {
            RenderQueue::Get().SwapQueues();
            PollInputEvents();
            ++frameNum;
            auto now = std::chrono::high_resolution_clock::now();
            thControl.DeltaSeconds = std::chrono::duration<float>(now - thControl.FrameStartTime).count();
            thControl.FrameStartTime = now;
        }
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();  // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

