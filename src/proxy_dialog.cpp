#include "proxy.hpp"
#include "appearance.hpp"
#include <thread>
#include <memory>
namespace mg {
LRESULT CALLBACK noWheelCombo(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR){
    if(m==WM_MOUSEWHEEL||m==WM_MOUSEHWHEEL){if(!SendMessageW(h,CB_GETDROPPEDSTATE,0,0))SendMessageW(GetParent(h),m,w,l);return 0;}
    if(m==WM_NCDESTROY)RemoveWindowSubclass(h,noWheelCombo,id);return DefSubclassProc(h,m,w,l);
}
namespace {
struct DialogState{
    fs::path root;ProxySettings p;Appearance appearance;Palette palette=paletteFor(Appearance{});bool busy=false,closing=false,saved=false,initializing=false;int result=0;
    std::atomic_bool cancel{false};std::thread thread;HBRUSH bg=nullptr,field=nullptr;
    const wchar_t* t(const wchar_t* value)const{return tr(value,appearance.language);}
};
std::wstring field(HWND h,int id){auto c=GetDlgItem(h,id);int n=GetWindowTextLengthW(c);std::wstring s(n+1,0);GetWindowTextW(c,s.data(),n+1);s.resize(n);return s;}
ProxySettings collect(HWND h){
    ProxySettings p;p.enabled=IsDlgButtonChecked(h,301)==BST_CHECKED;p.type=(int)SendDlgItemMessageW(h,302,CB_GETCURSEL,0,0);p.host=trim(field(h,303));p.user=field(h,305);p.password=field(h,306);
    auto port=field(h,304);if(!port.empty()){size_t end=0;try{p.port=std::stoi(port,&end);}catch(...){throw std::runtime_error(tr("Invalid port"));}if(end!=port.size())throw std::runtime_error(tr("Invalid port"));}
    validateProxy(p);return p;
}
void enable(HWND h,DialogState& s){
    bool enabled=IsDlgButtonChecked(h,301)==BST_CHECKED;
    for(int id:{301,401,402,403,IDOK})EnableWindow(GetDlgItem(h,id),!s.busy);
    for(int id:{302,303,304,305,306,307})EnableWindow(GetDlgItem(h,id),enabled&&!s.busy);
}
void fillCombo(HWND h,int id,std::initializer_list<const wchar_t*> values,int selected,DialogState& s){
    SendDlgItemMessageW(h,id,CB_RESETCONTENT,0,0);for(auto value:values)SendDlgItemMessageW(h,id,CB_ADDSTRING,0,(LPARAM)s.t(value));SendDlgItemMessageW(h,id,CB_SETCURSEL,selected,0);
}
void appearance(HWND h,DialogState& s){
    s.initializing=true;SetWindowTextW(h,s.t(L"Settings"));
    const std::pair<int,const wchar_t*> labels[]={{410,L"Appearance"},{411,L"Interface style"},{412,L"Color mode"},{413,L"Language"},{414,L"Proxy"},{301,L"Use a proxy for all downloads"},{310,L"Type"},{311,L"Server address"},{312,L"Port"},{313,L"Username (optional)"},{314,L"Password"},{315,L"The proxy applies to videos and components."},{307,L"Test connection"},{416,L"Changes apply after saving."},{IDOK,L"Save"},{IDCANCEL,L"Cancel"}};
    for(auto [id,text]:labels)SetDlgItemTextW(h,id,s.t(text));
    fillCombo(h,401,{L"Fluent",L"Graphite",L"Warm minimalism",L"Technical"},s.appearance.style,s);
    fillCombo(h,402,{L"Follow Windows",L"Light",L"Dark"},s.appearance.theme,s);
    fillCombo(h,403,{L"Russian",L"English"},s.appearance.language,s);
    s.palette=paletteFor(s.appearance);if(s.bg)DeleteObject(s.bg);if(s.field)DeleteObject(s.field);s.bg=CreateSolidBrush(s.palette.bg);s.field=CreateSolidBrush(s.palette.field);
    BOOL dark=s.palette.dark;DwmSetWindowAttribute(h,20,&dark,sizeof(dark));
    RedrawWindow(h,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);s.initializing=false;
}
INT_PTR CALLBACK dialogProc(HWND h,UINT m,WPARAM w,LPARAM l){
    auto s=(DialogState*)GetWindowLongPtrW(h,DWLP_USER);
    if(m==WM_INITDIALOG){
        if(auto owner=GetParent(h)){
            SendMessageW(h,WM_SETICON,ICON_SMALL,GetClassLongPtrW(owner,GCLP_HICONSM));
            SendMessageW(h,WM_SETICON,ICON_BIG,GetClassLongPtrW(owner,GCLP_HICON));
        }
        s=(DialogState*)l;SetWindowLongPtrW(h,DWLP_USER,l);auto& p=s->p;
        CheckDlgButton(h,301,p.enabled?BST_CHECKED:BST_UNCHECKED);
        SendDlgItemMessageW(h,302,CB_ADDSTRING,0,(LPARAM)L"HTTP");SendDlgItemMessageW(h,302,CB_ADDSTRING,0,(LPARAM)L"SOCKS5");SendDlgItemMessageW(h,302,CB_SETCURSEL,p.type,0);
        for(int id:{302,401,402,403}){auto c=GetDlgItem(h,id);SetWindowSubclass(c,noWheelCombo,1,0);COMBOBOXINFO ci{sizeof(ci)};if(GetComboBoxInfo(c,&ci)&&ci.hwndList)SetWindowSubclass(ci.hwndList,noWheelCombo,2,0);}
        SetDlgItemTextW(h,303,p.host.c_str());SetDlgItemTextW(h,304,p.port?std::to_wstring(p.port).c_str():L"");SetDlgItemTextW(h,305,p.user.c_str());SetDlgItemTextW(h,306,p.password.c_str());
        SendDlgItemMessageW(h,303,EM_SETLIMITTEXT,253,0);SendDlgItemMessageW(h,304,EM_SETLIMITTEXT,5,0);SendDlgItemMessageW(h,305,EM_SETLIMITTEXT,255,0);SendDlgItemMessageW(h,306,EM_SETLIMITTEXT,255,0);
        EnumChildWindows(h,[](HWND child,LPARAM data)->BOOL{skinControl(child,(Palette*)data);return TRUE;},(LPARAM)&s->palette);
        appearance(h,*s);enable(h,*s);
        RECT r,owner;GetWindowRect(h,&r);GetWindowRect(GetParent(h),&owner);MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(h,MONITOR_DEFAULTTONEAREST),&mi);
        int x=std::max<int>(mi.rcWork.left,std::min<int>((owner.left+owner.right-r.right+r.left)/2,mi.rcWork.right-(r.right-r.left)));
        int y=std::max<int>(mi.rcWork.top,std::min<int>((owner.top+owner.bottom-r.bottom+r.top)/2,mi.rcWork.bottom-(r.bottom-r.top)));
        SetWindowPos(h,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER);return TRUE;
    }
    if(!s)return FALSE;
    if(m==WM_DRAWITEM){auto& d=*(DRAWITEMSTRUCT*)l;if(d.CtlType==ODT_BUTTON){drawButton(d,s->palette,d.CtlID==IDOK);return TRUE;}}
    if(m==WM_CTLCOLORDLG||m==WM_CTLCOLORSTATIC||m==WM_CTLCOLOREDIT||m==WM_CTLCOLORLISTBOX||m==WM_CTLCOLORBTN){
        auto color=s->palette.fg;bool input=m==WM_CTLCOLOREDIT||m==WM_CTLCOLORLISTBOX;auto id=GetDlgCtrlID((HWND)l);
        if(id==308&&s->result)color=s->result>0?s->palette.success:s->palette.error;
        else if(m!=WM_CTLCOLORDLG&&!IsWindowEnabled((HWND)l))color=s->palette.muted;
        SetTextColor((HDC)w,color);SetBkColor((HDC)w,input?s->palette.field:s->palette.bg);return (INT_PTR)(input?s->field:s->bg);
    }
    if(m==WM_APP+7){std::unique_ptr<std::wstring> result((std::wstring*)l);if(s->thread.joinable())s->thread.join();s->busy=false;s->result=(int)w;SetDlgItemTextW(h,308,result->c_str());InvalidateRect(GetDlgItem(h,308),nullptr,TRUE);enable(h,*s);if(s->closing)EndDialog(h,IDCANCEL);return TRUE;}
    if(m==WM_CLOSE||(m==WM_COMMAND&&LOWORD(w)==IDCANCEL)){
        if(s->busy){s->cancel=true;s->closing=true;s->result=0;SetDlgItemTextW(h,308,s->t(L"Stopping the test…"));}else EndDialog(h,IDCANCEL);return TRUE;
    }
    if(m==WM_COMMAND){
        int id=LOWORD(w),notification=HIWORD(w);
        if(s->initializing)return TRUE;
        if(id>=401&&id<=403&&notification==CBN_SELCHANGE){
            s->appearance.style=(int)SendDlgItemMessageW(h,401,CB_GETCURSEL,0,0);s->appearance.theme=(int)SendDlgItemMessageW(h,402,CB_GETCURSEL,0,0);s->appearance.language=(int)SendDlgItemMessageW(h,403,CB_GETCURSEL,0,0);
            s->result=0;SetDlgItemTextW(h,308,L"");appearance(h,*s);return TRUE;
        }
        if(s->busy)return TRUE;
        if(id==301||((id>=303&&id<=306)&&notification==EN_CHANGE)||(id==302&&notification==CBN_SELCHANGE)){
            s->result=0;SetDlgItemTextW(h,308,L"");enable(h,*s);return TRUE;
        }
        if(id!=IDOK&&id!=307)return FALSE;
        try{
            auto p=collect(h);
            if(id==IDOK){saveProxy(s->root/L"proxy.json",p);saveAppearance(s->root,s->appearance);setProxy(p);s->saved=true;EndDialog(h,IDOK);return TRUE;}
            if(!p.enabled)return TRUE;
            s->busy=true;s->cancel=false;s->result=0;enable(h,*s);SetDlgItemTextW(h,308,s->t(L"Testing GitHub and YouTube through the proxy…"));
            s->thread=std::thread([s,h,p]{
                std::wstring result;int state=0;
                try{httpTextUsing(L"https://api.github.com/repos/yt-dlp/yt-dlp/releases/latest",s->cancel,p);httpTextUsing(L"https://www.youtube.com/robots.txt",s->cancel,p);state=1;result=s->t(L"Proxy connection successful. GitHub and YouTube are responding.");}
                catch(const Cancelled&){result=s->t(L"Test cancelled.");}
                catch(const std::exception& e){state=-1;auto original=sourceText(friendlyError(wide(e.what())).c_str());result=redactProxy(s->t(original.c_str()),p);}
                auto* value=new std::wstring(result);if(!PostMessageW(h,WM_APP+7,(WPARAM)state,(LPARAM)value))delete value;
            });
        }catch(const std::exception& e){s->result=-1;auto original=sourceText(friendlyError(wide(e.what())).c_str());SetDlgItemTextW(h,308,s->t(original.c_str()));InvalidateRect(GetDlgItem(h,308),nullptr,TRUE);}
        return TRUE;
    }
    return FALSE;
}
}
bool showSettingsDialog(HWND owner,const fs::path& root,Appearance& a){
    DialogState s;s.root=root;s.p=getProxy();s.appearance=a;
    DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(200),owner,dialogProc,(LPARAM)&s);
    s.cancel=true;if(s.thread.joinable())s.thread.join();if(s.bg)DeleteObject(s.bg);if(s.field)DeleteObject(s.field);
    if(s.saved)a=s.appearance;return s.saved;
}
}
