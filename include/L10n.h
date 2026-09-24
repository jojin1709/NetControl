#pragma once
#include <string>
#include <unordered_map>

class L10n {
public:
    static L10n& instance();
    void setLanguage(const std::wstring& lang);
    const std::wstring& language() const { return lang_; }
    const std::wstring& tr(const wchar_t* key) const;
    std::wstring format(const wchar_t* key, const std::wstring& a) const;
private:
    L10n();
    void loadEnglish();
    void loadGerman();
    void loadSpanish();
    std::wstring lang_{L"en"};
    std::unordered_map<std::wstring, std::wstring> map_{};
    mutable std::wstring fallback_;
};

#define TR(k) L10n::instance().tr(k)
