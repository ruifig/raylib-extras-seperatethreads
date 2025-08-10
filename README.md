# raylib-extras-seperatethreads

Raylib sample on how to implement a render command queue.

Raylib's API is single threaded, but with some care, one can have thread dedicated to Raylib calls, while another thread runs the game logic AND query Raylib input.

A few things are displayed:

* **FPS: <N>** - Curent frames per second
* **GameLogic Frametime** - Time used by the game logic thread.
* **Physics Frametime** - Time used by the physics thread.
    * Note that the sample doesn't actually have physics thread, so this is just dummy work.
    * Also, phsyics would probably **NOT** be tied to the framerate. This is just to show that N threads can sync, not just 2.
* **Render Frametime** - Time used by the Raylib/Render thread.
* **Number of cubes** - Number of cubes currently being drawn. Use `[` and `]` to decrement/increment.

Comparing **GameLogic Frametime** and **Render Frametime** gives an idea of any potential benefit of using this approach.
As-in, if queuing up render commands is faster than executing them, then there is value in this approach, since it frees up cycles for the game logic.

# Coding conventions

This sample tries to follow the code conventions from https://github.com/raylib-extras/GameObjectsExample

* Class/struct names use `CamelCase`
* Functions and methods use `CamelCase`
* Fields use `CamelCase` without any prefixes.
* Local variables and function parameters use `someFoo`

