#pragma once

 #include "Nexus_Utilities/SDL_Wrappers.h"
 #include <iostream>
 #include <string>

 namespace NEXUS_WINDOWING {
    class Window 
    {
    private:
        WindowPtr m_pWindow;
        std::string m_sTitle;
        int m_Width, m_Height, m_XPos, m_YPos;
        SDL_WindowFlags m_WindowFlags;

    private:
        void CreateNewWindow(SDL_WindowFlags flags);

    public:
        // 不显式传 flags，让被委托构造函数的默认实参（含 SDL_WINDOW_VULKAN）生效；
        // 传 NULL 会把它覆盖成 0，导致窗口不带 Vulkan 支持
        Window() : Window("default_window", 640, 480, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, true)
        {

        }
        Window(const std::string title, int width, int height, int x_pos, int y_pos, bool v_sync = true,
            SDL_WindowFlags flags = (SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MOUSE_CAPTURE));
        ~Window();

        inline WindowPtr& GetWindow() { return m_pWindow; }
        inline const std::string& GetWindowName() const { return m_sTitle; }

        inline int GetXPos() const { return m_XPos; }
        // SetXPos/SetYPos 原本声明返回 const int 却没有 return 语句，属于未定义行为，改为 void
        inline void SetXPos(int x_pos) { m_XPos = x_pos; }
        inline int GetYPos() const { return m_YPos; }
        inline void SetYPos(int y_pos) { m_YPos = y_pos; }

        inline int GetWidth() const { return m_Width; }
        inline int GetHeight() const { return m_Height; }

        void SetWindowName(const std::string& name);
       
    };
}