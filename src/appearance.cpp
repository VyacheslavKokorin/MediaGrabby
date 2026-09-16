#include "appearance.hpp"
#include <windowsx.h>
#include <uxtheme.h>
namespace mg {
void rounded(HDC dc,RECT r,COLORREF fill,COLORREF border,int radius){
    auto b=CreateSolidBrush(fill);auto pen=CreatePen(PS_SOLID,1,border);auto ob=SelectObject(dc,b);auto op=SelectObject(dc,pen);
    RoundRect(dc,r.left,r.top,r.right,r.bottom,radius*2,radius*2);SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(b);DeleteObject(pen);
}
void drawButton(const DRAWITEMSTRUCT& d,const Palette& p,bool primary){
    bool disabled=(d.itemState&ODS_DISABLED)!=0,down=(d.itemState&ODS_SELECTED)!=0;
    RECT r=d.rcItem;auto bg=CreateSolidBrush(p.bg);FillRect(d.hDC,&r,bg);DeleteObject(bg);
    rounded(d.hDC,r,primary&&!disabled?p.accent:down?p.border:p.field,primary&&!disabled?p.accent:p.border,p.radius);
    SetBkMode(d.hDC,TRANSPARENT);SetTextColor(d.hDC,disabled?p.muted:primary?p.onAccent:p.fg);
    auto old=SelectObject(d.hDC,(HFONT)SendMessageW(d.hwndItem,WM_GETFONT,0,0));wchar_t text[256]{};GetWindowTextW(d.hwndItem,text,256);
    InflateRect(&r,-8,0);if(down)OffsetRect(&r,1,1);DrawTextW(d.hDC,text,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    if(d.itemState&ODS_FOCUS){InflateRect(&r,-2,-4);DrawFocusRect(d.hDC,&r);}SelectObject(d.hDC,old);
}
namespace {
LRESULT CALLBACK skin(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR data){
    auto& p=*(Palette*)data;wchar_t cls[32]{};GetClassNameW(h,cls,32);bool combo=wcscmp(cls,L"ComboBox")==0;bool edit=wcscmp(cls,L"Edit")==0;
    bool check=wcscmp(cls,L"Button")==0&&(GetWindowLongPtrW(h,GWL_STYLE)&BS_TYPEMASK)==BS_AUTOCHECKBOX;
    if(m==WM_NCDESTROY)RemoveWindowSubclass(h,skin,id);
    if(m==WM_NCPAINT&&edit){auto result=DefSubclassProc(h,m,w,l);HDC dc=GetWindowDC(h);RECT r;GetWindowRect(h,&r);OffsetRect(&r,-r.left,-r.top);auto b=CreateSolidBrush(GetFocus()==h?p.accent:p.border);FrameRect(dc,&r,b);DeleteObject(b);ReleaseDC(h,dc);return result;}
    if(m==WM_PAINT&&(combo||check)){
        PAINTSTRUCT ps;auto dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);wchar_t parentClass[32]{};GetClassNameW(GetParent(h),parentClass,32);auto background=wcscmp(parentClass,L"#32770")==0?p.bg:p.card;auto b=CreateSolidBrush(background);FillRect(dc,&r,b);DeleteObject(b);
        auto font=(HFONT)SendMessageW(h,WM_GETFONT,0,0);auto old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,IsWindowEnabled(h)?p.fg:p.muted);
        wchar_t text[512]{};GetWindowTextW(h,text,512);
        if(combo){
            rounded(dc,r,p.field,GetFocus()==h?p.accent:p.border,p.radius);RECT t=r;t.left+=10;t.right-=28;
            DrawTextW(dc,text,-1,&t,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
            int x=r.right-17,y=(r.bottom+r.top)/2;auto pen=CreatePen(PS_SOLID,1,IsWindowEnabled(h)?p.fg:p.muted);auto op=SelectObject(dc,pen);MoveToEx(dc,x-4,y-2,nullptr);LineTo(dc,x,y+2);LineTo(dc,x+4,y-2);SelectObject(dc,op);DeleteObject(pen);
        }else{
            int size=MulDiv(17,GetDeviceCaps(dc,LOGPIXELSY),96);RECT box{0,(r.bottom-size)/2,size,(r.bottom+size)/2};bool checked=SendMessageW(h,BM_GETCHECK,0,0)==BST_CHECKED;
            rounded(dc,box,checked?p.accent:p.field,checked?p.accent:p.border,3);
            if(checked){auto pen=CreatePen(PS_SOLID,2,p.onAccent);auto op=SelectObject(dc,pen);MoveToEx(dc,box.left+4,box.top+size/2,nullptr);LineTo(dc,box.left+size/2-1,box.bottom-5);LineTo(dc,box.right-3,box.top+4);SelectObject(dc,op);DeleteObject(pen);}
            r.left=size+8;DrawTextW(dc,text,-1,&r,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);if(GetFocus()==h)DrawFocusRect(dc,&r);
        }
        SelectObject(dc,old);EndPaint(h,&ps);return 0;
    }
    auto result=DefSubclassProc(h,m,w,l);
    if(m==WM_SETFOCUS||m==WM_KILLFOCUS||m==WM_ENABLE||m==BM_SETCHECK||m==WM_LBUTTONUP||m==WM_KEYUP||m==CB_SETCURSEL)RedrawWindow(h,nullptr,nullptr,RDW_INVALIDATE|RDW_FRAME);
    return result;
}
}
void skinControl(HWND h,Palette* p){
    SetWindowTheme(h,L"",L"");wchar_t cls[32]{};GetClassNameW(h,cls,32);
    if(wcscmp(cls,L"Edit")==0){SetWindowLongPtrW(h,GWL_EXSTYLE,GetWindowLongPtrW(h,GWL_EXSTYLE)&~WS_EX_CLIENTEDGE);SetWindowLongPtrW(h,GWL_STYLE,GetWindowLongPtrW(h,GWL_STYLE)|WS_BORDER);SetWindowPos(h,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);SendMessageW(h,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELPARAM(9,9));}
    SetWindowSubclass(h,skin,8,(DWORD_PTR)p);
}
}
