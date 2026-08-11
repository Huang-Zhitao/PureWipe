#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <mutex>
#include <atomic>
#include <format>
#include <utility>
#include <cctype>

#if __cplusplus >= 202601L
#   define CPP26_CONSTEXPR constexpr
#else
#   define CPP26_CONSTEXPR const
#endif

#ifndef MB_ICONINFORMATION
#define MB_ICONINFORMATION 0x00000040L
#endif

int g_progress_cur = 0;
int g_progress_total = 100;

namespace Const
{
    CPP26_CONSTEXPR DWORD STEP_SLEEP_MS = 300;
    CPP26_CONSTEXPR size_t WSTR_BUF = MAX_PATH * 2;
    CPP26_CONSTEXPR LPCWSTR WND_CLASS = L"PureWipe";
    CPP26_CONSTEXPR LPCWSTR WND_TITLE = L"PureWipe 清理工具";
}

inline int DpiScale(HWND hwnd, int px)
{
    UINT dpi = GetDpiForWindow(hwnd);
    return MulDiv(px, 96, dpi);
}
inline RECT DpiRect(HWND hwnd, int l, int t, int r, int b)
{
    return { DpiScale(hwnd,l), DpiScale(hwnd,t), DpiScale(hwnd,r), DpiScale(hwnd,b) };
}

template<typename H, H Invalid = nullptr, auto CloseFn = CloseHandle>
struct SafeHandle
{
    H h = Invalid;
    constexpr SafeHandle() noexcept = default;
    constexpr SafeHandle(H hh) noexcept : h(hh) {}
    ~SafeHandle() noexcept { if(h != Invalid) CloseFn(h); }
    operator H() const noexcept { return h; }
    H Release() noexcept { H tmp=h; h=Invalid; return tmp; }
};
using SafeToken = SafeHandle<HANDLE>;
using SafeWow64State = SafeHandle<PVOID>;

enum LogType { LOG_NORMAL, LOG_OK, LOG_ERR };
struct LogLine { std::wstring txt; LogType tp; };
std::vector<LogLine> g_logs;
std::mutex g_logMtx;
int g_scrollY = 0;
const int LINE_BASE = 22;

class Logger
{
private:
    std::wofstream f;
    std::mutex m;
public:
    void Init();
    void Write(const std::wstring& s);
    void Close();
};
Logger g_logger;

inline void PushLog(LogType tp, const std::wstring& msg)
{
    std::lock_guard<std::mutex> lock(g_logMtx);
    g_logs.push_back({msg, tp});
    g_logger.Write(msg);
}

namespace SysAPI
{
    std::wstring Env(const std::wstring& s)
    {
        WCHAR buf[Const::WSTR_BUF]{};
        ExpandEnvironmentStringsW(s.c_str(), buf, Const::WSTR_BUF);
        return std::wstring(buf);
    }
    bool Exists(const std::wstring& p)
    {
        return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES;
    }
    bool ResetDir(const std::wstring& p)
    {
        if(Exists(p))
        {
            int ret = _wsystem(std::format(L"rd /s /q \"{}\"", p).c_str());
            if((ret >> 8) != 0) return false;
        }
        int ret = _wsystem(std::format(L"mkdir \"{}\"", p).c_str());
        return (ret >> 8) == 0;
    }
    SafeWow64State DisableWow64()
    {
        PVOID s = nullptr;
        Wow64DisableWow64FsRedirection(&s);
        return SafeWow64State(s);
    }
    bool IsAdmin()
    {
        SafeToken t;
        if(!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t.h)) return false;
        TOKEN_ELEVATION e;
        DWORD sz = sizeof(e);
        DWORD retSz = 0;
        BOOL ok = GetTokenInformation(t, TokenElevation, &e, sz, &retSz);
        return ok && e.TokenIsElevated;
    }
    bool ReRunAdmin()
    {
        WCHAR self[Const::WSTR_BUF]{};
        GetModuleFileNameW(nullptr, self, Const::WSTR_BUF);
        SHELLEXECUTEINFOW sei{};
        ZeroMemory(&sei, sizeof(sei));
        sei.cbSize = sizeof(sei);
        sei.lpFile = self;
        sei.lpVerb = L"runas";
        sei.nShow = SW_SHOWNORMAL;
        if(!ShellExecuteExW(&sei))
        {
            MessageBoxW(nullptr, L"提权失败，请右键管理员运行", L"错误", MB_OK|MB_ICONERROR);
            return false;
        }
        SafeHandle<HANDLE> h(sei.hProcess);
        WaitForSingleObject(h, INFINITE);
        return true;
    }
    bool Confirm(const std::wstring& t, const std::wstring& msg)
    {
        return MessageBoxW(nullptr, msg.c_str(), t.c_str(), MB_YESNO|MB_ICONWARNING) == IDYES;
    }
}

