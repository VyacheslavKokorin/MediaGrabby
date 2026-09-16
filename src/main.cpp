#include "i18n.hpp"
#include "download.hpp"
#include "proxy.hpp"
#include "appearance.hpp"
#include "terminal.hpp"
#include "resource.h"
#include <richedit.h>
#include <windowsx.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <shlobj.h>
#include <shellapi.h>
#include <objbase.h>
#include <fstream>
#include <thread>
#include <mutex>
#include <memory>
#include <map>
using namespace mg;
namespace {
constexpr UINT WM_EVENT=WM_APP+1;
enum{URL=101,FOLDER,BROWSE,PASTE,QUALITY,AUDIO,VCODEC,ACODEC,BROWSER,UNUSED_110,METADATA,THEME,START,STOP,OPEN_FOLDER,OPEN_FILE,UPDATE_YT,UPDATE_FF,UPDATE_JS,SPOILER,STATUS,PROGRESS,TERMINAL,VERSION_YT,VERSION_FF,VERSION_JS,PROXY_BUTTON,COPY_LOG,CLEAR_LOG};
struct Event{int kind;std::wstring text;int progress=-1;};
struct App {
    HWND window=nullptr;std::map<int,HWND> controls;TerminalLog terminal;
    struct LabelPos{HWND h;int x,y,w,height;std::wstring source;};std::vector<LabelPos> labels;int scrollY=0;bool layingOut=false;int wheelRemainder=0;HFONT font=nullptr,titleFont=nullptr,smallFont=nullptr,monoFont=nullptr;
    HBRUSH bgBrush=nullptr,fieldBrush=nullptr,cardBrush=nullptr;Appearance appearance;Palette palette=paletteFor(appearance);
    std::map<int,std::wstring> captions;std::map<int,std::vector<std::wstring>> comboValues;bool retryShown=false;
    COLORREF bg=RGB(18,22,30),field=RGB(29,35,47),fg=RGB(231,236,245),muted=RGB(156,169,190),accent=RGB(94,130,255);
    fs::path root=programDir(),lastFile;ComponentManager cm{root};Options options,lastAttempt;bool canRetry=false,proxyLoadError=false;
    std::atomic_bool cancel{false};std::thread worker;bool busy=false,closing=false,expanded=false,dark=true,preview=false;
    std::mutex logMutex;std::ofstream logfile;int dpi=96;bool starting=true;
    int px(int x)const{return MulDiv(x,dpi,96);}
    HWND get(int id)const{auto i=controls.find(id);return i==controls.end()?nullptr:i->second;}
    std::wstring text(int id)const{int n=GetWindowTextLengthW(get(id));std::wstring r(n+1,L'\0');GetWindowTextW(get(id),r.data(),n+1);r.resize(n);return r;}
    int selected(int id)const{return (int)SendMessageW(get(id),CB_GETCURSEL,0,0);}
    bool checked(int id)const{return SendMessageW(get(id),BM_GETCHECK,0,0)==BST_CHECKED;}
    void post(int kind,const std::wstring& value,int pc=-1){auto* e=new Event{kind,value,pc};if(!PostMessageW(window,WM_EVENT,0,(LPARAM)e))delete e;}
    void log(const std::wstring& value){
        auto s=redactProxy(value,getProxy());
        {std::lock_guard<std::mutex> lock(logMutex);if(logfile){logfile<<utf8(s)<<"\n";logfile.flush();}}
        post(0,s);
    }
    void status(const std::wstring& s,int pc=-1){post(1,redactProxy(s,getProxy()),pc);}
    void place(int id,int x,int y,int w,int h){MoveWindow(get(id),px(x),px(y-scrollY),px(w),px(h),TRUE);}
    HWND add(const wchar_t* cls,const wchar_t* value,int id,DWORD style=0){
        captions[id]=sourceText(value);
        HWND h=CreateWindowExW(0,cls,tr(value),WS_CHILD|WS_VISIBLE|style,0,0,0,0,window,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
        SendMessageW(h,WM_SETFONT,(WPARAM)font,TRUE);controls[id]=h;skinControl(h,&palette);return h;
    }
    HWND label(const wchar_t* value,int x,int y,int w,int h=20,bool compact=false){
        HWND c=CreateWindowW(L"STATIC",tr(value),WS_CHILD|WS_VISIBLE,px(x),px(y),px(w),px(h),window,nullptr,GetModuleHandleW(nullptr),nullptr);
        SendMessageW(c,WM_SETFONT,(WPARAM)(compact?smallFont:font),TRUE);labels.push_back({c,x,y,w,h,sourceText(value)});return c;
    }
    void combo(int id,std::initializer_list<const wchar_t*> values,int selection){
        auto h=add(L"COMBOBOX",L"",id,WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL);
        for(auto value:values){comboValues[id].push_back(sourceText(value));SendMessageW(h,CB_ADDSTRING,0,(LPARAM)tr(value));}
        SendMessageW(h,CB_SETCURSEL,selection,0);
        SendMessageW(h,CB_SETITEMHEIGHT,(WPARAM)-1,px(25));
        SetWindowSubclass(h,noWheelCombo,1,0);
        COMBOBOXINFO ci{sizeof(ci)};if(GetComboBoxInfo(h,&ci)&&ci.hwndList)SetWindowSubclass(ci.hwndList,noWheelCombo,2,0);
    }
    void button(int id,const wchar_t* value){add(L"BUTTON",value,id,WS_TABSTOP|BS_OWNERDRAW);}
    void load(){
        auto j=readJson(root/L"settings.json");
        try{
            options.folder=wide(j.value("folder",utf8(videosDir().wstring())));
            options.quality=std::clamp(j.value("quality",0),0,4);
            options.browser=std::clamp(j.value("browser",0),0,8);
            options.videoCodec=std::clamp(j.value("videoCodec",0),0,2);
            options.audioCodec=std::clamp(j.value("audioCodec",0),0,1);
            options.theme=std::clamp(j.value("theme",0),0,2);
            options.audioOnly=j.value("audioOnly",false);options.metadata=j.value("metadata",true);
        }catch(...){options=Options{};options.folder=videosDir().wstring();}
        if(options.folder.empty())options.folder=videosDir().wstring();
    }
    Options collect(){
        Options o;o.url=trim(text(URL));o.folder=trim(text(FOLDER));o.quality=selected(QUALITY);o.browser=selected(BROWSER);
        o.videoCodec=selected(VCODEC);o.audioCodec=selected(ACODEC);o.theme=appearance.theme;
        o.audioOnly=checked(AUDIO);o.metadata=checked(METADATA);return o;
    }
    void save(){
        auto o=collect();
        writeJson(root/L"settings.json",{{"folder",utf8(o.folder)},{"quality",o.quality},{"browser",o.browser},
            {"videoCodec",o.videoCodec},{"audioCodec",o.audioCodec},{"theme",o.theme},{"audioOnly",o.audioOnly},{"metadata",o.metadata}});
    }
    void retranslate(){
        for(auto& label:labels)SetWindowTextW(label.h,tr(label.source.c_str()));
        for(auto& [id,caption]:captions)if(!caption.empty()&&id!=URL&&id!=FOLDER&&id!=STATUS)SetWindowTextW(get(id),tr(caption.c_str()));
        for(auto& [id,values]:comboValues){int selection=selected(id);SendMessageW(get(id),CB_RESETCONTENT,0,0);for(auto& value:values)SendMessageW(get(id),CB_ADDSTRING,0,(LPARAM)tr(value.c_str()));SendMessageW(get(id),CB_SETCURSEL,selection,0);}
        SetWindowTextW(get(SPOILER),tr(expanded?L"▾ Terminal":L"▸ Terminal"));
        SetWindowTextW(get(START),tr(retryShown?L"Retry":L"Download"));
        if(!busy)SetWindowTextW(get(STATUS),tr(proxyLoadError?L"Open proxy settings and enter your password again.":L"Ready to download"));
    }
    void applyTheme(){
        palette=paletteFor(appearance);dark=palette.dark;bg=palette.bg;field=palette.field;fg=palette.fg;muted=palette.muted;accent=palette.accent;
        if(bgBrush)DeleteObject(bgBrush);if(fieldBrush)DeleteObject(fieldBrush);if(cardBrush)DeleteObject(cardBrush);cardBrush=CreateSolidBrush(palette.card);bgBrush=CreateSolidBrush(bg);fieldBrush=CreateSolidBrush(field);
        SendMessageW(get(TERMINAL),EM_SETBKGNDCOLOR,0,field);
        CHARFORMAT2W format{};format.cbSize=sizeof(format);format.dwMask=CFM_COLOR;format.crTextColor=fg;SendMessageW(get(TERMINAL),EM_SETCHARFORMAT,SCF_ALL,(LPARAM)&format);
        BOOL isDark=dark;DwmSetWindowAttribute(window,20,&isDark,sizeof(isDark));
        SendMessageW(get(PROGRESS),PBM_SETBARCOLOR,0,accent);SendMessageW(get(PROGRESS),PBM_SETBKCOLOR,0,field);
        RedrawWindow(window,nullptr,nullptr,RDW_ERASE|RDW_INVALIDATE|RDW_FRAME|RDW_ALLCHILDREN);
    }
    void paint(HDC dc){
        RECT r;GetClientRect(window,&r);FillRect(dc,&r,bgBrush);int w=MulDiv(r.right,96,dpi);
        auto card=[&](int y,int height){RECT box{px(16),px(y-scrollY),px(w-16),px(y+height-scrollY)};
            if(appearance.style==2){auto pen=CreatePen(PS_SOLID,1,palette.border);auto old=SelectObject(dc,pen);MoveToEx(dc,box.left,box.bottom,nullptr);LineTo(dc,box.right,box.bottom);SelectObject(dc,old);DeleteObject(pen);}
            else rounded(dc,box,palette.card,palette.border,px(palette.radius));};
        card(98,140);card(250,214);card(590,77);if(expanded)card(682,274);
        auto logo=(HICON)LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDI_MEDIAGRABBY),IMAGE_ICON,px(40),px(40),LR_SHARED);
        DrawIconEx(dc,px(27),px(27-scrollY),logo,px(40),px(40),0,nullptr,DI_NORMAL);
    }
    void refresh(){
        for(int id:{URL,FOLDER,BROWSE,PASTE,QUALITY,AUDIO,VCODEC,ACODEC,BROWSER,UNUSED_110,METADATA,START,UPDATE_YT,UPDATE_FF,UPDATE_JS,PROXY_BUTTON})EnableWindow(get(id),!busy);
        EnableWindow(get(STOP),busy&&!cancel.load());
        if(proxyLoadError)for(int id:{START,UPDATE_YT,UPDATE_FF,UPDATE_JS})EnableWindow(get(id),FALSE);
        SetWindowTextW(get(PROXY_BUTTON),tr(getProxy().enabled?L"Settings • Proxy":L"Settings"));
        bool sound=checked(AUDIO);
        if(!busy){EnableWindow(get(QUALITY),!sound);EnableWindow(get(VCODEC),!sound);EnableWindow(get(ACODEC),!sound);}
        EnableWindow(get(OPEN_FILE),!lastFile.empty()&&fs::is_regular_file(lastFile));
        if(!busy){
            SetWindowTextW(get(VERSION_YT),(L"yt-dlp  "+cm.version("ytdlp")).c_str());
            SetWindowTextW(get(VERSION_FF),(L"FFmpeg  "+cm.version("ffmpeg")).c_str());
            SetWindowTextW(get(VERSION_JS),(L"Node.js  "+cm.version("node")).c_str());
        }
        InvalidateRect(window,nullptr,TRUE);
    }
    void create(){
        HDC dc=GetDC(window);dpi=GetDeviceCaps(dc,LOGPIXELSX);ReleaseDC(window,dc);
        font=CreateFontW(-px(14),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        titleFont=CreateFontW(-px(29),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        smallFont=CreateFontW(-px(12),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        monoFont=CreateFontW(-px(12),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,FIXED_PITCH,L"Consolas");
        load();appearance=loadAppearance(root,options.theme);uiLanguage=appearance.language;
        try{setProxy(loadProxy(root/L"proxy.json"));}catch(const std::exception& e){proxyLoadError=true;MessageBoxW(window,wide(e.what()).c_str(),tr(L"Proxy settings"),MB_ICONERROR);}
        auto title=label(L"MediaGrabby",82,20,350,40);SendMessageW(title,WM_SETFONT,(WPARAM)titleFont,TRUE);
        label(tr(L"Video and music from a link"),83,62,400,22,true);
        button(PROXY_BUTTON,tr(L"Settings"));
        label(tr(L"Video or playlist URL"),28,108,440);
        add(L"EDIT",L"",URL,WS_TABSTOP|ES_AUTOHSCROLL);SendMessageW(get(URL),EM_SETCUEBANNER,TRUE,(LPARAM)L"https://…");SendMessageW(get(URL),EM_SETLIMITTEXT,16384,0);
        button(PASTE,tr(L"Paste"));
        label(tr(L"Save folder"),28,175,420);add(L"EDIT",options.folder.c_str(),FOLDER,WS_TABSTOP|ES_AUTOHSCROLL);button(BROWSE,tr(L"Browse…"));
        label(tr(L"Video quality"),28,262,260);combo(QUALITY,{tr(L"Best quality"),L"1080p",L"720p",L"480p",L"360p"},options.quality);
        add(L"BUTTON",tr(L"Audio only — MP3"),AUDIO,WS_TABSTOP|BS_AUTOCHECKBOX);SendMessageW(get(AUDIO),BM_SETCHECK,options.audioOnly?BST_CHECKED:BST_UNCHECKED,0);
        label(tr(L"Video codec"),330,262,260);combo(VCODEC,{tr(L"Original"),L"H.264",L"H.265 / HEVC"},options.videoCodec);
        combo(ACODEC,{tr(L"Original"),L"AAC"},options.audioCodec);
        label(tr(L"Browser cookies"),28,370,350);combo(BROWSER,{tr(L"No browser — no cookies"),L"Google Chrome",L"Microsoft Edge",L"Mozilla Firefox",L"Brave",L"Opera",L"Vivaldi",L"Chromium",L"Whale"},options.browser);
        add(L"BUTTON",tr(L"Embed metadata and thumbnail"),METADATA,WS_TABSTOP|BS_AUTOCHECKBOX);
        SendMessageW(get(METADATA),BM_SETCHECK,options.metadata?BST_CHECKED:BST_UNCHECKED,0);
        button(START,tr(L"Download"));button(STOP,tr(L"Stop"));button(OPEN_FOLDER,tr(L"Open folder"));button(OPEN_FILE,tr(L"Open file"));
        add(L"STATIC",tr(L"Getting ready…"),STATUS,SS_LEFT);
        add(PROGRESS_CLASSW,L"",PROGRESS,0);SendMessageW(get(PROGRESS),PBM_SETRANGE32,0,100);
        add(L"STATIC",L"",VERSION_YT);add(L"STATIC",L"",VERSION_FF);add(L"STATIC",L"",VERSION_JS);
        for(int id:{VERSION_YT,VERSION_FF,VERSION_JS})SendMessageW(get(id),WM_SETFONT,(WPARAM)smallFont,TRUE);
        button(UPDATE_YT,tr(L"Update yt-dlp"));button(UPDATE_FF,tr(L"Update FFmpeg"));button(UPDATE_JS,tr(L"Update Node.js"));
        button(SPOILER,tr(L"▸ Terminal"));button(COPY_LOG,tr(L"Copy"));button(CLEAR_LOG,tr(L"Clear"));
        add(MSFTEDIT_CLASS,L"",TERMINAL,ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL|WS_TABSTOP);
        terminal.attach(get(TERMINAL));SendMessageW(get(TERMINAL),EM_SETTARGETDEVICE,0,0);
        SendMessageW(get(TERMINAL),WM_SETFONT,(WPARAM)monoFont,TRUE);SendMessageW(get(TERMINAL),EM_SETLIMITTEXT,2000000,0);ShowWindow(get(TERMINAL),SW_HIDE);
        // The right-hand labels move with the columns when the window is resized.
        label(tr(L"Audio codec"),630,262,260);
        try{
            ensureWritable(root);fs::create_directories(root/L"logs");
            SYSTEMTIME t{};GetLocalTime(&t);wchar_t name[80];swprintf_s(name,L"%04d%02d%02d-%02d%02d%02d-%lu.log",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,GetCurrentProcessId());
            logfile.open(root/L"logs"/name,std::ios::binary);
        }catch(const std::exception& e){MessageBoxW(window,(tr(L"Extract the application into a writable folder.\n")+wide(e.what())).c_str(),L"MediaGrabby",MB_ICONERROR);}
        retranslate();applyTheme();layout();refresh();
        if(preview){SetWindowTextW(get(STATUS),tr(L"Ready to download"));starting=false;}
        else if(proxyLoadError){SetWindowTextW(get(STATUS),tr(L"Open proxy settings and enter your password again."));starting=false;}else SetTimer(window,1,100,nullptr);
    }
    void layout(){
        if(layingOut)return;layingOut=true;
        RECT r;GetClientRect(window,&r);int w=MulDiv(r.right,96,dpi),h=MulDiv(r.bottom,96,dpi);
        int content=expanded?972:734;scrollY=std::clamp(scrollY,0,std::max(0,content-h));
        SCROLLINFO si{sizeof(si),SIF_RANGE|SIF_PAGE|SIF_POS|SIF_DISABLENOSCROLL};si.nMin=0;si.nMax=content-1;si.nPage=h;si.nPos=scrollY;SetScrollInfo(window,SB_VERT,&si,TRUE);
        int cw=(w-80)/3;
        for(auto& p:labels){int x=p.x,width=p.w;
            if(p.source==L"Video codec"){x=40+cw;width=cw;}
            if(p.source==L"Audio codec"){x=52+2*cw;width=cw;}
            MoveWindow(p.h,px(x),px(p.y-scrollY),px(width),px(p.height),TRUE);
        }
        place(PROXY_BUTTON,w-225,29,197,38);
        place(URL,28,133,w-176,33);place(PASTE,w-136,133,108,33);
        place(FOLDER,28,200,w-176,29);place(BROWSE,w-136,196,108,34);
        place(QUALITY,28,287,cw,240);place(VCODEC,40+cw,287,cw,220);place(ACODEC,52+2*cw,287,cw,200);
        place(AUDIO,28,330,w-56,28);place(BROWSER,28,395,w-56,290);place(METADATA,28,431,w-56,26);
        int bw=(w-92)/4;place(START,28,480,bw,42);place(STOP,40+bw,480,bw,42);place(OPEN_FOLDER,52+2*bw,480,bw,42);place(OPEN_FILE,64+3*bw,480,bw,42);
        place(STATUS,28,536,w-56,30);place(PROGRESS,28,572,w-56,7);
        place(VERSION_YT,28,602,cw,18);place(VERSION_FF,40+cw,602,cw,18);place(VERSION_JS,52+2*cw,602,cw,18);
        place(UPDATE_YT,28,627,cw,29);place(UPDATE_FF,40+cw,627,cw,29);place(UPDATE_JS,52+2*cw,627,cw,29);
        place(SPOILER,28,688,w-318,30);place(COPY_LOG,w-274,688,137,30);place(CLEAR_LOG,w-125,688,97,30);
        if(expanded){place(TERMINAL,28,730,w-56,std::max(220,h-750));ShowWindow(get(TERMINAL),SW_SHOW);}
        else ShowWindow(get(TERMINAL),SW_HIDE);
        EnableWindow(get(COPY_LOG),expanded);EnableWindow(get(CLEAR_LOG),expanded);layingOut=false;InvalidateRect(window,nullptr,FALSE);
    }
    void scrollTo(int next){
        SCROLLINFO si{sizeof(si),SIF_ALL};GetScrollInfo(window,SB_VERT,&si);
        next=std::clamp(next,0,std::max(0,si.nMax-(int)si.nPage+1));if(next==scrollY)return;
        int dy=px(scrollY-next);scrollY=next;SetScrollPos(window,SB_VERT,next,TRUE);
        HDWP batch=BeginDeferWindowPos((int)(controls.size()+labels.size()));
        auto move=[&](HWND child){RECT r;GetWindowRect(child,&r);MapWindowPoints(nullptr,window,(POINT*)&r,2);batch=DeferWindowPos(batch,child,nullptr,r.left,r.top+dy,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);};
        for(auto [id,h]:controls)move(h);for(auto& l:labels)move(l.h);if(batch)EndDeferWindowPos(batch);
        RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
    }
    void wheel(WPARAM wp,LPARAM lp){
        POINT point{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
        RECT r;GetWindowRect(get(TERMINAL),&r);
        if(expanded&&PtInRect(&r,point)){SendMessageW(get(TERMINAL),EM_LINESCROLL,0,-GET_WHEEL_DELTA_WPARAM(wp)/WHEEL_DELTA*3);return;}
        wheelRemainder+=GET_WHEEL_DELTA_WPARAM(wp);int ticks=wheelRemainder/WHEEL_DELTA;wheelRemainder%=WHEEL_DELTA;
        if(ticks)scrollTo(scrollY-ticks*56);
    }
    void async(std::function<void()> fn){
        if(busy)return;if(worker.joinable())worker.join();cancel=false;busy=true;refresh();
        worker=std::thread([this,fn]{
            try{fn();}
            catch(const Cancelled&){log(tr(L"Operation stopped. Incomplete work removed."));status(tr(L"Stopped"),-2);}
            catch(const std::exception& e){log(tr(L"ERROR: ")+wide(e.what()));status(friendlyError(wide(e.what())),-2);}
            post(2,L"");
        });
    }
    void startDownload(bool retry=false){
        auto o=retry&&canRetry?lastAttempt:collect();
        if(!validUrl(o.url)){MessageBoxW(window,tr(L"Enter a complete video or playlist URL."),L"MediaGrabby",MB_ICONINFORMATION);SetFocus(get(URL));return;}
        if(o.folder.empty()){MessageBoxW(window,tr(L"Choose a save folder."),L"MediaGrabby",MB_ICONINFORMATION);return;}
        try{save();}catch(const std::exception& e){MessageBoxW(window,wide(e.what()).c_str(),L"MediaGrabby",MB_ICONERROR);return;}
        lastAttempt=o;canRetry=true;retryShown=false;SetWindowTextW(get(START),tr(L"Download"));
        async([this,o]{
            auto r=download(o,cm,cancel,[this](auto s){log(s);},[this](auto s,int p){status(s,p);},[this](auto p){post(3,p.wstring());});
            status(r.failed?tr(L"Errors: ")+std::to_wstring(r.failed)+L". "+r.lastError:tr(L"Done. Downloaded: ")+std::to_wstring(r.completed)+tr(L" · Skipped: ")+std::to_wstring(r.skipped),r.failed?-2:100);
        });
    }
    void update(const std::vector<std::string>& ids){
        async([this,ids]{
            int errors=0;
            for(const auto& id:ids)try{cm.install(id,true,cancel,[this](auto s){log(s);},[this](auto s,int p){status(s,p);});}
                catch(const Cancelled&){throw;}catch(const std::exception& e){++errors;log(tr(L"ERROR: ")+wide(e.what()));}
            status(errors?tr(L"Update finished with errors. See the terminal for details."):tr(L"Components updated. Ready."),errors?-2:100);
        });
    }
    void chooseFolder(){
        IFileDialog* dialog=nullptr;
        if(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))){
            DWORD flags=0;dialog->GetOptions(&flags);dialog->SetOptions(flags|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM);dialog->SetTitle(tr(L"Save folder"));
            IShellItem* initial=nullptr;if(SUCCEEDED(SHCreateItemFromParsingName(text(FOLDER).c_str(),nullptr,IID_PPV_ARGS(&initial)))){dialog->SetFolder(initial);initial->Release();}
            if(SUCCEEDED(dialog->Show(window))){
                IShellItem* item=nullptr;if(SUCCEEDED(dialog->GetResult(&item))){PWSTR p=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&p))){SetWindowTextW(get(FOLDER),p);CoTaskMemFree(p);}item->Release();}
            }dialog->Release();
        }
    }
    void shellOpen(const fs::path& path){
        if(path.empty()||!fs::exists(path)){MessageBoxW(window,tr(L"The file or folder does not exist yet."),L"MediaGrabby",MB_ICONINFORMATION);return;}
        if((INT_PTR)ShellExecuteW(window,L"open",path.c_str(),nullptr,nullptr,SW_SHOWNORMAL)<=32)
            MessageBoxW(window,tr(L"Windows could not open the file or folder. Check your default application."),L"MediaGrabby",MB_ICONERROR);
    }
    void event(Event& e){
        if(e.kind==0){
            terminal.append(e.text);
        }else if(e.kind==1){
            SetWindowTextW(get(STATUS),e.text.c_str());
            if(e.progress==-2&&canRetry){retryShown=true;SetWindowTextW(get(START),tr(L"Retry"));}
            LONG_PTR style=GetWindowLongPtrW(get(PROGRESS),GWL_STYLE);
            SetWindowLongPtrW(get(PROGRESS),GWL_STYLE,e.progress==-1?style|PBS_MARQUEE:style&~PBS_MARQUEE);
            SendMessageW(get(PROGRESS),PBM_SETMARQUEE,e.progress==-1,40);SendMessageW(get(PROGRESS),PBM_SETPOS,std::max(0,e.progress),0);
        }else if(e.kind==2){
            if(worker.joinable())worker.join();busy=false;starting=false;refresh();
            if(closing){try{save();}catch(...){}DestroyWindow(window);}
        }else if(e.kind==3){lastFile=e.text;EnableWindow(get(OPEN_FILE),TRUE);}
    }
    void command(int id,int notification){
        (void)notification;
        if(id==COPY_LOG){
            auto value=text(TERMINAL);if(OpenClipboard(window)){EmptyClipboard();auto h=GlobalAlloc(GMEM_MOVEABLE,(value.size()+1)*sizeof(wchar_t));
                if(h){auto p=GlobalLock(h);if(p){memcpy(p,value.c_str(),(value.size()+1)*sizeof(wchar_t));GlobalUnlock(h);if(!SetClipboardData(CF_UNICODETEXT,h))GlobalFree(h);}else GlobalFree(h);}CloseClipboard();}return;
        }
        if(id==CLEAR_LOG){terminal.clear();return;}
        if(id==PROXY_BUTTON&&!busy){if(showSettingsDialog(window,root,appearance)){proxyLoadError=false;uiLanguage=appearance.language;retranslate();applyTheme();layout();refresh();}return;}
        if(id==AUDIO){refresh();return;}
        if(id==STOP){cancel=true;SetWindowTextW(get(STATUS),tr(L"Stopping and removing incomplete files…"));refresh();return;}
        if(id==SPOILER){
            expanded=!expanded;SetWindowTextW(get(SPOILER),expanded?tr(L"▾ Terminal"):tr(L"▸ Terminal"));
            RECT r;GetWindowRect(window,&r);int height=expanded?px(1030):px(784);
            MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&mi);
            // Keep the entire form reachable on short screens using the window scrollbar.
            int top=std::max<int>(mi.rcWork.top,r.top-(expanded?px(120):0));
            SetWindowPos(window,nullptr,r.left,top,r.right-r.left,std::min<int>(height,mi.rcWork.bottom-top),SWP_NOZORDER);if(expanded){RECT client;GetClientRect(window,&client);scrollY=std::max(0,972-MulDiv(client.bottom,96,dpi));}else scrollY=0;layout();return;
        }
        if(id==OPEN_FOLDER){shellOpen(!lastFile.empty()?lastFile.parent_path():fs::path(text(FOLDER)));return;}
        if(id==OPEN_FILE){shellOpen(lastFile);return;}
        if(busy)return;
        if(id==START)startDownload(retryShown);
        else if(id==BROWSE)chooseFolder();
        else if(id==PASTE&&OpenClipboard(window)){
            if(HANDLE h=GetClipboardData(CF_UNICODETEXT)){auto p=(wchar_t*)GlobalLock(h);if(p){SetWindowTextW(get(URL),trim(p).c_str());GlobalUnlock(h);}}CloseClipboard();
        }else if(id==UPDATE_YT)update({"ytdlp"});
        else if(id==UPDATE_FF)update({"ffmpeg"});
        else if(id==UPDATE_JS)update({"node"});
    }
    ~App(){cancel=true;if(worker.joinable())worker.join();for(auto h:{font,titleFont,smallFont,monoFont})if(h)DeleteObject(h);if(bgBrush)DeleteObject(bgBrush);if(fieldBrush)DeleteObject(fieldBrush);if(cardBrush)DeleteObject(cardBrush);}
};
LRESULT CALLBACK wndproc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
    auto* a=(App*)GetWindowLongPtrW(h,GWLP_USERDATA);
    if(msg==WM_NCCREATE){a=(App*)((CREATESTRUCTW*)lp)->lpCreateParams;a->window=h;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)a);}
    if(!a)return DefWindowProcW(h,msg,wp,lp);
    switch(msg){
    case WM_CREATE:a->create();return 0;
    case WM_SIZE:if(a->get(URL))a->layout();return 0;
    case WM_GETMINMAXINFO:{auto* m=(MINMAXINFO*)lp;m->ptMinTrackSize={a->px(820),a->px(620)};return 0;}
    case WM_VSCROLL:{
        SCROLLINFO si{sizeof(si),SIF_ALL};GetScrollInfo(h,SB_VERT,&si);int p=a->scrollY;
        switch(LOWORD(wp)){case SB_LINEUP:p-=28;break;case SB_LINEDOWN:p+=28;break;case SB_PAGEUP:p-=(int)si.nPage;break;case SB_PAGEDOWN:p+=(int)si.nPage;break;case SB_THUMBTRACK:p=si.nTrackPos;break;}
        a->scrollTo(p);return 0;
    }
    case WM_MOUSEWHEEL:a->wheel(wp,lp);return 0;
    case WM_MOUSEHWHEEL:return 0;
    case WM_TIMER:if(wp==1){KillTimer(h,1);a->async([a]{a->cm.prepare(a->cancel,[a](auto s){a->log(s);},[a](auto s,int p){a->status(s,p);});a->status(tr(L"Ready to download"),0);});}return 0;
    case WM_COMMAND:a->command(LOWORD(wp),HIWORD(wp));return 0;
    case WM_EVENT:{std::unique_ptr<Event>e((Event*)lp);a->event(*e);return 0;}
    case WM_SETTINGCHANGE:if(a->appearance.theme==0)a->applyTheme();return 0;
    case WM_CTLCOLORSTATIC:case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:case WM_CTLCOLORBTN:{
        HDC dc=(HDC)wp;HWND child=(HWND)lp;wchar_t cls[32]{};GetClassNameW(child,cls,32);
        bool field=wcscmp(cls,L"Edit")==0||wcscmp(cls,L"ComboBox")==0||msg==WM_CTLCOLORLISTBOX;
        SetTextColor(dc,IsWindowEnabled(child)?a->fg:a->muted);
        RECT childRect;GetWindowRect(child,&childRect);MapWindowPoints(nullptr,h,(POINT*)&childRect,2);int y=MulDiv(childRect.top,96,a->dpi)+a->scrollY;
        bool card=!field&&((y>=98&&y<464)||(y>=590&&y<667));SetBkColor(dc,field?a->field:card?a->palette.card:a->bg);
        if(card)return (LRESULT)a->cardBrush;
        return (LRESULT)(field?a->fieldBrush:a->bgBrush);
    }
    case WM_ERASEBKGND:a->paint((HDC)wp);return 1;
    case WM_PAINT:{PAINTSTRUCT ps;auto dc=BeginPaint(h,&ps);a->paint(dc);EndPaint(h,&ps);return 0;}
    case WM_DRAWITEM:{
        auto* d=(DRAWITEMSTRUCT*)lp;if(d->CtlType!=ODT_BUTTON)break;
        drawButton(*d,a->palette,d->CtlID==START);return TRUE;
    }
    case WM_CLOSE:
        if(a->busy){a->closing=true;a->cancel=true;SetWindowTextW(a->get(STATUS),tr(L"Stopping before closing…"));a->refresh();return 0;}
        try{a->save();}catch(...){}DestroyWindow(h);return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(h,msg,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR command,int show){
    LoadLibraryW(L"Msftedit.dll");SetProcessDPIAware();CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(!windows10()){MessageBoxW(nullptr,tr(L"MediaGrabby requires Windows 10 or Windows 11."),L"MediaGrabby",MB_ICONERROR);CoUninitialize();return 1;}
    INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_STANDARD_CLASSES|ICC_PROGRESS_CLASS};InitCommonControlsEx(&ic);
    App app;app.preview=std::wstring(command).find(L"--preview")!=std::wstring::npos;
    WNDCLASSEXW wc{sizeof(wc)};wc.lpfnWndProc=wndproc;wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc.hIcon=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(IDI_MEDIAGRABBY),IMAGE_ICON,GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON),LR_SHARED);
    wc.hIconSm=(HICON)LoadImageW(instance,MAKEINTRESOURCEW(IDI_MEDIAGRABBY),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED);wc.lpszClassName=L"MediaGrabbyWindow";
    RegisterClassExW(&wc);
    HDC screen=GetDC(nullptr);app.dpi=GetDeviceCaps(screen,LOGPIXELSY);ReleaseDC(nullptr,screen);
    RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    HWND window=CreateWindowExW(WS_EX_CONTROLPARENT|WS_EX_COMPOSITED,wc.lpszClassName,L"MediaGrabby",WS_OVERLAPPEDWINDOW|WS_VSCROLL|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,std::min(app.px(1020),(int)(work.right-work.left)-16),std::min(app.px(784),(int)(work.bottom-work.top)-16),nullptr,nullptr,instance,&app);
    if(!window){CoUninitialize();return 1;}
    ShowWindow(window,show);UpdateWindow(window);MSG msg{};
    while(GetMessageW(&msg,nullptr,0,0)>0){if(!IsDialogMessageW(window,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
    CoUninitialize();return (int)msg.wParam;
}
