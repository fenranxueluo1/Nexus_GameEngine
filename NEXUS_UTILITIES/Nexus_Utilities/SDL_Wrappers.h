#pragma once

#include <SDL3/SDL.h>
#include <memory>

namespace NEXUS_UTIL {
    struct SDL_Destroyer
    {
        void operator() (SDL_Window* window) const;
        void operator() (SDL_Gamepad* controller) const;
        void operator() (SDL_Cursor* cursor) const;
    };
}

typedef std::shared_ptr<SDL_Gamepad> Controller;
// 注意：不能加 static，函数定义在 SDL_Wrappers.cpp 中，
// 加 static 会让每个包含本头文件的 TU 各得到一个未定义的内部链接声明。
inline Controller make_shared_controller(SDL_Gamepad* controller)
{
    return Controller(controller, NEXUS_UTIL::SDL_Destroyer{});
}

typedef std::shared_ptr<SDL_Cursor> Cursor;
Cursor make_shared_cursor(SDL_Cursor* cursor);

typedef std::unique_ptr<SDL_Window, NEXUS_UTIL::SDL_Destroyer> WindowPtr;
