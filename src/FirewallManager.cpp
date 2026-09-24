#include "FirewallManager.h"
#include <windows.h>
#include <netfw.h>
#include <comdef.h>
#include <sstream>
#include <fstream>
#include <algorithm>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

static std::wstring hrText(HRESULT hr) {
    _com_error e(hr);
    return e.ErrorMessage();
}

FirewallManager::FirewallManager() = default;
FirewallManager::~FirewallManager() {
    if (policy_) reinterpret_cast<INetFwPolicy2*>(policy_)->Release();
}

bool FirewallManager::isElevated() const {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elev{};
    DWORD sz = sizeof(elev);
    BOOL ok = GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &sz);
    CloseHandle(token);
    return ok && elev.TokenIsElevated;
}

bool FirewallManager::initialize(std::wstring& error) {
    if (policy_) return true;
    INetFwPolicy2* p = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(NetFwPolicy2), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&p));
    if (FAILED(hr)) { error = L"Windows Firewall API initialization failed: " + hrText(hr); return false; }
    policy_ = p;
    return true;
}

std::wstring FirewallManager::ruleName(const std::wstring& path) const {
    std::wstring s = L"NetControl Block - ";
    for (wchar_t c : path) s += (c == L'\\' ? L'_' : c);
    return s;
}

bool FirewallManager::removeRule(const std::wstring& path, std::wstring& error) {
    return removeRuleByName(ruleName(path), error);
}

bool FirewallManager::removeRuleByName(const std::wstring& name, std::wstring& error) {
    if (!initialize(error)) return false;
    auto* p = reinterpret_cast<INetFwPolicy2*>(policy_);
    INetFwRules* rules = nullptr;
    HRESULT hr = p->get_Rules(&rules);
    if (FAILED(hr)) { error = L"Unable to access firewall rules: " + hrText(hr); return false; }
    BSTR b = SysAllocString(name.c_str());
    hr = rules->Remove(b);
    SysFreeString(b);
    rules->Release();
    if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
        error = L"Unable to remove firewall rule: " + hrText(hr);
        return false;
    }
    return true;
}

bool FirewallManager::setRuleEnabled(const std::wstring& name, bool enabled, std::wstring& error) {
    if (!initialize(error)) return false;
    auto* p = reinterpret_cast<INetFwPolicy2*>(policy_);
    INetFwRules* rules = nullptr;
    HRESULT hr = p->get_Rules(&rules);
    if (FAILED(hr)) { error = hrText(hr); return false; }
    BSTR b = SysAllocString(name.c_str());
    INetFwRule* rule = nullptr;
    hr = rules->Item(b, &rule);
    SysFreeString(b);
    rules->Release();
    if (FAILED(hr)) { error = L"Rule not found: " + name; return false; }
    hr = rule->put_Enabled(enabled ? VARIANT_TRUE : VARIANT_FALSE);
    rule->Release();
    if (FAILED(hr)) { error = hrText(hr); return false; }
    return true;
}

std::vector<FirewallRuleInfo> FirewallManager::listRules(bool netControlOnly) const {
    std::vector<FirewallRuleInfo> out;
    auto* self = const_cast<FirewallManager*>(this);
    std::wstring err;
    if (!self->initialize(err)) return out;
    auto* p = reinterpret_cast<INetFwPolicy2*>(policy_);
    INetFwRules* rules = nullptr;
    if (FAILED(p->get_Rules(&rules)) || !rules) return out;
    IUnknown* enum_ = nullptr;
    if (SUCCEEDED(rules->get__NewEnum(&enum_)) && enum_) {
        IEnumVARIANT* ev = nullptr;
        if (SUCCEEDED(enum_->QueryInterface(IID_PPV_ARGS(&ev))) && ev) {
            VARIANT var;
            ULONG fetched = 0;
            while (ev->Next(1, &var, &fetched) == S_OK) {
                if (var.vt == VT_UNKNOWN && var.punkVal) {
                    INetFwRule* r = nullptr;
                    if (SUCCEEDED(var.punkVal->QueryInterface(IID_PPV_ARGS(&r))) && r) {
                        FirewallRuleInfo info;
                        BSTR bn = nullptr; r->get_Name(&bn);
                        if (bn) { info.name = bn; SysFreeString(bn); }
                        BSTR ba = nullptr; r->get_ApplicationName(&ba);
                        if (ba) { info.application = ba; SysFreeString(ba); }
                        VARIANT_BOOL en = VARIANT_TRUE; r->get_Enabled(&en);
                        info.enabled = (en == VARIANT_TRUE);
                        info.netControl = info.name.rfind(L"NetControl Block - ", 0) == 0;
                        info.rule = r;
                        if (!netControlOnly || info.netControl) out.push_back(info);
                        r->Release();
                    }
                }
                VariantClear(&var);
            }
            ev->Release();
        }
        enum_->Release();
    }
    rules->Release();
    return out;
}

