#pragma once
#include "platform.hpp"
namespace mg {
struct ProxySettings {
    bool enabled=false;
    int type=0; // 0 HTTP, 1 SOCKS5 with remote DNS
    std::wstring host,user,password;
    int port=0;
};
void validateProxy(const ProxySettings& p);
std::wstring proxyUrl(const ProxySettings& p,bool credentials=true);
std::wstring redactProxy(std::wstring s,const ProxySettings& p);
std::wstring friendlyError(const std::wstring& s);
ProxySettings getProxy();
void setProxy(const ProxySettings& p);
ProxySettings loadProxy(const fs::path& file);
void saveProxy(const fs::path& file,const ProxySettings& p);
std::string protectSecret(const std::wstring& text);
std::wstring unprotectSecret(const std::string& text);
std::string httpTextUsing(const std::wstring& url,std::atomic_bool& cancel,const ProxySettings& proxy);
LRESULT CALLBACK noWheelCombo(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR);
}
