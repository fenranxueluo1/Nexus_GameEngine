#include "SDL_Wrappers.h"
#include <iostream>

void NEXUS_UTIL::SDL_Destroyer::operator()(SDL_Window *window) const
{
    SDL_DestroyWindow(window);
    std::cout << "SLD窗口已销毁" << std::endl;
}

void NEXUS_UTIL::SDL_Destroyer::operator()(SDL_Gamepad *controller) const
{
    SDL_CloseGamepad(controller);
    controller = nullptr;
    std::cout << "Closed SDL Game Controller!" << std::endl;
}

void NEXUS_UTIL::SDL_Destroyer::operator()(SDL_Cursor *cursor) const
{
    SDL_DestroyCursor(cursor);
}

Cursor make_shared_cursor(SDL_Cursor *cursor)
{
    return Cursor(cursor, NEXUS_UTIL::SDL_Destroyer{});
}
