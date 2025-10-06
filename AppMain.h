#pragma once
#include <Windows.h>

class AppMain {
public:
    bool Initialize(HINSTANCE hInst);
    int  Run();
    void Finalize();

private:
    HWND hwnd_ = nullptr;
    bool running_ = true;

    bool CreateAppWindow(HINSTANCE hInst);
};
