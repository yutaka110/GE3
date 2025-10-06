#include "AppMain.h"
#include <windowsx.h> // GET_X_LPARAM など

// ローカルなウィンドウプロシージャ
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hWnd, msg, wp, lp);
}

bool AppMain::CreateAppWindow(HINSTANCE hInst) {
    const wchar_t kClass[] = L"DX12AppWindow";
    WNDCLASSEX wc{ sizeof(WNDCLASSEX) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = kClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    if (!RegisterClassEx(&wc)) return false;

    hwnd_ = CreateWindowEx(
        0, kClass, L"App",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
        nullptr, nullptr, hInst, nullptr);
    if (!hwnd_) return false;

    ShowWindow(hwnd_, SW_SHOWDEFAULT);
    return true;
}

bool AppMain::Initialize(HINSTANCE hInst) {
    if (!CreateAppWindow(hInst)) return false;

    // TODO: ここにエンジン初期化（DxContext/Media など）を後で移す
    return true;
}

int AppMain::Run() {
    MSG msg{};
    while (running_) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { running_ = false; break; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!running_) break;

        // TODO: 1フレーム処理（更新→描画→Present）を後で追加
    }
    Finalize();
    return static_cast<int>(msg.wParam);
}

void AppMain::Finalize() {
    // TODO: 後始末（リソース解放など）
}