enum TaskRisk { SAFE, NET_RISK, DANGER };
struct TaskItem
{
    TaskRisk risk;
    std::wstring name;
    std::wstring cmd;
    std::wstring resetFolder;
    bool enable = true;
};
std::vector<TaskItem> g_tasks;
void RegisterTasks();

struct ITaskNotify
{
    virtual void OnProgress(int cur, int total, const std::wstring& name) = 0;
    virtual void OnStep(bool ok, DWORD code) = 0;
    virtual void OnFinish(bool allOk) = 0;
};

class TaskRunner
{
private:
    std::atomic<bool> stop{false};
    HANDLE thread = nullptr;
    ITaskNotify* cb = nullptr;
    static DWORD WINAPI Work(LPVOID p);
public:
    ~TaskRunner(){ Stop(); if(thread) CloseHandle(thread); }
    void Start(ITaskNotify* c);
    void Stop(){ stop.store(true); if(thread) WaitForSingleObject(thread, 2000); }
    bool Running() const { return thread && WaitForSingleObject(thread,0) == WAIT_TIMEOUT; }
};
TaskRunner g_runner;

enum CtrlID
{
    ID_PROG = 1001,
    ID_SCROLL = 1002,
    ID_BTN_START = 1003,
    ID_BTN_STOP = 1004,
    ID_CHK_SAFE = 1005,
    ID_CHK_NET = 1006,
    ID_CHK_DANGER = 1007
};

struct WinCtx : ITaskNotify
{
    HWND hMain = nullptr;
    HWND hScroll = nullptr;
    HWND hStart = nullptr;
    HWND hStop = nullptr;
    HWND hChkSafe = nullptr;
    HWND hChkNet = nullptr;
    HWND hChkDanger = nullptr;
    std::wstring curTask;

    void RedrawLog() { InvalidateRect(hMain, nullptr, TRUE); }
    void OnProgress(int cur, int total, const std::wstring& name) override;
    void OnStep(bool ok, DWORD code) override;
    void OnFinish(bool allOk) override;
};
WinCtx g_win;

