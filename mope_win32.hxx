#ifndef MOPE_WINDOW_H
#define MOPE_WINDOW_H

/*
    To use, #define MOPE_WINDOW_IMPL before the #include in exactly one of
    your source files
*/


#include <utility>
#include <unordered_map>
#include <string_view>
#include <bitset>
#include <iostream>
#include <string>
#include <mutex>
#include <atomic>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <Windows.h>
#include <strsafe.h>

#include "mope_illustrator/mope_illustrator.hxx"

namespace
{
    std::wstring wfromcstr(const char* cstr, size_t len)
    {
        wchar_t* buf = new wchar_t[len + 1];
        size_t n_chars{ };
        mbstowcs_s(&n_chars, buf, len + 1, cstr, len);
        std::wstring wstr(buf);
        delete[] buf;
        return wstr;
    }
}

/*============================================================================*\
|  Declarations                                                                |
\*============================================================================*/

namespace mope
{
    enum class Key : uint8_t {
        A = 'A', B = 'B', C = 'C', D = 'D', E = 'E', F = 'F', G = 'G', H = 'H', I = 'I',
        J = 'J', K = 'K', L = 'L', M = 'M', N = 'N', O = 'O', P = 'P', Q = 'Q', R = 'R',
        S = 'S', T = 'T', U = 'U', V = 'V', W = 'W', X = 'X', Y = 'Y', Z = 'Z',
        R0 = '0', R1 = '1', R2 = '2', R3 = '3', R4 = '4',
        R5 = '5', R6 = '6', R7 = '7', R8 = '8', R9 = '9',
        F1 = VK_F1, F2 = VK_F2, F3 = VK_F3, F4 = VK_F4, F5 = VK_F5, F6 = VK_F6,
        F7 = VK_F7, F8 = VK_F8, F9 = VK_F9, F10 = VK_F10, F11 = VK_F11, F12 = VK_F12,
        RETURN = VK_RETURN, BACK = VK_BACK, INS = VK_INSERT, DEL = VK_DELETE,
        CTRL = VK_CONTROL, ALT = VK_MENU, ESC = VK_ESCAPE, SHIFT = VK_SHIFT, CAPS = VK_CAPITAL,
        PGUP = VK_PRIOR, PGDN = VK_NEXT, END = VK_END, HOME = VK_HOME, PAUSE = VK_PAUSE,
        LEFT = VK_LEFT, RIGHT = VK_RIGHT, UP = VK_UP, DOWN = VK_DOWN,
        TAB = VK_TAB, SPACE = VK_SPACE,
        PLUS = VK_OEM_PLUS, MINUS = VK_OEM_MINUS, PERIOD = VK_OEM_PERIOD, COMMA = VK_OEM_COMMA,
        OEM_1 = VK_OEM_1, OEM_2 = VK_OEM_2, OEM_3 = VK_OEM_3, OEM_4 = VK_OEM_4,
        OEM_5 = VK_OEM_5, OEM_6 = VK_OEM_6, OEM_7 = VK_OEM_8,
    };

    class RenderingContext : public BaseRenderer
    {
    public:
        RenderingContext(HWND hwnd);
        ~RenderingContext();

        // move - okay
        RenderingContext(RenderingContext&&);
        RenderingContext& operator=(RenderingContext&&);

        // copy - not okay
        RenderingContext(RenderingContext&) = delete;
        RenderingContext& operator=(RenderingContext&) = delete;

        void showFrame() override;

    private:
        HDC m_hdc;
        HGLRC m_hglrc;

        HGLRC create();
    };

    class Window : public BaseWindow
    {
    public:
        Window(std::wstring name, int width, int height);
        Window(std::string_view name, int width, int height);
        ~Window();

        std::unique_ptr<BaseRenderer> getRenderer() override;
        void setTitle(LPCWSTR title);
        void setTitle(std::string_view title) override;
        int getWidth() override;
        int getHeight() override;
        int retrieveYDelta() override;
        int retrieveXDelta() override;
        std::bitset<256> getKeyStates() override;
        bool running() override;
        void close() override;

    private:
        static const LPCWSTR m_windowClass;
        HWND m_hwnd = NULL;

        std::thread m_messageThread{ };
        std::atomic_bool m_built{ false };
        std::atomic_bool m_closing{ false };
        std::atomic_bool m_destroying{ false };
        std::once_flag m_destroyOnce{ };
        
        std::atomic_int m_width = 0;
        std::atomic_int m_height = 0;

