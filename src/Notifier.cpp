#include "Notifier.h"

void Notifier::notifyBlocked(const std::wstring& app, const std::wstring& remote) {
    if (!enabled_ || !cb_) return;
    std::wstring key = app + L"|" + remote;
    if (key == lastKey_) return;
    lastKey_ = key;
    cb_(L"Blocked connection attempt", app + L" \u2192 " + remote);
}