DWORD WINAPI TaskRunner::Work(LPVOID p)
{
    auto self = reinterpret_cast<TaskRunner*>(p);
    auto wow = SysAPI::DisableWow64();
    int total = static_cast<int>(g_tasks.size());
    bool allOk = true;
    self->stop.store(false);

    for(int i=0; i<total; i++)
    {
        if(self->stop.load()) break;
        auto& task = g_tasks[i];
        if(!task.enable) continue;

        self->cb->OnProgress(i+1, total, task.name);
        PushLog(LOG_NORMAL, std::format(L"==== {} ====", task.name));

        bool skip = false;
        switch(task.risk)
        {
            case NET_RISK:
                if(!SysAPI::Confirm(L"网络风险", L"重置IP/DNS，确认继续？"))
                {
                    PushLog(LOG_NORMAL, L"跳过网络任务");
                    skip = true;
                }
                break;
            case DANGER:
                if(!SysAPI::Confirm(L"高危删除", L"文件删除不可恢复！"))
                {
                    PushLog(LOG_NORMAL, L"跳过高危任务");
                    skip = true;
                }
                break;
            default: break;
        }
        if(skip) continue;

        int ret = _wsystem(task.cmd.c_str());
        DWORD ec = static_cast<DWORD>(ret >> 8);
        self->cb->OnStep(ec == 0, ec);

        if(!task.resetFolder.empty() && SysAPI::Exists(task.resetFolder))
            SysAPI::ResetDir(task.resetFolder);

        Sleep(Const::STEP_SLEEP_MS);
        if(ec != 0)
        {
            int sel = MessageBoxW(g_win.hMain, L"当前步骤失败，是否继续？", L"警告", MB_YESNO|MB_ICONERROR);
            if(sel == IDNO) { allOk = false; break; }
        }
    }
    self->cb->OnFinish(allOk && !self->stop.load());
    return 0;
}

void TaskRunner::Start(ITaskNotify* c)
{
    cb = c;
    stop.store(false);
    thread = CreateThread(nullptr,0,TaskRunner::Work,this,0,nullptr);
}

void WinCtx::OnProgress(int cur, int total, const std::wstring& name)
{
    curTask = name;
    g_progress_cur = cur;
    g_progress_total = total;
    SetWindowTextW(hMain, std::format(L"{} | {}/{} {}", Const::WND_TITLE, cur, total, name).c_str());
    RedrawLog();
}

void WinCtx::OnStep(bool ok, DWORD code)
{
    if(ok) PushLog(LOG_OK, std::format(L"✅ 成功 返回码{}", code));
    else PushLog(LOG_ERR, std::format(L"❌ 失败 返回码{}", code));
    RedrawLog();
}

void WinCtx::OnFinish(bool allOk)
{
    EnableWindow(hStart, TRUE);
    EnableWindow(hStop, FALSE);
    SetWindowTextW(hMain, Const::WND_TITLE);
    if(allOk) MessageBoxW(hMain, L"全部清理完成", L"完成", MB_OK | MB_ICONINFORMATION);
    else MessageBoxW(hMain, L"任务中途失败/终止", L"警告", MB_OK | MB_ICONWARNING);
}

LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch(msg)
    {
    case WM_CREATE:
    {
        auto dr = [&](int l,int t,int r,int b){ return DpiRect(h,l,t,r,b); };

        g_win.hChkSafe = CreateWindowExW(0, L"BUTTON", L"安全清理", BS_AUTOCHECKBOX|WS_CHILD|WS_VISIBLE,
            dr(10,40,180,62).left,dr(10,40,180,62).top,160,22,
            h,(HMENU)ID_CHK_SAFE,nullptr,nullptr);
        g_win.hChkNet = CreateWindowExW(0, L"BUTTON", L"网络重置", BS_AUTOCHECKBOX|WS_CHILD|WS_VISIBLE,
            dr(200,40,370,62).left,dr(200,40,370,62).top,160,22,
            h,(HMENU)ID_CHK_NET,nullptr,nullptr);
        g_win.hChkDanger = CreateWindowExW(0, L"BUTTON", L"高危删除", BS_AUTOCHECKBOX|WS_CHILD|WS_VISIBLE,
            dr(390,40,560,62).left,dr(390,40,560,62).top,160,22,
            h,(HMENU)ID_CHK_DANGER,nullptr,nullptr);
        SendMessageW(g_win.hChkSafe, BM_SETCHECK, BST_CHECKED,0);
        SendMessageW(g_win.hChkNet, BM_SETCHECK, BST_CHECKED,0);
        SendMessageW(g_win.hChkDanger, BM_SETCHECK, BST_CHECKED,0);

        g_win.hScroll = CreateWindowExW(0, L"SCROLLBAR", L"", SBS_VERT|WS_CHILD|WS_VISIBLE,
            dr(760,80,775,480).left,dr(760,80,775,480).top,15,400,
            h,(HMENU)ID_SCROLL,nullptr,nullptr);

        g_win.hStart = CreateWindowExW(0, L"BUTTON", L"开始清理", WS_CHILD|WS_VISIBLE,
            dr(10,490,170,520).left,dr(10,490,170,520).top,160,30,
            h,(HMENU)ID_BTN_START,nullptr,nullptr);
        g_win.hStop = CreateWindowExW(0, L"BUTTON", L"停止任务", WS_CHILD|WS_VISIBLE,
            dr(190,490,350,520).left,dr(190,490,350,520).top,160,30,
            h,(HMENU)ID_BTN_STOP,nullptr,nullptr);
        EnableWindow(g_win.hStop, FALSE);
        g_win.hMain = h;
        break;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(h, &ps);
        RECT progRect = DpiRect(h,10,10,770,30);
        FillRect(hdc, &progRect, (HBRUSH)GetStockObject(GRAY_BRUSH));
        if(g_progress_total > 0)
        {
            RECT fill = progRect;
            int w = fill.right - fill.left;
            fill.right = fill.left + (w * g_progress_cur) / g_progress_total;
            FillRect(hdc, &fill, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
        }
        RECT logArea = DpiRect(h,10,80,760,480);
        FillRect(hdc, &logArea, (HBRUSH)GetStockObject(WHITE_BRUSH));
        int lineH = DpiScale(h, LINE_BASE);
        int y = logArea.top - g_scrollY;
        std::lock_guard<std::mutex> lock(g_logMtx);
        for(auto& line : g_logs)
        {
            RECT r = logArea;
            r.top = y;
            r.bottom = y + lineH;
            if(r.bottom < logArea.top) { y += lineH; continue; }
            if(r.top > logArea.bottom) break;
            COLORREF col;
            switch(line.tp)
            {
                case LOG_OK: col = RGB(0,140,0); break;
                case LOG_ERR: col = RGB(200,0,0); break;
                default: col = RGB(20,20,20);
            }
            SetTextColor(hdc, col);
            SetBkMode(hdc, TRANSPARENT);
            DrawTextExW(hdc, const_cast<LPWSTR>(line.txt.c_str()), static_cast<int>(line.txt.size()), &r, DT_VCENTER|DT_SINGLELINE, nullptr);
            y += lineH;
        }
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0;
        si.nMax = static_cast<int>(g_logs.size()) * lineH;
        si.nPage = logArea.bottom - logArea.top;
        si.nPos = g_scrollY;
        SetScrollInfo(g_win.hScroll, SB_CTL, &si, TRUE);
        EndPaint(h, &ps);
        break;
    }
    case WM_VSCROLL:
    {
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(g_win.hScroll, SB_CTL, &si);
        int lineH = DpiScale(h, LINE_BASE);
        int newY = g_scrollY;
        switch(LOWORD(wp))
        {
            case SB_LINEUP: newY -= lineH; break;
            case SB_LINEDOWN: newY += lineH; break;
            case SB_PAGEUP: newY -= si.nPage; break;
            case SB_PAGEDOWN: newY += si.nPage; break;
            case SB_THUMBTRACK: newY = si.nTrackPos; break;
        }
        int maxY = si.nMax - si.nPage;
        if(maxY < 0) maxY = 0;
        if(newY < 0) newY = 0;
        if(newY > maxY) newY = maxY;
        if(newY != g_scrollY)
        {
            g_scrollY = newY;
            g_win.RedrawLog();
        }
        break;
    }
    case WM_COMMAND:
    {
        int cid = LOWORD(wp);
        if(cid == ID_BTN_START)
        {
            if(g_runner.Running()) return 0;
            BOOL safe = SendMessageW(g_win.hChkSafe, BM_GETCHECK,0,0) == BST_CHECKED;
            BOOL net = SendMessageW(g_win.hChkNet, BM_GETCHECK,0,0) == BST_CHECKED;
            BOOL danger = SendMessageW(g_win.hChkDanger, BM_GETCHECK,0,0) == BST_CHECKED;
            for(auto& t : g_tasks)
            {
                if(t.risk == SAFE) t.enable = safe;
                else if(t.risk == NET_RISK) t.enable = net;
                else if(t.risk == DANGER) t.enable = danger;
            }
            g_logs.clear(); g_scrollY = 0;
            PushLog(LOG_NORMAL, L"==== 清理任务启动 ====");
            EnableWindow(g_win.hStart, FALSE);
            EnableWindow(g_win.hStop, TRUE);
            g_runner.Start(&g_win);
        }
        else if(cid == ID_BTN_STOP)
        {
            g_runner.Stop();
            PushLog(LOG_ERR, L"用户请求停止任务");
            g_win.RedrawLog();
        }
        break;
    }
    case WM_CLOSE:
        g_runner.Stop();
        DestroyWindow(h);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(h, msg, wp, lp);
    }
    return 0;
}

