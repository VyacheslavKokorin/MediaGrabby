#include "terminal.hpp"
#include "i18n.hpp"
#include <algorithm>
namespace mg {
void TerminalLog::attach(HWND window){
    window_=window;SetWindowSubclass(window,proc,17,(DWORD_PTR)this);
}
bool TerminalLog::atBottom()const{
    // RichEdit formats wrapped text lazily after a resize. Complete the line
    // layout before inspecting its range; an incomplete range ends too early.
    SendMessageW(window_,EM_GETLINECOUNT,0,0);
    SCROLLINFO s{sizeof(s),SIF_RANGE|SIF_PAGE|SIF_POS};
    if(!GetScrollInfo(window_,SB_VERT,&s))return true;
    // An empty/non-scrollable RichEdit can retain the default 0..100 range
    // with a zero page size even though no vertical scrolling is possible.
    if(GetWindowTextLengthW(window_)==0||s.nPage==0||!(GetWindowLongPtrW(window_,GWL_STYLE)&WS_VSCROLL))return true;
    // RichEdit's SB_BOTTOM settles at nMax - nPage (one pixel below the
    // generic scrollbar maximum). Use its actual viewport boundary, without
    // a tolerance that could keep following after the user scrolls upward.
    int last=std::max(s.nMin,s.nMax-(int)s.nPage);
    return s.nPos>=last;
}
LRESULT CALLBACK TerminalLog::proc(HWND h,UINT message,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR data){
    auto& self=*(TerminalLog*)data;
    if(message==WM_NCDESTROY){RemoveWindowSubclass(h,proc,id);self.window_=nullptr;return DefSubclassProc(h,message,w,l);}
    // Resizing changes the available viewport, not the user's follow preference.
    if(message==WM_SIZE&&!self.updating_){
        self.updating_=true;auto result=DefSubclassProc(h,message,w,l);
        if(self.following_)SendMessageW(h,WM_VSCROLL,SB_BOTTOM,0);
        self.updating_=false;return result;
    }
    if(message==WM_VSCROLL&&LOWORD(w)==SB_BOTTOM)SendMessageW(h,EM_GETLINECOUNT,0,0);
    auto result=DefSubclassProc(h,message,w,l);
    if(!self.updating_){
        switch(message){
        case WM_MOUSEMOVE:if(w&MK_LBUTTON)self.following_=self.atBottom();break;
        case WM_VSCROLL:case WM_MOUSEWHEEL:case WM_KEYDOWN:case WM_LBUTTONUP:
        case EM_LINESCROLL:case EM_SCROLL:case EM_SETSCROLLPOS:
            self.following_=self.atBottom();break;
        }
    }
    return result;
}
void TerminalLog::append(const std::wstring& text){
    if(!window_)return;
    auto line=text.size()>20000?text.substr(0,20000)+tr(L"… [full line in logs]"):text;line+=L"\r\n";
    auto h=window_;bool visible=(GetWindowLongPtrW(h,GWL_STYLE)&WS_VISIBLE)!=0;
    POINT position{};SendMessageW(h,EM_GETSCROLLPOS,0,(LPARAM)&position);
    CHARRANGE selection{};SendMessageW(h,EM_EXGETSEL,0,(LPARAM)&selection);
    int first=(int)SendMessageW(h,EM_GETFIRSTVISIBLELINE,0,0),removedLines=0;
    updating_=true;
    // WM_SETREDRAW(TRUE) adds WS_VISIBLE: never send it to a collapsed terminal.
    if(visible)SendMessageW(h,WM_SETREDRAW,FALSE,0);
    int n=GetWindowTextLengthW(h);
    if(n>500000){
        auto marker=tr(L"[Earlier output saved in logs]\r\n");
        int before=(int)SendMessageW(h,EM_GETLINECOUNT,0,0);
        SendMessageW(h,EM_SETSEL,0,200000);SendMessageW(h,EM_REPLACESEL,FALSE,(LPARAM)marker);
        removedLines=before-(int)SendMessageW(h,EM_GETLINECOUNT,0,0);
        int delta=200000-(int)wcslen(marker);
        selection.cpMin=std::max(0L,selection.cpMin-delta);selection.cpMax=std::max(0L,selection.cpMax-delta);
    }
    SendMessageW(h,EM_SETSEL,(WPARAM)-1,(LPARAM)-1);SendMessageW(h,EM_REPLACESEL,FALSE,(LPARAM)line.c_str());
    // RichEdit finalizes its scroll range when redraw is re-enabled.
    // Restore the viewport only after that, while ignoring programmatic scrolls.
    if(visible)SendMessageW(h,WM_SETREDRAW,TRUE,0);
    SendMessageW(h,EM_EXSETSEL,0,(LPARAM)&selection);
    if(following_)SendMessageW(h,WM_VSCROLL,SB_BOTTOM,0);
    else if(removedLines){
        int current=(int)SendMessageW(h,EM_GETFIRSTVISIBLELINE,0,0);
        SendMessageW(h,EM_LINESCROLL,0,std::max(0,first-removedLines)-current);
    }else SendMessageW(h,EM_SETSCROLLPOS,0,(LPARAM)&position);
    if(visible)RedrawWindow(h,nullptr,nullptr,RDW_INVALIDATE|RDW_FRAME);
    updating_=false;
}
void TerminalLog::clear(){
    updating_=true;SetWindowTextW(window_,L"");following_=true;updating_=false;
}
}
