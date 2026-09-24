#pragma once
#include <string>
#include <functional>

class Notifier {
public:
    using Callback = std::function<void(const std::wstring&, const std::wstring&)>;
    void setCallback(Callback cb) { cb_ = std::move(cb); }
    void notifyBlocked(const std::wstring& app, const std::wstring& remote);
    bool enabled() const { return enabled_; }
    void setEnabled(bool e) { enabled_ = e; }
private:
    Callback cb_;
    bool enabled_{true};
    std::wstring lastKey_;
};
