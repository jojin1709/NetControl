#include "App.h"
#include "Ui.h"
#include "FirewallManager.h"
#include "ProcessManager.h"
#include "NetworkMonitor.h"
#include "UsageStore.h"
#include <windows.h>
#include <objbase.h>

int App::run() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return 1;
    UsageStore store;
    store.load();
    FirewallManager fw;
    std::wstring err;
    if (!fw.initialize(err)) {
        MessageBoxW(nullptr, err.c_str(), L"NetControl", MB_ICONERROR);
        CoUninitialize();
        return 2;
    }
    ProcessManager pm;
    NetworkMonitor nm;
    Ui ui;
    if (!ui.create(GetModuleHandleW(nullptr), &fw, &pm, &nm, &store)) {
        CoUninitialize();
        return 3;
    }
    int r = ui.run();
    store.save();
    CoUninitialize();
    return r;
}