HWND CreateMainWnd()
{
    WNDCLASSEXW wc{};
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = Const::WND_CLASS;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    RegisterClassExW(&wc);
    HWND h = CreateWindowExW(0, Const::WND_CLASS, Const::WND_TITLE,
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, DpiScale(nullptr,800), DpiScale(nullptr,560),
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    ShowWindow(h, SW_SHOW);
    UpdateWindow(h);
    return h;
}

void RegisterTasks()
{
    auto env = [](LPCWSTR s){ return SysAPI::Env(std::wstring(s)); };
    g_tasks.push_back({DANGER, L"系统临时目录",
        std::format(L"if exist \"{}\" rd /s /q \"{}\"",env(L"%windir%\\temp"),env(L"%windir%\\temp")), env(L"%windir%\\temp")});
    g_tasks.push_back({DANGER, L"用户临时目录",
        std::format(L"if exist \"{}\" rd /s /q \"{}\"",env(L"%localappdata%\\Temp"),env(L"%localappdata%\\Temp")), env(L"%localappdata%\\Temp")});
    g_tasks.push_back({DANGER, L"LocalLow临时",
        std::format(L"if exist \"{}\" rd /s /q \"{}\"",env(L"%localappdata%\\LocalLow\\Temp"),env(L"%localappdata%\\LocalLow\\Temp")), env(L"%localappdata%\\LocalLow\\Temp")});
    g_tasks.push_back({SAFE, L"预读缓存Prefetch",
        std::format(L"if exist \"{}\" del /f /s /q \"{}\"",env(L"%windir%\\prefetch\\*"),env(L"%windir%\\prefetch\\*")), L""});
    g_tasks.push_back({SAFE, L"最近访问记录",
        std::format(L"if exist \"{}\" del /f /q \"{}\"",env(L"%userprofile%\\Recent\\*"),env(L"%userprofile%\\Recent\\*")), L""});
    g_tasks.push_back({SAFE, L"结束资源管理器", L"taskkill /f /im explorer.exe", L""});
    g_tasks.push_back({DANGER, L"缩略图缓存",
        std::format(L"if exist \"{}\" rd /s /q \"{}\"",env(L"%localappdata%\\Microsoft\\Windows\\Explorer"),env(L"%localappdata%\\Microsoft\\Windows\\Explorer")), env(L"%localappdata%\\Microsoft\\Windows\\Explorer")});
    g_tasks.push_back({SAFE, L"重启资源管理器", L"start explorer.exe", L""});
    g_tasks.push_back({SAFE, L"停止Windows更新", L"net stop wuauserv", L""});
    g_tasks.push_back({DANGER, L"更新下载缓存",
        std::format(L"if exist \"{}\" rd /s /q \"{}\"",env(L"%windir%\\SoftwareDistribution\\Download"),env(L"%windir%\\SoftwareDistribution\\Download")), env(L"%windir%\\SoftwareDistribution\\Download")});
    g_tasks.push_back({DANGER, L"更新数据库",
        std::format(L"if exist \"{}\" rd /s /q \"{}\"",env(L"%windir%\\SoftwareDistribution\\DataStore"),env(L"%windir%\\SoftwareDistribution\\DataStore")), env(L"%windir%\\SoftwareDistribution\\DataStore")});
    g_tasks.push_back({SAFE, L"启动Windows更新", L"net start wuauserv", L""});
    g_tasks.push_back({DANGER, L"蓝屏转储",
        std::format(L"if exist \"{}\" rd /s /q \"{}\"",env(L"%windir%\\Minidump"),env(L"%windir%\\Minidump")), env(L"%windir%\\Minidump")});
    g_tasks.push_back({DANGER, L"Edge缓存",
        std::format(L"if exist \"{}\" rd /s /q \"{}\"",env(L"%localappdata%\\Microsoft\\Edge\\User Data\\Cache"),env(L"%localappdata%\\Microsoft\\Edge\\User Data\\Cache")), env(L"%localappdata%\\Microsoft\\Edge\\User Data\\Cache")});
    g_tasks.push_back({DANGER, L"Chrome缓存",
        std::format(L"if exist \"{}\" rd /s /q \"{}\"",env(L"%localappdata%\\Google\\Chrome\\User Data\\Cache"),env(L"%localappdata%\\Google\\Chrome\\User Data\\Cache")), env(L"%localappdata%\\Google\\Chrome\\User Data\\Cache")});
    g_tasks.push_back({SAFE, L"DISM基础组件清理", L"dism /online /startcomponentcleanup", L""});
    g_tasks.push_back({SAFE, L"SFC系统修复", L"sfc /scannow", L""});
    g_tasks.push_back({NET_RISK, L"重置Winsock", L"netsh winsock reset", L""});
    g_tasks.push_back({NET_RISK, L"重置IP栈", L"netsh int ip reset", L""});
    g_tasks.push_back({SAFE, L"刷新DNS", L"ipconfig /flushdns", L""});
    g_tasks.push_back({DANGER, L"关闭休眠", L"powercfg /hibernate off", L""});
}

void Logger::Init()
{
    std::lock_guard<std::mutex> lock(m);
    f.open(L"PureWipe.log", std::ios::trunc);
    SYSTEMTIME st; GetLocalTime(&st);
    f << std::format(L"[{:04}-{:02}-{:02} {:02}:{:02}:{:02}] 程序启动\n",
        st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
}
void Logger::Write(const std::wstring& s)
{
    std::lock_guard<std::mutex> lock(m);
    if(f.is_open()) f << s << L"\n";
}
void Logger::Close()
{
    std::lock_guard<std::mutex> lock(m);
    if(f.is_open()) f.close();
}

int WINAPI wWinMain(
[[maybe_unused]] HINSTANCE hInst,
[[maybe_unused]] HINSTANCE hPrev,
[[maybe_unused]] LPWSTR cmd,
[[maybe_unused]] int nShow)
{
    g_logger.Init();
    if(!SysAPI::IsAdmin())
    {
        PushLog(LOG_ERR, L"未检测管理员权限，尝试自动提权");
        if(SysAPI::ReRunAdmin())
        {
            g_logger.Close();
            return 0;
        }
        PushLog(LOG_ERR, L"提权失败，部分清理功能无法使用");
    }
    RegisterTasks();
    [[maybe_unused]] HWND win = CreateMainWnd();
    MSG msg{};
    while(GetMessage(&msg,nullptr,0,0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    g_logger.Close();
    return static_cast<int>(msg.wParam);
}
