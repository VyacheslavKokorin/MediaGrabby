#include "terminal.hpp"
#include <iostream>
#include <string>
using namespace mg;
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed: " #x " line "<<__LINE__<<"\n";return 1;}}while(0)
void pump(HWND host){
    MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
    RedrawWindow(host,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
}
void dump(HWND edit,const char* phase){
    SCROLLINFO s{sizeof(s),SIF_ALL};GetScrollInfo(edit,SB_VERT,&s);RECT r{},format{};GetClientRect(edit,&r);SendMessageW(edit,EM_GETRECT,0,(LPARAM)&format);
    POINT point{};SendMessageW(edit,EM_GETSCROLLPOS,0,(LPARAM)&point);
    std::cout<<phase<<": max="<<s.nMax<<" page="<<s.nPage<<" pos="<<s.nPos<<" y="<<point.y<<" height="<<r.bottom<<" style="<<GetWindowLongPtrW(edit,GWL_STYLE)<<" format="<<format.top<<","<<format.bottom<<"\n";
}
int main(){
    LoadLibraryW(L"Msftedit.dll");INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_STANDARD_CLASSES};InitCommonControlsEx(&ic);
    HWND host=CreateWindowExW(0,L"STATIC",L"MediaGrabby terminal test",WS_OVERLAPPEDWINDOW,0,0,640,400,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    CHECK(host);ShowWindow(host,SW_SHOWNOACTIVATE);
    HWND edit=CreateWindowExW(0,MSFTEDIT_CLASS,L"",WS_CHILD|WS_VISIBLE|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,10,10,580,290,host,nullptr,GetModuleHandleW(nullptr),nullptr);
    CHECK(edit);SendMessageW(edit,EM_SETTARGETDEVICE,0,0);SendMessageW(edit,EM_SETLIMITTEXT,2000000,0);
    TerminalLog log;log.attach(edit);
    for(int i=0;i<300;++i)log.append(L"Live output line "+std::to_wstring(i));
    SCROLLINFO info{sizeof(info),SIF_ALL};GetScrollInfo(edit,SB_VERT,&info);
    std::cout<<"Initial scroll range: min="<<info.nMin<<" max="<<info.nMax<<" page="<<info.nPage<<" pos="<<info.nPos<<"\n";
    CHECK(log.following());CHECK(log.atBottom());
    // A one-pixel departure is enough to pause; the exact bottom resumes.
    POINT pixel{};SendMessageW(edit,EM_GETSCROLLPOS,0,(LPARAM)&pixel);--pixel.y;
    SendMessageW(edit,EM_SETSCROLLPOS,0,(LPARAM)&pixel);CHECK(!log.following());
    SendMessageW(edit,WM_VSCROLL,SB_BOTTOM,0);CHECK(log.following());
    // Moving just one line away must pause before the next output arrives.
    SendMessageW(edit,EM_LINESCROLL,0,-1);CHECK(!log.atBottom());CHECK(!log.following());
    int first=(int)SendMessageW(edit,EM_GETFIRSTVISIBLELINE,0,0);
    for(int i=0;i<30;++i)log.append(L"Output while reading");
    CHECK(!log.following());CHECK((int)SendMessageW(edit,EM_GETFIRSTVISIBLELINE,0,0)==first);
    SendMessageW(edit,WM_VSCROLL,SB_BOTTOM,0);CHECK(log.following());
    log.append(L"Following again");CHECK(log.atBottom());
    SendMessageW(edit,WM_VSCROLL,SB_TOP,0);CHECK(!log.following());
    CHARRANGE selection{5,15};SendMessageW(edit,EM_EXSETSEL,0,(LPARAM)&selection);
    log.append(L"Keep the selection");CHARRANGE actual{};SendMessageW(edit,EM_EXGETSEL,0,(LPARAM)&actual);
    CHECK(actual.cpMin==selection.cpMin&&actual.cpMax==selection.cpMax);
    CHECK((int)SendMessageW(edit,EM_GETFIRSTVISIBLELINE,0,0)==0);
    // Hidden append must not restore WS_VISIBLE or change the follow preference.
    ShowWindow(edit,SW_HIDE);
    for(int i=0;i<40;++i)log.append(L"Output while collapsed");
    CHECK((GetWindowLongPtrW(edit,GWL_STYLE)&WS_VISIBLE)==0);CHECK(!log.following());
    ShowWindow(edit,SW_SHOW);CHECK(!log.following());
    MoveWindow(edit,10,10,480,220,TRUE);CHECK(!log.following());dump(edit,"Resized before paint");pump(host);dump(edit,"Resized after paint");
    SendMessageW(edit,WM_VSCROLL,SB_BOTTOM,0);dump(edit,"Bottom after resize");CHECK(log.following());
    MoveWindow(edit,10,10,400,170,TRUE);CHECK(log.following());CHECK(log.atBottom());
    ShowWindow(edit,SW_HIDE);log.append(L"Hidden at the bottom");CHECK((GetWindowLongPtrW(edit,GWL_STYLE)&WS_VISIBLE)==0);
    ShowWindow(edit,SW_SHOW);log.append(L"Visible again");CHECK(log.following());CHECK(log.atBottom());
    // Trimming the bounded display buffer must preserve a retained reading position.
    log.clear();std::wstring row(18000,L'x');
    for(int i=0;i<28;++i)log.append(row);
    SendMessageW(edit,EM_LINESCROLL,0,-3);CHECK(!log.following());
    log.append(L"This append trims old output");CHECK(!log.following());
    CHECK(GetWindowTextLengthW(edit)<400000);
    log.clear();CHECK(log.following());CHECK(log.atBottom());CHECK(GetWindowTextLengthW(edit)==0);
    log.append(L"A short fresh log");dump(edit,"Short log after clear");CHECK(log.following());CHECK(log.atBottom());
    DestroyWindow(host);std::cout<<"Terminal visibility, pause/resume, selection, resize and trimming passed\n";
}
