#include "i18n.hpp"
#include "proxy.hpp"
#include <wincrypt.h>
#include <mutex>
#include <regex>
#include <cctype>
namespace mg {
static std::mutex proxyMutex;
static ProxySettings activeProxy;
ProxySettings getProxy(){std::lock_guard<std::mutex> lock(proxyMutex);return activeProxy;}
void setProxy(const ProxySettings& p){validateProxy(p);std::lock_guard<std::mutex> lock(proxyMutex);activeProxy=p;}
void validateProxy(const ProxySettings& p){
    if(!p.enabled)return;
    if(p.type<0||p.type>1||p.host.empty()||p.port<1||p.port>65535)throw std::runtime_error(tr("Specify a proxy type, address and port from 1 to 65535"));
    if(p.host.find_first_of(L" /\\@?#\r\n\t")!=p.host.npos||p.host.find(L"://")!=p.host.npos)throw std::runtime_error(tr("Enter only the server address, without http:// or a port"));
    if(!p.password.empty()&&p.user.empty())throw std::runtime_error(tr("Enter a username to use a password"));
}
static std::wstring encoded(const std::wstring& s){
    const char* hex="0123456789ABCDEF";std::wstring out;
    for(unsigned char c:utf8(s)){if(std::isalnum(c)&&c<128||c=='-'||c=='_'||c=='.'||c=='~')out+=wchar_t(c);else{out+=L'%';out+=hex[c>>4];out+=hex[c&15];}}return out;
}
std::wstring proxyUrl(const ProxySettings& p,bool credentials){
    if(!p.enabled)return L"";validateProxy(p);
    auto host=p.host;if(host.find(L':')!=host.npos&&host.front()!=L'[')host=L"["+host+L"]";
    return std::wstring(p.type?L"socks5h://":L"http://")+(credentials&&!p.user.empty()?encoded(p.user)+L":"+encoded(p.password)+L"@":L"")+host+L":"+std::to_wstring(p.port);
}
std::wstring redactProxy(std::wstring s,const ProxySettings& p){
    // Remove URL credentials even from yt-dlp's escaped Python debug representation.
    static const std::wregex auth(LR"((https?|socks5h?)://[^\s/'"]*@)",std::regex_constants::icase);
    s=std::regex_replace(s,auth,L"$1://***@");
    auto replace=[&](const std::wstring& needle){if(needle.empty())return;size_t n=0;while((n=s.find(needle,n))!=s.npos){s.replace(n,needle.size(),L"***");n+=3;}};
    replace(p.password);replace(encoded(p.password));
    return s;
}
std::wstring friendlyError(const std::wstring& s){
    auto t=s;std::transform(t.begin(),t.end(),t.begin(),::towlower);
    if(t.find(L"timed out")!=t.npos||t.find(L"timeout")!=t.npos)return tr(L"The server timed out. Check your connection or proxy and press Retry.");
    if(t.find(L"407")!=t.npos||t.find(L"authentication")!=t.npos||t.find(L"user was rejected")!=t.npos)return tr(L"Proxy authentication failed. Check your username and password.");
    if(t.find(L"could not resolve proxy")!=t.npos)return tr(L"The proxy address could not be resolved. Check your settings.");
    if(t.find(L"couldn't connect")!=t.npos||t.find(L"failed to connect")!=t.npos||t.find(L"connection refused")!=t.npos)return tr(L"Could not connect to the server or proxy. Check the address and port.");
    if(t.find(L"403")!=t.npos)return tr(L"The website denied access. Try another connection or browser cookies.");
    return s;
}
std::string protectSecret(const std::wstring& s){
    if(s.empty())return {};
    auto data=utf8(s);DATA_BLOB in{(DWORD)data.size(),(BYTE*)data.data()},out{};
    if(!CryptProtectData(&in,L"MediaGrabby proxy",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out))throw std::runtime_error(tr("Could not protect the password"));
    DWORD count=0;CryptBinaryToStringA(out.pbData,out.cbData,CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,nullptr,&count);
    std::string result(count,'\0');BOOL ok=CryptBinaryToStringA(out.pbData,out.cbData,CRYPT_STRING_BASE64|CRYPT_STRING_NOCRLF,result.data(),&count);LocalFree(out.pbData);
    if(!ok)throw std::runtime_error(tr("Could not save the password"));result.resize(count);while(!result.empty()&&result.back()=='\0')result.pop_back();return result;
}
std::wstring unprotectSecret(const std::string& s){
    if(s.empty())return {};DWORD size=0;
    if(!CryptStringToBinaryA(s.c_str(),0,CRYPT_STRING_BASE64,nullptr,&size,nullptr,nullptr))throw std::runtime_error(tr("Saved proxy data is corrupted"));
    std::vector<BYTE> bytes(size);CryptStringToBinaryA(s.c_str(),0,CRYPT_STRING_BASE64,bytes.data(),&size,nullptr,nullptr);
    DATA_BLOB in{size,bytes.data()},out{};
    if(!CryptUnprotectData(&in,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out))throw std::runtime_error(tr("The password was saved by a different Windows user. Enter it again in proxy settings."));
    std::string text((char*)out.pbData,out.cbData);SecureZeroMemory(out.pbData,out.cbData);LocalFree(out.pbData);return wide(text);
}
ProxySettings loadProxy(const fs::path& file){
    auto j=readJson(file);ProxySettings p;p.enabled=j.value("enabled",false);p.type=j.value("type",0);p.host=wide(j.value("host",std::string()));p.port=j.value("port",0);p.user=wide(j.value("user",std::string()));
    p.password=unprotectSecret(j.value("passwordProtected",std::string()));validateProxy(p);return p;
}
void saveProxy(const fs::path& file,const ProxySettings& p){validateProxy(p);writeJson(file,{{"enabled",p.enabled},{"type",p.type},{"host",utf8(p.host)},{"port",p.port},{"user",utf8(p.user)},{"passwordProtected",protectSecret(p.password)}});}
}
