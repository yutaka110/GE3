#pragma once
#include <Windows.h>

class AppMain {
public:
    bool Initialize(HINSTANCE hInstance);
    int Run();
   
private:
    void Update();    // ← 追加
    void Render();    // ← 追加
    void Finalize();  // ← 追加

private:
    HINSTANCE hInstance_ = nullptr;
};
