/*******************************************************************************************
*
*   Simple render queue
* 
*   This example has been created using raylib 5.5 (www.raylib.com)
*   raylib is licensed under an unmodified zlib/libpng license (View raylib.h for details)
*
*   Copyright (c) 2025 Rui Figueira (https://github.com/ruifig)
*
********************************************************************************************/

#include "RenderCmdQueue.h"

#include "raylib.h"

#include <cstdint>
#include <type_traits>
#include <limits>
#include <assert.h>
#include <stdlib.h>
#include <string_view>

enum class RenderGroup
{
    World,
    UI,
    MAX
};

/*!
 * Keeps two working sets of render command queues.
 * The user code is responsible for creating an instance, but only one instance can exist at one given time.
 */
class RenderQueue
{
  public:
    RenderQueue()
    {
        // There can be only one RenderQueue instance
        assert(!Instance);

        Instance = this;
        LogicSet = &QSet[0];
        RenderSet = &QSet[1];
    }

    static RenderQueue& Get()
    {
        assert(Instance);
        return *Instance;
    }

    void SwapQueues()
    {
        std::swap(LogicSet, RenderSet);
    }

    /*!
     * Process all render commands in the rendering set.
     */
    void Render();

    //
    // The available commands that can be queued, just as an example.
    //
    // These would ideally be outside the class, but can cause conflicts with Raylib's own API.
    // The example commands match the Raylib's API, but that's not a requirement. Commands can be as simple or complex as you need.
    //
    static void DrawText(std::string_view text, int posX, int posY, int fontSize, Color color);
    static void DrawRectangle(int posX, int posY, int width, int height, Color color);
    static void DrawCube(Vector3 position, float width, float height, float length, Color color);
    static void DrawCubeWires(Vector3 position, float width, float height, float length, Color color);
    // Renders a cube + wireframe, with a rotation
    static void DrawCubeEx(Vector3 position, float degrees, Vector3 rotationAxis, float width, float height, float length, Color color, Color wcolor);

  private:
    inline static RenderQueue* Instance = nullptr;

    struct QueueSet
    {
        RenderCmdQueue Q[static_cast<int>(RenderGroup::MAX)];
    } QSet[2];

    QueueSet* LogicSet;   // Queue that is being used by the game logic thread
    QueueSet* RenderSet;  // Queue that is being used by the raylib thread

    // Shortcut to get the queue to insert new render commands
    static RenderCmdQueue& GetQ(RenderGroup group)
    {
        return RenderQueue::Get().LogicSet->Q[static_cast<int>(group)];
    }

};


