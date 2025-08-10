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

void RenderQueue::Render()
{
    UpdateCamera(&camera, CAMERA_PERSPECTIVE);

    // Render 3D group
    BeginMode3D(camera);
		RenderSet->Q[static_cast<int>(RenderGroup::World)].CallAll();
		RenderSet->Q[static_cast<int>(RenderGroup::World)].Clear();
    EndMode3D();

    // Render UI group
    RenderSet->Q[static_cast<int>(RenderGroup::UI)].CallAll();
    RenderSet->Q[static_cast<int>(RenderGroup::UI)].Clear();
}

// Helper code
namespace
{
    // Pushes a string to the queue.
    // A render command lambda can then keep the Ref and capture that instead
    RenderCmdQueue::Ref PushString(RenderCmdQueue& q, std::string_view str)
    {
        // +1, to make it null terminated
        RenderCmdQueue::Ref ref = q.OobPushEmpty<uint8_t>(static_cast<uint8_t>(str.size() + 1));
        uint8_t* ptr = q.OobAt(ref);
        memcpy(ptr, str.data(), str.size());
        ptr[str.size()] = 0;
        return ref;
    }

}  // namespace


void RenderQueue::DrawText(std::string_view text, int posX, int posY, int fontSize, Color color)
{
    RenderCmdQueue& q = GetQ(RenderGroup::UI);

    //
    // Since we can't capture an std::string (due to the limitations of the container), we can insert it as oob data, capture
    // the Ref insteand, and then get the string data back. This is all done without allocating memory.
    q.Push([textRef = PushString(q, text), posX, posY, fontSize, color](RenderCmdQueue& q)
    {
        ::DrawText(reinterpret_cast<const char*>(q.OobAt(textRef)), posX, posY, fontSize, color);
    });
}

void RenderQueue::DrawRectangle(int posX, int posY, int width, int height, Color color)
{
    GetQ(RenderGroup::UI).Push([posX, posY, width, height, color](RenderCmdQueue& )
    {
        ::DrawRectangle(posX, posY, width, height, color);
    });
}

void RenderQueue::DrawCube(Vector3 position, float width, float height, float length, Color color)
{
    GetQ(RenderGroup::World).Push([position, width, height, length, color](RenderCmdQueue& )
    {
        ::DrawCube(position, width, height, length, color);
    });
}

void RenderQueue::DrawCubeWires(Vector3 position, float width, float height, float length, Color color)
{
    GetQ(RenderGroup::World).Push([position, width, height, length, color](RenderCmdQueue&)
    {
        ::DrawCubeWires(position, width, height, length, color);
    });
}

void RenderQueue::DrawCubeEx(Vector3 position, float degrees, Vector3 rotationAxis, float width, float height, float length, Color color, Color wcolor)
{
    GetQ(RenderGroup::World).Push([position, degrees, rotationAxis, width, height, length, color, wcolor](RenderCmdQueue& )
    {
        ::rlPushMatrix();
            ::rlTranslatef(position.x, position.y, position.z);
            ::rlRotatef(degrees, rotationAxis.x, rotationAxis.y, rotationAxis.z);
            ::DrawCube({}, width, height, length, color);
            ::DrawCubeWires({}, width, height, length, wcolor);
        ::rlPopMatrix();
    });
}

