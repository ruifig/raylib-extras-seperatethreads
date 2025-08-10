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


#if defined(NDEBUG)
    #define DOLOG(...)
#else
    #define DOLOG(...) printf(##__VA_ARGS__)
#endif


//
// Do some checks to see if the development environment has all we need
//
#if !defined(SUPPORT_CUSTOM_FRAME_CONTROL)
    #error This sample requires Raylib to be compiled with SUPPORT_CUSTOM_FRAME_CONTROL
#endif

#ifdef __has_cpp_attribute
    #if __has_include(<barrier>)
        #include <barrier>
    #else
        #error This sample requires support for std::barrier
    #endif
#else
    #error This sample requires C++20
#endif

#pragma comment(lib, "raylib.lib")

Camera3D camera = {};

//
// Just putting all these in a struct so we polute the global namespace as little as possible
//
struct ThreadControls
{
    ThreadControls()
        : frameStartBarrier(NUM_THREADS)
        , frameEndBarrier(NUM_THREADS)
    {
    }

    std::atomic<bool> shouldFinish = false;

    // Used by all threads to wait until every other thread finishes its frame work.
    std::barrier<> frameEndBarrier;

    // Used by all threads to wait for the main thread to prepare the next frame.
    std::barrier<> frameStartBarrier;

    // seconds from the last frame
    std::chrono::high_resolution_clock::time_point frameStartTime;
    float deltaSeconds = 0;
} thCtrl;

//
// A thread wrapper to make it easier to add more threads to the frame synchronization.
// A worker thread only has to provide the `tick` method, which gets call each frame.
//
class WorkerThread
{
public:
    explicit WorkerThread(std::string_view name)
        : name(name)
    {
    }

    virtual ~WorkerThread()
    {
        th.join();
    }

    void start()
    {
        th = std::thread([this]()
        {
            onStarted();
            while (!thCtrl.shouldFinish)
            {
                // We can only start our work once all threads are ready to start (aka: arrive at the frameStart barrier)
                DOLOG("%s: Arrived at frameStartBarrier.\n", name.c_str());
                thCtrl.frameStartBarrier.arrive_and_wait();

                // Do the work for the current frame
                auto start = std::chrono::high_resolution_clock::now();
                tick();
                workCalc.tick(std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - start).count());;

                // We are done with our work, so now wait for all other threads to finish  (aka: arrive at the frameEnd barrier)
                DOLOG("%s: Arrived at frameEndBarrier.\n", name.c_str());
                thCtrl.frameEndBarrier.arrive_and_wait();
            }

            onEnded();
        });
    }

    float getAvgWorkTimeMs() const
    {
        return workCalc.avgMsPerFrame;
    }

protected:

    virtual void onStarted() {}
    virtual void onEnded() {}

    // Custom worker threads should implement this.
    // This is called one per game loop.
    virtual void tick() = 0;

    // Abusing the fps calculator to calculate how long the work takes
    FPSCalculator workCalc;

    std::string name;
private:

    std::thread th;
};

//
// Thread for physics
//
class PhysicsThread : public WorkerThread
{
public:
    PhysicsThread()
        : WorkerThread("Physics")
    {
    }

protected:
    void tick() override
    {
        DOLOG("%s: Work done\n", name.c_str());
        // Since we don't really have Physics in this sample, we just fake some work with a sleep
        std::this_thread::sleep_for(5ms);
    }
};

PhysicsThread physicsTh;
float renderAvgWorkTimeMs = 0;

//
// Thread for the gameplay logic.
//
class GameLogicThread : public WorkerThread
{
public:
    GameLogicThread()
        : WorkerThread("GameLogic")
        , rdgen(std::random_device()())
    {
    }

protected:

    // Generate a random integer number in the [from, to] range
    int genRd(int from, int to)
    {
        std::uniform_int_distribution distrib(from, to);
        return distrib(rdgen);
    }

    // Generate a random float number in the [from, to] range
    float genRd(float from, float to)
    {
        std::uniform_real_distribution<float> distrib(from, to);
        return distrib(rdgen);
    }

    // Generates a random color (with alpha set to 255)
    Color genRdColor()
    {
        return {
            static_cast<uint8_t>(genRd(0, 255)),
            static_cast<uint8_t>(genRd(0, 255)),
            static_cast<uint8_t>(genRd(0, 255)),
            255
            };
    }
    
