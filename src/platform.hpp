#pragma once
#include <windows.h>
#include <filesystem>
#include <atomic>
#include <functional>
#include <string>
#include <vector>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include "core.hpp"
namespace mg {
namespace fs=std::filesystem;
using json=nlohmann::json;
using Log=std::function<void(const std::wstring&)>;
using Progress=std::function<void(const std::wstring&,int)>;
struct Cancelled:std::runtime_error{Cancelled():runtime_error("Stopped"){}};
void checkCancel(const std::atomic_bool& c);
std::string utf8(const std::wstring& s);
std::wstring wide(const std::string& s);
std::wstring winError(const std::wstring& action);
fs::path programDir();
fs::path videosDir();
fs::path systemExe(const wchar_t* relative);
bool system64();
bool windows10();
void ensureWritable(const fs::path& dir);
json readJson(const fs::path& p,const json& fallback=json::object());
void writeJson(const fs::path& p,const json& value);
std::string sha256(const fs::path& p);
std::string httpText(const std::wstring& url,std::atomic_bool& cancel);
void httpDownload(const std::wstring& url,const fs::path& file,std::atomic_bool& cancel,const Progress& progress);
int runProcess(const std::vector<std::wstring>& args,const fs::path& cwd,std::atomic_bool& cancel,const Log& log,unsigned timeoutSeconds=0);
void extractZip(const fs::path& archive,const fs::path& dest,const std::vector<std::wstring>& names,std::atomic_bool& cancel,const Log& log);
void installDirectory(const fs::path& stage,const fs::path& target);
struct TempDir {
    fs::path path;
    explicit TempDir(const fs::path& parent);
    ~TempDir();
    TempDir(const TempDir&)=delete;
};
}

