#include "i18n.hpp"
#include "platform.hpp"
#include "proxy.hpp"
#include <curl/curl.h>
#include <bcrypt.h>
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <memory>

namespace mg {
struct Handle {
    HANDLE h=nullptr;
    explicit Handle(HANDLE v=nullptr):h(v){}
    ~Handle(){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);}
    Handle(const Handle&)=delete;
};
void checkCancel(const std::atomic_bool& c){if(c.load())throw Cancelled();}
std::string utf8(const std::wstring& s) {
    if(s.empty())return {};
    int n=WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0,nullptr,nullptr);
    std::string r(n,'\0');WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),r.data(),n,nullptr,nullptr);return r;
}
std::wstring wide(const std::string& s) {
    if(s.empty())return {};
    int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0);
    std::wstring r(n,L'\0');MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),r.data(),n);return r;
}
std::wstring winError(const std::wstring& action) {
    DWORD code=GetLastError();wchar_t* msg=nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,code,0,(LPWSTR)&msg,0,nullptr);
    std::wstring r=action+L": "+(msg?msg:tr(L"Windows error"))+L" ("+std::to_wstring(code)+L")";
    if(msg)LocalFree(msg);return r;
}
fs::path programDir(){std::vector<wchar_t>b(32768);GetModuleFileNameW(nullptr,b.data(),(DWORD)b.size());return fs::path(b.data()).parent_path();}
fs::path videosDir(){PWSTR p=nullptr;fs::path r;
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Videos,KF_FLAG_DEFAULT,nullptr,&p))){r=p;CoTaskMemFree(p);}
    if(r.empty()){wchar_t b[32768]{};GetEnvironmentVariableW(L"USERPROFILE",b,32768);r=fs::path(b)/L"Videos";}return r;
}
fs::path systemExe(const wchar_t* relative){wchar_t p[MAX_PATH]{};GetSystemDirectoryW(p,MAX_PATH);return fs::path(p)/relative;}
bool system64(){SYSTEM_INFO s{};GetNativeSystemInfo(&s);return s.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64||s.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_ARM64;}
bool windows10(){
    using Fn=LONG(WINAPI*)(OSVERSIONINFOW*);
    auto fn=(Fn)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlGetVersion");
    OSVERSIONINFOW v{};v.dwOSVersionInfoSize=sizeof(v);return fn&&fn(&v)==0&&v.dwMajorVersion>=10;
}
void ensureWritable(const fs::path& dir){
    fs::create_directories(dir);TempDir t(dir);auto p=t.path/L"test";std::ofstream o(p,std::ios::binary);
    if(!o)throw std::runtime_error(tr("No write access: ")+utf8(dir.wstring()));
}
json readJson(const fs::path& p,const json& fallback){try{std::ifstream i(p);if(!i)return fallback;json j;i>>j;return j;}catch(...){return fallback;}}
void writeJson(const fs::path& p,const json& v){
    auto tmp=p;tmp+=L".new";{std::ofstream o(tmp,std::ios::binary|std::ios::trunc);o<<v.dump(2);o.flush();if(!o)throw std::runtime_error(tr("Could not write settings"));}
    if(!MoveFileExW(tmp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error(utf8(winError(tr(L"Saving settings"))));
}
std::string sha256(const fs::path& p){
    BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error(tr("SHA256 unavailable"));
    DWORD size=0,got=0;BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,(PUCHAR)&size,sizeof(size),&got,0);
    std::vector<unsigned char> object(size);unsigned char digest[32]{};
    if(BCryptCreateHash(alg,&hash,object.data(),size,nullptr,0,0)<0){BCryptCloseAlgorithmProvider(alg,0);throw std::runtime_error(tr("SHA256 error"));}
    std::ifstream in(p,std::ios::binary);char b[65536];bool ok=bool(in);
    while(in){in.read(b,sizeof(b));if(in.gcount()&&BCryptHashData(hash,(PUCHAR)b,(ULONG)in.gcount(),0)<0)ok=false;}
    if(BCryptFinishHash(hash,digest,32,0)<0)ok=false;
    BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(alg,0);
    if(!ok||in.bad())throw std::runtime_error(tr("Could not verify SHA256"));
    std::ostringstream out;for(auto v:digest)out<<std::hex<<std::setw(2)<<std::setfill('0')<<(int)v;return out.str();
}
static void request(const std::wstring& url,std::atomic_bool& cancel,const std::function<void(const char*,DWORD,unsigned long long)>& sink,const ProxySettings& proxy){
    checkCancel(cancel);validateProxy(proxy);
    if(url.rfind(L"https://",0)!=0)throw std::runtime_error(tr("Components can only be downloaded over HTTPS"));
    static const CURLcode init=curl_global_init(CURL_GLOBAL_DEFAULT);
    if(init!=CURLE_OK)throw std::runtime_error(tr("Could not initialize networking"));
    std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> curl(curl_easy_init(),curl_easy_cleanup);
    if(!curl)throw std::runtime_error(tr("Could not open a connection"));
    struct Transfer{const std::function<void(const char*,DWORD,unsigned long long)>* sink;std::atomic_bool* cancel;CURL* curl;std::exception_ptr error;unsigned long long received=0;};
    Transfer t{&sink,&cancel,curl.get()};char error[CURL_ERROR_SIZE]{};
    auto address=utf8(url),proxyAddress=utf8(proxyUrl(proxy,false)),user=utf8(proxy.user),password=utf8(proxy.password);
    curl_easy_setopt(curl.get(),CURLOPT_URL,address.c_str());
    curl_easy_setopt(curl.get(),CURLOPT_USERAGENT,"MediaGrabby/0.2");
    curl_easy_setopt(curl.get(),CURLOPT_PROXY,proxyAddress.c_str());
    curl_easy_setopt(curl.get(),CURLOPT_NOPROXY,"");
    if(proxy.enabled){curl_easy_setopt(curl.get(),CURLOPT_PROXYUSERNAME,user.c_str());curl_easy_setopt(curl.get(),CURLOPT_PROXYPASSWORD,password.c_str());curl_easy_setopt(curl.get(),CURLOPT_PROXYAUTH,(long)CURLAUTH_BASIC);}
    curl_easy_setopt(curl.get(),CURLOPT_CONNECTTIMEOUT,15L);curl_easy_setopt(curl.get(),CURLOPT_LOW_SPEED_LIMIT,1L);curl_easy_setopt(curl.get(),CURLOPT_LOW_SPEED_TIME,30L);
    curl_easy_setopt(curl.get(),CURLOPT_FOLLOWLOCATION,1L);curl_easy_setopt(curl.get(),CURLOPT_MAXREDIRS,8L);
    curl_easy_setopt(curl.get(),CURLOPT_PROTOCOLS_STR,"https");curl_easy_setopt(curl.get(),CURLOPT_REDIR_PROTOCOLS_STR,"https");
    curl_easy_setopt(curl.get(),CURLOPT_FAILONERROR,1L);curl_easy_setopt(curl.get(),CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(curl.get(),CURLOPT_ERRORBUFFER,error);
    curl_easy_setopt(curl.get(),CURLOPT_NOPROGRESS,0L);curl_easy_setopt(curl.get(),CURLOPT_XFERINFODATA,&cancel);
    curl_easy_setopt(curl.get(),CURLOPT_XFERINFOFUNCTION,+[](void* p,curl_off_t,curl_off_t,curl_off_t,curl_off_t)->int{return ((std::atomic_bool*)p)->load()?1:0;});
    curl_easy_setopt(curl.get(),CURLOPT_WRITEDATA,&t);
    curl_easy_setopt(curl.get(),CURLOPT_WRITEFUNCTION,+[](char* b,size_t size,size_t count,void* context)->size_t{
        auto& t=*(Transfer*)context;size_t n=size*count;
        try{checkCancel(*t.cancel);t.received+=n;if(t.received>2ull*1024*1024*1024)throw std::runtime_error(tr("Component too large"));
            long status=0;curl_easy_getinfo(t.curl,CURLINFO_RESPONSE_CODE,&status);if(status>=300&&status<400)return n;
            curl_off_t total=0;curl_easy_getinfo(t.curl,CURLINFO_CONTENT_LENGTH_DOWNLOAD_T,&total);(*t.sink)(b,(DWORD)n,total>0?(unsigned long long)total:0);return n;
        }catch(...){t.error=std::current_exception();return 0;}
    });
    auto code=curl_easy_perform(curl.get());checkCancel(cancel);if(t.error)std::rethrow_exception(t.error);
    if(code!=CURLE_OK)throw std::runtime_error(utf8(friendlyError(wide(error[0]?error:curl_easy_strerror(code)))));
}
std::string httpTextUsing(const std::wstring& url,std::atomic_bool& c,const ProxySettings& proxy){std::string r;request(url,c,[&](const char*b,DWORD n,unsigned long long){
    if(r.size()+n>32*1024*1024)throw std::runtime_error(tr("Text response too large"));r.append(b,n);
},proxy);return r;}
std::string httpText(const std::wstring& url,std::atomic_bool& c){return httpTextUsing(url,c,getProxy());}
void httpDownload(const std::wstring& url,const fs::path& p,std::atomic_bool& c,const Progress& progress){
    std::ofstream out(p,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error(tr("Could not create the component file"));
    unsigned long long received=0;auto last=std::chrono::steady_clock::now();
    request(url,c,[&](const char*b,DWORD n,unsigned long long total){
        out.write(b,n);if(!out)throw std::runtime_error(tr("Insufficient disk space or no write access"));
        received+=n;auto now=std::chrono::steady_clock::now();
        if(now-last>std::chrono::milliseconds(150)||received==total){
            progress(tr(L"Downloaded ")+std::to_wstring(received/1024/1024)+tr(L" MB")+(total?tr(L" of ")+std::to_wstring(total/1024/1024)+tr(L" MB"):L""),total?(int)(received*100/total):-1);last=now;
        }
    },getProxy());
    out.flush();if(!out)throw std::runtime_error(tr("Could not write the component"));
}
int runProcess(const std::vector<std::wstring>& args,const fs::path& cwd,std::atomic_bool& cancel,const Log& log,unsigned timeout){
    checkCancel(cancel);if(args.empty())throw std::runtime_error(tr("Empty command"));
    SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE rh=nullptr,wh=nullptr;
    if(!CreatePipe(&rh,&wh,&sa,0))throw std::runtime_error(utf8(winError(tr(L"Output pipe"))));
    Handle read(rh),write(wh);SetHandleInformation(read.h,HANDLE_FLAG_INHERIT,0);
    Handle input(CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr));
    Handle job(CreateJobObjectW(nullptr,nullptr));if(!job.h)throw std::runtime_error(utf8(winError(tr(L"Process group"))));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!SetInformationJobObject(job.h,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))throw std::runtime_error(utf8(winError(tr(L"Process group"))));
    SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);std::vector<char> attrs(bytes);
    STARTUPINFOEXW si{};si.StartupInfo.cb=sizeof(si);si.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
    si.StartupInfo.hStdOutput=si.StartupInfo.hStdError=write.h;si.StartupInfo.hStdInput=input.h;
    si.lpAttributeList=(LPPROC_THREAD_ATTRIBUTE_LIST)attrs.data();
    if(!InitializeProcThreadAttributeList(si.lpAttributeList,1,0,&bytes))throw std::runtime_error(utf8(winError(tr(L"Launch"))));
    HANDLE inherited[]={write.h,input.h};
    if(!UpdateProcThreadAttribute(si.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr)){
        DeleteProcThreadAttributeList(si.lpAttributeList);throw std::runtime_error(utf8(winError(tr(L"Launch"))));
    }
    std::wstring line=commandLine(args);PROCESS_INFORMATION pi{};
    BOOL started=CreateProcessW(args.front().c_str(),line.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED|EXTENDED_STARTUPINFO_PRESENT,
        nullptr,cwd.empty()?nullptr:cwd.c_str(),&si.StartupInfo,&pi);
    DeleteProcThreadAttributeList(si.lpAttributeList);
    if(!started)throw std::runtime_error(utf8(winError(tr(L"Launching ")+args.front())));
    Handle process(pi.hProcess),thread(pi.hThread);
    if(!AssignProcessToJobObject(job.h,process.h)){TerminateProcess(process.h,1);throw std::runtime_error(utf8(winError(tr(L"Process group"))));}
    if(ResumeThread(thread.h)==(DWORD)-1){TerminateJobObject(job.h,1);throw std::runtime_error(utf8(winError(tr(L"Starting process"))));}
    CloseHandle(write.h);write.h=nullptr;
    std::string pending;bool stopped=false,timedout=false;auto begin=std::chrono::steady_clock::now();
    while(true){
        if(cancel.load()&&!stopped){TerminateJobObject(job.h,130);stopped=true;}
        if(timeout&&!stopped&&std::chrono::steady_clock::now()-begin>std::chrono::seconds(timeout)){TerminateJobObject(job.h,124);stopped=true;timedout=true;}
        DWORD available=0;
        if(PeekNamedPipe(read.h,nullptr,0,nullptr,&available,nullptr)&&available){
            char b[8192];DWORD got=0;
            if(ReadFile(read.h,b,(DWORD)std::min<size_t>(sizeof(b),available),&got,nullptr)&&got){
                pending.append(b,got);size_t pos;
                while((pos=pending.find_first_of("\r\n"))!=std::string::npos){if(pos)log(wide(pending.substr(0,pos)));pending.erase(0,pos+1);}
                if(pending.size()>32*1024*1024)throw std::runtime_error(tr("Output line too long"));
            }
        }else if(WaitForSingleObject(process.h,0)==WAIT_OBJECT_0)break;
        else std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if(!pending.empty())log(wide(pending));
    DWORD exitCode=1;GetExitCodeProcess(process.h,&exitCode);
    if(timedout)throw std::runtime_error(tr("Component timed out"));
    checkCancel(cancel);return (int)exitCode;
}
static std::wstring psQuote(std::wstring s){size_t p=0;while((p=s.find(L"'",p))!=s.npos){s.insert(p,L"'");p+=2;}return L"'"+s+L"'";}
static std::wstring b64(const std::wstring& s){
    static const wchar_t* alphabet=L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const auto* bytes=(const unsigned char*)s.data();size_t count=s.size()*sizeof(wchar_t);std::wstring r;
    for(size_t i=0;i<count;i+=3){unsigned v=(unsigned)bytes[i]<<16;if(i+1<count)v|=(unsigned)bytes[i+1]<<8;if(i+2<count)v|=bytes[i+2];
        r+=alphabet[(v>>18)&63];r+=alphabet[(v>>12)&63];r+=i+1<count?alphabet[(v>>6)&63]:L'=';r+=i+2<count?alphabet[v&63]:L'=';
    }return r;
}
void extractZip(const fs::path& archive,const fs::path& dest,const std::vector<std::wstring>& names,std::atomic_bool& cancel,const Log& log){
    fs::create_directories(dest);
    std::wstring script=L"$ErrorActionPreference='Stop'; $ProgressPreference='SilentlyContinue'; [Console]::OutputEncoding=[Text.Encoding]::UTF8; Add-Type -AssemblyName System.IO.Compression.FileSystem; $z=[IO.Compression.ZipFile]::OpenRead("+psQuote(archive.wstring())+L"); try {";
    for(const auto& name:names){
        script+=L"$e=@($z.Entries | Where-Object { $_.Name -ceq "+psQuote(name)+L" }); if($e.Count -ne 1){throw 'Missing or duplicate archive entry'}; if($e[0].Length -gt 1073741824){throw 'Entry too large'}; [IO.Compression.ZipFileExtensions]::ExtractToFile($e[0],"+psQuote((dest/name).wstring())+L",$false);";
    }
    script+=L"} finally { $z.Dispose() }";
    if(runProcess({systemExe(L"WindowsPowerShell\\v1.0\\powershell.exe").wstring(),L"-NoLogo",L"-NoProfile",L"-NonInteractive",L"-EncodedCommand",b64(script)},dest,cancel,log,180)!=0)
        throw std::runtime_error(tr("Could not extract the component"));
}
void installDirectory(const fs::path& stage,const fs::path& target){
    auto backup=target;backup+=L".previous";std::error_code ec;fs::remove_all(backup,ec);
    bool old=fs::exists(target);if(old)fs::rename(target,backup);
    try{fs::rename(stage,target);}catch(...){if(old)fs::rename(backup,target);throw;}
    fs::remove_all(backup,ec);
}
TempDir::TempDir(const fs::path& parent){
    GUID g{};if(FAILED(CoCreateGuid(&g)))throw std::runtime_error(tr("Could not create the working folder"));
    wchar_t id[64]{};StringFromGUID2(g,id,64);path=parent/(L"job-"+std::wstring(id));fs::create_directories(path);
}
TempDir::~TempDir(){std::error_code ec;fs::remove_all(path,ec);}
}