    // Add `count` random cubes
    void addCube(int count)
    {
        while(count--)
        {
            cubes.emplace_back();
            cubes.back().rotationSpeed = genRd(0.02f, 2.0f);
            cubes.back().color = genRdColor();
            cubes.back().wcolor = genRdColor();
            cubes.back().width =  genRd(0.05f, 2.0f);
            cubes.back().height = genRd(0.05f, 2.0f);
            cubes.back().length = genRd(0.05f, 2.0f);
            cubes.back().position = {genRd(-100.0f, 100.0f), genRd(-100.0f, 100.0f), genRd(-500.0f, 80.0f)};
            cubes.back().rotationAxis = Vector3Normalize({genRd(-1.f, 1.f), genRd(-1.f, 1.f), genRd(-1.f, 1.f)});
        }
    }

    void onStarted()
    {
        Mesh cube = GenMeshCube(1,1,1);
        UploadMesh(&cube, false);
        cubeModel = LoadModelFromMesh(cube);

        addCube(10000);
    }

    void tick() override
    {
        fpsCalc.tick(thCtrl.deltaSeconds);

        // Process the cubes
        for(Cube& cube : cubes)
        {
            cube.rotationDegrees += thCtrl.deltaSeconds * 360 * cube.rotationSpeed;
            RenderQueue::drawCubeEx(cube.position, cube.rotationDegrees, cube.rotationAxis, cube.width, cube.height, cube.height, cube.color, cube.wcolor);
        }

        constexpr int fontSize = 20;
        auto line = [&](int l) { return l * fontSize; };

        RenderQueue::drawRectangle(0, 0, fontSize*30, 6*fontSize, {32, 32, 32, 200});
        RenderQueue::drawText(TextFormat("FPS: %d", static_cast<int>(fpsCalc.fps)) , 0, line(0), fontSize, RED);
        RenderQueue::drawText(TextFormat("GameLogic frametime: %4.2f ms", getAvgWorkTimeMs()), 0, line(1), fontSize, RED);
        RenderQueue::drawText(TextFormat("Physics frametime: %4.2f ms", physicsTh.getAvgWorkTimeMs()), 0, line(2), fontSize, RED);
        RenderQueue::drawText(TextFormat("Render frametime: %4.2f ms", renderAvgWorkTimeMs), 0, line(3), fontSize, RED);

        RenderQueue::drawText(TextFormat("Number of cubes: %d", static_cast<int>(cubes.size())), 0, line(4), fontSize, RED);
        RenderQueue::drawText("Press [ or ] change the number of cubes", 0, line(5), 20, BROWN);

        if (IsKeyPressed(KEY_LEFT_BRACKET))
        {
            int todo = std::min(100, static_cast<int>(cubes.size()));
            while(todo--)
            {
                cubes.pop_back();
            }
        }
        else if (IsKeyPressed(KEY_RIGHT_BRACKET))
        {
            addCube(100);
        }

        DOLOG("%s: Work done\n", name.c_str());
    }

    struct Cube
    {
        float rotationSpeed; // Rotation speed in full revolutions per second
        float rotationDegrees; // Rotation in degrees
        Vector3 rotationAxis;
        Vector3 position;
        float width;
        float height;
        float length;
        Color color;
        Color wcolor;
    };
    std::vector<Cube> cubes;
    FPSCalculator fpsCalc;
    std::mt19937 rdgen;

    Model cubeModel;
};

GameLogicThread gameLogicTh;

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

    gameLogicTh.start();
    physicsTh.start();

    uint32_t frameNum = 0;

    //
    // Main game loop
    // This loop behaves very simular to the worker threads, with the extra step to prepare for the next frame
    //
    while (!thCtrl.shouldFinish)  // Detect window close button or ESC key
    {

        {
            DOLOG("Starting frame %u\n", frameNum);
            DOLOG("%s: Arrived at frameStartBarrier.\n", "MainTread");
            thCtrl.frameStartBarrier.arrive_and_wait();
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
                RenderQueue::get().render();
            EndDrawing();
            SwapScreenBuffer();

            DOLOG("%s: Work done\n", "MainThread");

            if (WindowShouldClose())
                thCtrl.shouldFinish = true;

            renderWorkCalc.tick(std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - start).count());;
            renderAvgWorkTimeMs = renderWorkCalc.avgMsPerFrame;
        }

        // Signal that we are finished with our work.
        // This waits for all other threads to finish, so that then we can prepare for the next
        // frame.
        DOLOG("%s: Arrived at frameEndBarrier.\n", "MainThread");
        thCtrl.frameEndBarrier.arrive_and_wait();
        
        // At this point all threads are done with their work for the frame and are waiting for this thread to
        // kickstart the next frame. In this step, we update whatever Raylib internals we need, such as polling input.
        {
            RenderQueue::get().swapQueues();
            PollInputEvents();
            ++frameNum;
            auto now = std::chrono::high_resolution_clock::now();
            thCtrl.deltaSeconds = std::chrono::duration<float>(now - thCtrl.frameStartTime).count();
            thCtrl.frameStartTime = now;
        }
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();  // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