        int m_xLastPos = 0;
        int m_yLastPos = 0;
        std::atomic_int m_xPos = 0;
        std::atomic_int m_yPos = 0;

        std::mutex m_muxKeystates{ };
        std::bitset<256> m_keystates{ };

        ATOM registerClass(HINSTANCE hinstance);
        HWND createWindow(LPCWSTR name, HINSTANCE hinstance, int width, int height);
        void execute(std::wstring name, int width, int height);
        void messageLoop();

        // Callback for Windows event handling
        static LRESULT CALLBACK windowProc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam);

        // Most Windows event handling is delegated to this...
        LRESULT handleMessage(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam);

        // ... which then forwards events to the handlers
        void handleMouseMove(LPARAM lparam);
        void handleSize(LPARAM lparam);
        void handleKillFocus();
        void handleKeyUp(WPARAM vk_code);
        void handleKeyDown(WPARAM vk_code);
        void handleClose();
        void handleDestroy();
    };
}

#endif MOPE_WINDOW_H

#ifdef MOPE_WINDOW_IMPL
#undef MOPE_WINDOW_IMPL

/*============================================================================*\
|  Implementation                                                              |
\*============================================================================*/

namespace mope
{
    const LPCWSTR Window::m_windowClass = L"mope";

    Window::Window(std::wstring name, int width, int height)
    {
        m_messageThread = std::thread{ &Window::execute, this, name, width, height };
        m_built.wait(false);
    }

    Window::Window(std::string_view name, int width, int height)
        : Window(wfromcstr(name.data(), name.size()), width, height)
    { }

    Window::~Window()
    {
        m_destroying = true;
        close();
        m_messageThread.join();
    }

    std::unique_ptr<BaseRenderer> Window::getRenderer()
    {
        return std::make_unique<RenderingContext>(m_hwnd);
    }

    // Makes the given string the title of the window
    void Window::setTitle(LPCWSTR title)
    {
        SetWindowText(m_hwnd, title);
    }

    void Window::setTitle(std::string_view title)
    {
        setTitle(wfromcstr(title.data(), title.length()).c_str());
    }

    int Window::getWidth()
    {
        return m_width;
    }

    int Window::getHeight()
    {
        return m_height;
    }

    int Window::retrieveXDelta()
    {
        int x_pos = m_xPos;
        int x_delta = x_pos - m_xLastPos;
        m_xLastPos = x_pos;
        return x_delta;
    }

    int Window::retrieveYDelta()
    {
        int y_pos = m_yPos;
        int y_delta = y_pos - m_yLastPos;
        m_yLastPos = y_pos;
        return y_delta;
    }

    std::bitset<256> Window::getKeyStates()
    {
        std::lock_guard lg{ m_muxKeystates };
        return m_keystates;
    }

    void Window::close()
    {
        PostMessage(m_hwnd, WM_CLOSE, NULL, NULL);
    }

    bool Window::running()
    {
        return !m_closing;
    }


    /*========================================================================*\
    |  Window creation                                                         |
    \*========================================================================*/

    ATOM Window::registerClass(HINSTANCE hinstance)
    {
        WNDCLASSEX wcx{ }, gwc{ };
        if (GetClassInfoEx(hinstance, m_windowClass, &gwc))
        {
            // class already exists - we must have already registered it
            return TRUE;
        }

        wcx.cbSize = sizeof(WNDCLASSEX);
        wcx.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
        wcx.lpfnWndProc = Window::windowProc;
        wcx.lpszClassName = m_windowClass;
        wcx.hbrBackground = NULL;

        return RegisterClassEx(&wcx);
    }

    HWND Window::createWindow(LPCWSTR name, HINSTANCE hinstance, int width, int height)
    {
        // Use default width/height if args are 0
        int w = width > 0 ? width : CW_USEDEFAULT;
        int h = height > 0 ? height : CW_USEDEFAULT;

        // Default window settings
        DWORD dwStyle = WS_OVERLAPPEDWINDOW;
        DWORD dwExStyle = 0;
        int x = CW_USEDEFAULT;
        int y = CW_USEDEFAULT;
        HWND hWndParent = NULL;
        HMENU hMenu = NULL;

        return CreateWindowEx(
            dwExStyle, m_windowClass, name, dwStyle,
            x, y, w, h, hWndParent, hMenu, hinstance, this
        );
    }

