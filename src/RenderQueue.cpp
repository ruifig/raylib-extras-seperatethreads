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

#include "RenderQueue.h"
#include "rlgl.h"

// The camera should not really be controlled by this code, but it's for simplicity
extern Camera3D camera;

void RenderQueue::render()
{
    UpdateCamera(&camera, CAMERA_PERSPECTIVE);

    // Render 3D group
    BeginMode3D(camera);
    m_renderSet->q[static_cast<int>(RenderGroup::World)].callAll();
    m_renderSet->q[static_cast<int>(RenderGroup::World)].clear();
    EndMode3D();

    // Render UI group
    m_renderSet->q[static_cast<int>(RenderGroup::UI)].callAll();
    m_renderSet->q[static_cast<int>(RenderGroup::UI)].clear();
}

// Helper code
namespace
{
    // Pushes a string to the queue.
    // A render command lambda can then keep the Ref and capture that instead
    RenderCmdQueue::Ref pushString(RenderCmdQueue& q, std::string_view str)
    {
        // +1, to make it null terminated
        RenderCmdQueue::Ref ref = q.oob_push_empty<uint8_t>(static_cast<uint8_t>(str.size() + 1));
        uint8_t* ptr = q.oobAt(ref);
        memcpy(ptr, str.data(), str.size());
        ptr[str.size()] = 0;
        return ref;
    }

}  // namespace


void RenderQueue::drawText(std::string_view text, int posX, int posY, int fontSize, Color color)
{
    RenderCmdQueue& q = getQ(RenderGroup::UI);
    //
    // Since we can't capture an std::string (due to the limitations of the container), we can insert it as oob data, capture
    // the Ref insteand, and then get the string data back. This is all done without allocating memory.
    q.push([textRef = pushString(q, text), posX, posY, fontSize, color](RenderCmdQueue& q)
    {
        DrawText(reinterpret_cast<const char*>(q.oobAt(textRef)), posX, posY, fontSize, color);
    });
}

void RenderQueue::drawRectangle(int posX, int posY, int width, int height, Color color)
{
    getQ(RenderGroup::UI).push([posX, posY, width, height, color](RenderCmdQueue& )
    {
        DrawRectangle(posX, posY, width, height, color);
    });
}


void RenderQueue::drawCube(Vector3 position, float width, float height, float length, Color color)
{
    getQ(RenderGroup::World).push([position, width, height, length, color](RenderCmdQueue& )
    {
        DrawCube(position, width, height, length, color);
    });
}

void RenderQueue::drawCubeWires(Vector3 position, float width, float height, float length, Color color)
{
    getQ(RenderGroup::World).push([position, width, height, length, color](RenderCmdQueue&)
    {
        DrawCubeWires(position, width, height, length, color);
    });
}

void RenderQueue::drawCubeEx(Vector3 position, float degrees, Vector3 rotationAxis, float width, float height, float length, Color color, Color wcolor)
{
    getQ(RenderGroup::World).push([position, degrees, rotationAxis, width, height, length, color, wcolor](RenderCmdQueue& )
    {
        rlPushMatrix();
            rlTranslatef(position.x, position.y, position.z);
            rlRotatef(degrees, rotationAxis.x, rotationAxis.y, rotationAxis.z);
            DrawCube({}, width, height, length, color);
            DrawCubeWires({}, width, height, length, wcolor);
        rlPopMatrix();
    });
}


void RenderQueue::drawModelEx(Model model, Vector3 position, Vector3 rotationAxis, float rotationAngle, Vector3 scale, Color tint)
{
    getQ(RenderGroup::World).push([model, position, rotationAxis, rotationAngle, scale, tint](RenderCmdQueue& )
    {
        DrawModelEx(model, position, rotationAxis, rotationAngle, scale, tint);
    });
}

void RenderQueue::drawModelWiresEx(Model model, Vector3 position, Vector3 rotationAxis, float rotationAngle, Vector3 scale, Color tint)
{
    getQ(RenderGroup::World).push([model, position, rotationAxis, rotationAngle, scale, tint](RenderCmdQueue& )
    {
        DrawModelWiresEx(model, position, rotationAxis, rotationAngle, scale, tint);
    });
}
