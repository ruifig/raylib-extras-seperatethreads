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

#include "raylib.h"
#include "RenderCmdQueue.h"

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

class RenderQueue
{
  public:
    RenderQueue()
    {
        // There can be only one RenderQueue object
        assert(!m_instance);

        m_instance = this;
        m_logicSet = &m_qset[0];
        m_renderSet = &m_qset[1];
    }

    static RenderQueue& get()
    {
        assert(m_instance);
        return *m_instance;
    }

    void swapQueues()
    {
        std::swap(m_logicSet, m_renderSet);
    }

    // Process all render commands in the rendering set.
    void render();

    //
    // The available commands that can be queued, just as an example.
    //
    // These should probably be outside the class, but intentionally putting them as members, to avoid
    // confusing with Raylib's functions.
    //
    // The example render commands match the Raylib's API, but that's not a requirement.
    //
    static void drawText(std::string_view text, int posX, int posY, int fontSize, Color color);
    static void drawRectangle(int posX, int posY, int width, int height, Color color);
    static void drawCube(Vector3 position, float width, float height, float length, Color color);
    static void drawCubeWires(Vector3 position, float width, float height, float length, Color color);
    // Renders a cube + wireframe, with a rotation
    static void drawCubeEx(Vector3 position, float degrees, Vector3 rotationAxis, float width, float height, float length, Color color, Color wcolor);
	static void drawModelEx(Model model, Vector3 position, Vector3 rotationAxis, float rotationAngle, Vector3 scale, Color tint);
	static void drawModelWiresEx(Model model, Vector3 position, Vector3 rotationAxis, float rotationAngle, Vector3 scale, Color tint);

  private:
    inline static RenderQueue* m_instance = nullptr;

    struct QueueSet
    {
        RenderCmdQueue q[static_cast<int>(RenderGroup::MAX)];
    } m_qset[2];

    QueueSet* m_logicSet;   // Queue that is being used by the game logic thread
    QueueSet* m_renderSet;  // Queue that is being used by the raylib thread

    // Shortcut to get the queue to insert new render commands
    static RenderCmdQueue& getQ(RenderGroup group)
    {
        return RenderQueue::get().m_logicSet->q[static_cast<int>(group)];
    }

};