    void Window::execute(std::wstring name, int width, int height)
    {
        HINSTANCE hinstance = GetModuleHandle(NULL);
        if (registerClass(hinstance)
            && (m_hwnd = createWindow(name.c_str(), hinstance, width, height)))
        {
            ShowWindow(m_hwnd, SW_SHOWDEFAULT);
            ShowCursor(FALSE);
        }

        m_built = true;
        m_built.notify_all();

        if (!m_hwnd) return;
        messageLoop();
    }

    /*========================================================================*\
    |  Message Handling                                                        |
    \*========================================================================*/


    void Window::messageLoop()
    {
        MSG msg{ };
        while (GetMessage(&msg, NULL, 0, 0) > 0)
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    LRESULT CALLBACK Window::windowProc(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam)
    {
        // On window creation, save a pointer to the window structure
        if (umsg == WM_NCCREATE) {
            CREATESTRUCT* pcs = reinterpret_cast<CREATESTRUCT*>(lparam);
            Window* pthis = reinterpret_cast<Window*>(pcs->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pthis));
            return TRUE;
        }
        // Retrieve the window pointer and call the handler
        else {
            Window *pthis = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
            if (pthis) {
                return pthis->handleMessage(hwnd, umsg, wparam, lparam);
            }
            else {
                return DefWindowProc(hwnd, umsg, wparam, lparam);
            }
        }
    }

    LRESULT Window::handleMessage(HWND hwnd, UINT umsg, WPARAM wparam, LPARAM lparam)
    {
        switch (umsg)
        {
        case WM_MOUSEMOVE:      handleMouseMove(lparam);    break;
        case WM_KEYDOWN:        handleKeyDown(wparam);      break;  
        case WM_KEYUP:          handleKeyUp(wparam);        break;
        case WM_SIZE:           handleSize(lparam);         break;
        case WM_KILLFOCUS:      handleKillFocus();          break;
        case WM_CLOSE:          handleClose();              break;
        case WM_DESTROY:        handleDestroy();            break;
        default: return DefWindowProc(hwnd, umsg, wparam, lparam);
        }

        return 0;
    }

    void Window::handleMouseMove(LPARAM lparam)
    {
        m_xPos = LOWORD(lparam);
        m_yPos = HIWORD(lparam);
    }

    void Window::handleKeyUp(WPARAM vk_code)
    {
        std::lock_guard lg{ m_muxKeystates };
        m_keystates.reset(vk_code);
    }

    void Window::handleKeyDown(WPARAM vk_code)
    {
        std::lock_guard lg{ m_muxKeystates };
        m_keystates.set(vk_code);
    }

    void Window::handleSize(LPARAM lparam)
    {
        m_width = LOWORD(lparam);
        m_height = HIWORD(lparam);
    }

    void Window::handleKillFocus()
    {
        std::lock_guard lg{ m_muxKeystates };
        m_keystates.reset();
    }

    void Window::handleClose()
    {
        m_closing = true;
        if (m_destroying) {
            std::call_once(m_destroyOnce, [this]() { DestroyWindow(m_hwnd); });
        }
    }

    void Window::handleDestroy()
    {
        SetWindowLongPtr(m_hwnd, GWLP_USERDATA, NULL);
        PostQuitMessage(0);
    }

    RenderingContext::RenderingContext(HWND hwnd)
        : m_hdc{ GetDC( hwnd )}
        , m_hglrc{ create() }
    {
        wglMakeCurrent(m_hdc, m_hglrc);
    }
    
    RenderingContext::~RenderingContext()
    {
        if (m_hglrc)
        {
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(m_hglrc);
        }
    }

    void RenderingContext::showFrame()
    {
        SwapBuffers(m_hdc);
    }

    HGLRC RenderingContext::create()
    {
        PIXELFORMATDESCRIPTOR pfd = { 0 };
        pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 16;
        pfd.iLayerType = PFD_MAIN_PLANE;
        
        int pf = ChoosePixelFormat(m_hdc, &pfd);
        SetPixelFormat(m_hdc, pf, &pfd);
        
        return wglCreateContext(m_hdc);
    }

    RenderingContext::RenderingContext(RenderingContext&& other)
    {
        (*this) = std::move(other);
    }

    RenderingContext& RenderingContext::operator=(RenderingContext&& other)
    {
        m_hdc = other.m_hdc;
        m_hglrc = other.m_hglrc;
        other.m_hglrc = NULL;
        return *this;
    }
}

#endif //MOPE_WINDOW_IMPL