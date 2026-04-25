#include <windows.h>
#include <wininet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// 函数通过URL加载数据
unsigned char* 下载图像数据(const char* 网址, int* 输出大小) {
    HINTERNET 网络句柄 = InternetOpenA("图像加载器/1.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!网络句柄) return NULL;

    HINTERNET 网址句柄 = InternetOpenUrlA(网络句柄, 网址, NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
    if (!网址句柄) {
        InternetCloseHandle(网络句柄);
        return NULL;
    }

    // 加载到内存（简化版 - 分块读取）
    unsigned char* 数据 = malloc(1024 * 1024); // 初始缓冲区1MB
    if (!数据) {
        InternetCloseHandle(网址句柄);
        InternetCloseHandle(网络句柄);
        return NULL;
    }

    DWORD 总共读取 = 0;
    DWORD 缓冲区大小 = 1024 * 1024;
    
    while (1) {
        DWORD 本次读取 = 0;
        if (!InternetReadFile(网址句柄, 数据 + 总共读取, 缓冲区大小 - 总共读取, &本次读取)) break;
        if (本次读取 == 0) break;
        
        总共读取 += 本次读取;
        
        // 如果缓冲区满，则扩展
        if (总共读取 >= 缓冲区大小 - 4096) {
            缓冲区大小 *= 2;
            unsigned char* 新数据 = realloc(数据, 缓冲区大小);
            if (!新数据) {
                free(数据);
                InternetCloseHandle(网址句柄);
                InternetCloseHandle(网络句柄);
                return NULL;
            }
            数据 = 新数据;
        }
    }

    InternetCloseHandle(网址句柄);
    InternetCloseHandle(网络句柄);

    *输出大小 = 总共读取;
    return 数据;
}

// 全局变量
HBITMAP g_位图句柄 = NULL;
int g_图像宽度 = 0, g_图像高度 = 0;

// 窗口过程
LRESULT CALLBACK 窗口过程(HWND 窗口句柄, UINT 消息, WPARAM 参数1, LPARAM 参数2) {
    switch (消息) {
        case WM_CREATE: {
            int 数据大小;
            unsigned char* jpeg数据 = 下载图像数据("https://goodgame.ru/files/avatars/av_51631_AM9g.jpg", &数据大小);
            
            if (!jpeg数据) {
                MessageBoxW(窗口句柄, L"无法加载图像", L"错误", MB_ICONERROR);
                PostQuitMessage(1);
                return 0;
            }
            
            // 通过stb_image解码JPEG
            int 宽度, 高度, 通道数;
            unsigned char* 图像数据 = stbi_load_from_memory(jpeg数据, 数据大小, &宽度, &高度, &通道数, 4); // 4 = RGBA
            free(jpeg数据);
            
            if (!图像数据) {
                MessageBoxW(窗口句柄, L"无法解码JPEG", L"错误", MB_ICONERROR);
                PostQuitMessage(1);
                return 0;
            }
            
            g_图像宽度 = 宽度;
            g_图像高度 = 高度;
            
            // 创建设备无关位图(DIB)
            BITMAPINFO 位图信息 = {0};
            位图信息.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            位图信息.bmiHeader.biWidth = 宽度;
            位图信息.bmiHeader.biHeight = -高度; // 负值 = 从上到下
            位图信息.bmiHeader.biPlanes = 1;
            位图信息.bmiHeader.biBitCount = 32;
            位图信息.bmiHeader.biCompression = BI_RGB;
            
            void* 像素位;
            HDC 设备上下文 = GetDC(窗口句柄);
            g_位图句柄 = CreateDIBSection(设备上下文, &位图信息, DIB_RGB_COLORS, &像素位, NULL, 0);
            ReleaseDC(窗口句柄, 设备上下文);
            
            if (g_位图句柄 && 像素位) {
                // 复制像素（RGBA -> BGRA 用于Windows）
                for (int y = 0; y < 高度; y++) {
                    unsigned int* 目标 = (unsigned int*)((unsigned char*)像素位 + y * 宽度 * 4);
                    unsigned int* 源 = (unsigned int*)(图像数据 + y * 宽度 * 4);
                    for (int x = 0; x < 宽度; x++) {
                        unsigned int rgba = 源[x];
                        unsigned char r = (rgba >> 0) & 0xFF;
                        unsigned char g = (rgba >> 8) & 0xFF;
                        unsigned char b = (rgba >> 16) & 0xFF;
                        unsigned char a = (rgba >> 24) & 0xFF;
                        目标[x] = (b << 16) | (g << 8) | r | (a << 24); // BGRA
                    }
                }
            }
            
            stbi_image_free(图像数据);
            
            if (!g_位图句柄) {
                MessageBoxW(窗口句柄, L"无法创建位图", L"错误", MB_ICONERROR);
                PostQuitMessage(1);
            }
            
            return 0;
        }
        
        case WM_PAINT: {
            PAINTSTRUCT 绘制结构;
            HDC 设备上下文 = BeginPaint(窗口句柄, &绘制结构);
            
            if (g_位图句柄) {
                HDC 内存上下文 = CreateCompatibleDC(设备上下文);
                HBITMAP 旧位图 = SelectObject(内存上下文, g_位图句柄);
                
                // 拉伸至整个窗口
                RECT 客户区;
                GetClientRect(窗口句柄, &客户区);
                StretchBlt(设备上下文, 0, 0, 客户区.right, 客户区.bottom, 
                          内存上下文, 0, 0, g_图像宽度, g_图像高度, SRCCOPY);
                
                SelectObject(内存上下文, 旧位图);
                DeleteDC(内存上下文);
            }
            
            EndPaint(窗口句柄, &绘制结构);
            return 0;
        }
        
        case WM_ERASEBKGND:
            return 1; // 防止闪烁
            
        case WM_DESTROY:
            if (g_位图句柄) DeleteObject(g_位图句柄);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(窗口句柄, 消息, 参数1, 参数2);
}

int WINAPI wWinMain(HINSTANCE 实例句柄, HINSTANCE 前一实例, PWSTR 命令行, int 显示方式) {
    const wchar_t 类名[] = L"图像加载器窗口";
    
    WNDCLASSEXW 窗口类 = {0};
    窗口类.cbSize = sizeof(WNDCLASSEXW);
    窗口类.lpfnWndProc = 窗口过程;
    窗口类.hInstance = 实例句柄;
    窗口类.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    窗口类.lpszClassName = 类名;
    RegisterClassExW(&窗口类);
    
    HWND 窗口句柄 = CreateWindowExW(
        0, 类名, L"已加载的图像",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, 实例句柄, NULL
    );
    
    if (!窗口句柄) return 0;
    
    MSG 消息;
    while (GetMessage(&消息, NULL, 0, 0)) {
        TranslateMessage(&消息);
        DispatchMessage(&消息);
    }
    
    return 0;
}