int FirewallManager::cleanupStaleRules(const std::vector<std::wstring>& livePaths, std::wstring& error) {
    auto rules = listRules(true);
    int removed = 0;
    for (auto& r : rules) {
        bool exists = !r.application.empty() && GetFileAttributesW(r.application.c_str()) != INVALID_FILE_ATTRIBUTES;
        bool tracked = std::find(livePaths.begin(), livePaths.end(), r.application) != livePaths.end();
        if (!exists || !tracked) {
            if (removeRuleByName(r.name, error)) ++removed;
        }
    }
    return removed;
}

bool FirewallManager::exportRules(const std::wstring& csvPath, std::wstring& error) const {
    auto rules = listRules(true);
    std::ofstream out(csvPath, std::ios::binary | std::ios::trunc);
    if (!out) { error = L"Cannot write file"; return false; }
    out << "path,mode,enabled\n";
    for (auto& r : rules) {
        std::string path;
        for (wchar_t c : r.application) path += static_cast<char>(c < 128 ? c : '?');
        out << path << ",block," << (r.enabled ? 1 : 0) << "\n";
    }
    return true;
}

bool FirewallManager::importRules(const std::wstring& csvPath, std::wstring& error) {
    std::ifstream in(csvPath);
    if (!in) { error = L"Cannot read file"; return false; }
    std::string line;
    std::getline(in, line);
    int applied = 0;
    while (std::getline(in, line)) {
        auto c1 = line.find(',');
        auto c2 = line.find(',', c1 + 1);
        if (c1 == std::string::npos) continue;
        std::string pathS = line.substr(0, c1);
        std::string mode = line.substr(c1 + 1, (c2 == std::string::npos ? line.size() : c2) - c1 - 1);
        std::wstring path(pathS.begin(), pathS.end());
        if (path.empty()) continue;
        std::wstring err;
        if (mode == "block") { if (setMode(path, AccessMode::Block, err)) ++applied; }
        else if (mode == "allow") { if (setMode(path, AccessMode::Allow, err)) ++applied; }
    }
    error = L"Imported " + std::to_wstring(applied) + L" rules";
    return applied > 0;
}

bool FirewallManager::setBlockRule(const std::wstring& path, bool block, std::wstring& error) {
    if (!initialize(error)) return false;
    if (!block) return removeRule(path, error);
    auto* p = reinterpret_cast<INetFwPolicy2*>(policy_);
    INetFwRules* rules = nullptr;
    HRESULT hr = p->get_Rules(&rules);
    if (FAILED(hr)) { error = L"Unable to access firewall rules: " + hrText(hr); return false; }
    BSTR ruleB = SysAllocString(ruleName(path).c_str());
    rules->Remove(ruleB);
    INetFwRule* rule = nullptr;
    hr = CoCreateInstance(__uuidof(NetFwRule), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&rule));
    if (FAILED(hr)) { SysFreeString(ruleB); rules->Release(); error = L"Unable to create firewall rule: " + hrText(hr); return false; }
    rule->put_Name(ruleB);
    BSTR desc = SysAllocString(L"Created by NetControl. Blocks outbound network access for this executable.");
    rule->put_Description(desc); SysFreeString(desc);
    BSTR app = SysAllocString(path.c_str());
    rule->put_ApplicationName(app); SysFreeString(app);
    rule->put_Direction(NET_FW_RULE_DIR_OUT);
    rule->put_Action(NET_FW_ACTION_BLOCK);
    rule->put_Enabled(VARIANT_TRUE);
    rule->put_Protocol(NET_FW_IP_PROTOCOL_ANY);
    rule->put_Profiles(NET_FW_PROFILE2_ALL);
    hr = rules->Add(rule);
    rule->Release();
    rules->Release();
    SysFreeString(ruleB);
    if (FAILED(hr)) { error = L"Windows rejected the firewall rule: " + hrText(hr); return false; }
    return true;
}

bool FirewallManager::setMode(const std::wstring& exePath, AccessMode mode, std::wstring& error) {
    if (exePath.empty()) { error = L"No executable path was supplied."; return false; }
    switch (mode) {
        case AccessMode::Block: return setBlockRule(exePath, true, error);
        case AccessMode::Allow: return setBlockRule(exePath, false, error);
        case AccessMode::Ask: return setBlockRule(exePath, false, error);
    }
    return false;
}

AccessMode FirewallManager::getMode(const std::wstring&) const {
    return AccessMode::Ask;
}
