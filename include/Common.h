/*******************************************************************************************
*
*   Common settings for the sample
* 
*   This example has been created using raylib 5.5 (www.raylib.com)
*   raylib is licensed under an unmodified zlib/libpng license (View raylib.h for details)
*
*   Copyright (c) 2025 Rui Figueira (https://github.com/ruifig)
*
********************************************************************************************/

#pragma once

#if defined(NDEBUG)
    #define DOLOG(...)
#else
    #define DOLOG(...) printf(##__VA_ARGS__)
#endif

