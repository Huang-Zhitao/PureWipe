#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma execution_character_set("utf-8")
#include <objbase.h>
#include <windows.h>
#include <shellapi.h>
#include <conio.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <cstdio>
#include <iomanip>

// 全局进度条工具函数
void ShowProgress(int current, int total, const char* taskName)
{
    system("cls");
    float percent = (static_cast<float>(current) / total) * 100;
    int barWidth = 40;
    int filled = static_cast<int>(barWidth * current / total);

    std::cout << "========================================================" << std::endl;
    std::cout << "当前任务：" << taskName << std::endl;
    std::cout << "进度：[";
    for (int i = 0; i < barWidth; i++)
    {
        if (i < filled) std::cout << "#";
        else std::cout << " ";
    }
    std::cout << "] " << std::fixed << std::setprecision(1) << percent << "%" << std::endl;
    std::cout << "步骤 " << current << "/" << total << std::endl;
    std::cout << "========================================================" << std::endl << std::endl;
}

void SetConsoleUTF8()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::ios::sync_with_stdio(false);
}

bool IsAdmin()
{
    const DWORD TokenElevationIdx = 20;
    typedef struct _TOKEN_ELEVATION
    {
        DWORD TokenIsElevated;
    } TOKEN_ELEVATION, *PTOKEN_ELEVATION;
    BOOL bIsAdmin = FALSE;
    HANDLE hToken = NULL;
    do
    {
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
            break;
        TOKEN_ELEVATION elevation = { 0 };
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (!GetTokenInformation(hToken, (TOKEN_INFORMATION_CLASS)TokenElevationIdx, &elevation, cbSize, &cbSize))
            break;
        bIsAdmin = elevation.TokenIsElevated;
    } while (false);
    if (hToken) CloseHandle(hToken);
    return bIsAdmin;
}

void RerunAsAdmin()
{
    char szPath[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, szPath, MAX_PATH);
    HINSTANCE hInst = ShellExecuteA(NULL, "runas", szPath, NULL, NULL, SW_SHOWNORMAL);
    if ((reinterpret_cast<ULONG_PTR>(hInst)) <= 32)
    {
        MessageBoxA(NULL, "管理员权限获取失败！\n请手动右键以管理员身份运行。", "错误", MB_ICONWARNING);
    }
}

int ExecuteSystemCommand(const char* cmd)
{
    if (!cmd || strlen(cmd) == 0) return -1;
    int retCode = std::system(cmd);
    if (retCode == -1)
    {
        std::cerr << "命令执行失败：" << cmd << " | 错误码：" << GetLastError() << std::endl;
    }
    return retCode;
}

void SafeRecreateDirectory(const char* dirPath)
{
    if (!dirPath || strlen(dirPath) == 0) return;
    std::string rdCmd = "rd /s /q \"" + std::string(dirPath) + "\" 2>nul";
    ExecuteSystemCommand(rdCmd.c_str());
    std::string mdCmd = "md \"" + std::string(dirPath) + "\"";
    ExecuteSystemCommand(mdCmd.c_str());
}

// 安全步骤（无确认自动执行）
void RunSafeStep(int cur, int total, const char* desc, const char* cmd)
{
    ShowProgress(cur, total, desc);
    std::cout << "【安全清理】执行指令：" << cmd << std::endl;
    int retCode = ExecuteSystemCommand(cmd);
    std::cout << "\n【完成】" << desc << " 返回码：" << retCode << "\n" << std::endl;
    Sleep(300);
}

// 高危删除步骤（文件永久删除，强制确认）
void RunDangerStep(int cur, int total, const char* desc, const char* cmd, const char* recreateDir = nullptr)
{
    ShowProgress(cur, total, desc);
    std::cout << "【⚠️ 高危删除操作】" << desc << std::endl;
    std::cout << "指令：" << cmd << std::endl;
    std::cout << "文件删除无法恢复！确认执行(Y/y)，跳过(N/n)：";
    char ch = 'N';
    if (!(std::cin >> ch))
    {
        std::cin.clear();
        std::cin.ignore(1024, '\n');
        ch = 'N';
    }
    if (ch == 'Y' || ch == 'y')
    {
        std::cout << "开始执行高危清理..." << std::endl;
        int retCode = ExecuteSystemCommand(cmd);
        std::cout << "\n【完成】" << desc << " 返回码：" << retCode << std::endl;
        if (recreateDir)
        {
            SafeRecreateDirectory(recreateDir);
            std::cout << "重建目录：" << recreateDir << " 完成" << std::endl;
        }
    }
    else
    {
        std::cout << "已跳过本条高危清理。" << std::endl;
    }
    std::cout << std::endl;
    Sleep(300);
}

// 次级风险：网络重置（企业静态IP/域设备专用警告）
void RunNetRiskStep(int cur, int total, const char* desc, const char* cmd)
{
    ShowProgress(cur, total, desc);
    std::cout << "【⚠️ 次级风险 · 企业设备慎用】" << desc << std::endl;
    std::cout << "指令：" << cmd << std::endl;
    std::cout << "警告：静态IP/域控/财务工控机执行后会丢失内网配置、断业务！确认执行(Y/y)，跳过(N/n)：";
    char ch = 'N';
    if (!(std::cin >> ch))
    {
        std::cin.clear();
        std::cin.ignore(1024, '\n');
        ch = 'N';
    }
    if (ch == 'Y' || ch == 'y')
    {
        std::cout << "开始执行网络重置..." << std::endl;
        int retCode = ExecuteSystemCommand(cmd);
        std::cout << "\n【完成】" << desc << " 返回码：" << retCode << std::endl;
    }
    else
    {
        std::cout << "已跳过网络重置，保护企业内网配置" << std::endl;
    }
    std::cout << std::endl;
    Sleep(300);
}

void CleanupAndDestroySelf()
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "✅ 全部清理&修复流程执行完毕" << std::endl;
    std::cout << "提示：如需恢复休眠功能，管理员CMD执行 powercfg /hibernate on" << std::endl;
    std::cout << "按任意键退出程序..." << std::endl;
    std::cout << "========================================" << std::endl;
    _getch();
    FreeConsole();
    ExitProcess(0);
}

int main()
{
    SetConsoleUTF8();
    if (!IsAdmin())
    {
        std::cout << "未检测管理员权限，自动拉起管理员权限窗口运行..." << std::endl;
        RerunAsAdmin();
        Sleep(1000);
        return 0;
    }

    std::cout << "==================================================================" << std::endl;
    std::cout << "                    LEATHY™ 工作室 版权所有                          " << std::endl;
    std::cout << "              PureWipe 纯净强力系统清理工具 v1.3.0 修复版          " << std::endl;
    std::cout << "==================================================================" << std::endl;
    std::cout << "更新日志：" << std::endl;
    std::cout << "1. 修复v1.2.2全盘递归删除*.tmp误删业务文件漏洞" << std::endl;
    std::cout << "2. 移除批量注册全部System32 DLL卡死逻辑" << std::endl;
    std::cout << "3. 移除自动清理Downloads安装包、压缩包风险逻辑" << std::endl;
    std::cout << "4. 新增控制台实时进度条，全程可视化清理进度" << std::endl;
    std::cout << "5. 网络重置独立次级风险确认，保护企业静态IP/域主机" << std::endl;
    std::cout << "6. 分层风险：安全操作 / 网络次级风险 / 文件高危删除" << std::endl;
    std::cout << "==================================================================" << std::endl;
    std::cout << "⚠️ 全局风险说明：" << std::endl;
    std::cout << "1. rd /s /q 删除文件永久无法恢复；高危项全部手动确认" << std::endl;
    std::cout << "2. ResetBase执行后无法卸载历史Windows更新" << std::endl;
    std::cout << "3. 企业办公静态IP电脑强烈建议跳过全部网络重置步骤" << std::endl;
    std::cout << "==================================================================" << std::endl;

    char globalConfirm = 'N';
    std::cout << "\n确认已知全部风险并进入工具流程？输入 Y/y 继续，其他按键退出：";
    if (!(std::cin >> globalConfirm))
    {
        std::cin.clear();
        std::cin.ignore(1024, '\n');
    }
    if (globalConfirm != 'Y' && globalConfirm != 'y')
    {
        std::cout << "操作已取消，程序退出..." << std::endl;
        CleanupAndDestroySelf();
    }

    // 总步骤计数（所有任务总数）
    const int TOTAL_STEP = 28;
    int curStep = 0;

    // ========== 阶段1：基础临时垃圾清理 ==========
    std::cout << "\n===== 第一阶段：基础临时垃圾清理 =====" << std::endl;
    curStep++; RunDangerStep(curStep, TOTAL_STEP, "系统临时目录 Windows\\Temp", "rd /s /q \"%windir%\\temp\"", "%windir%\\temp");
    curStep++; RunDangerStep(curStep, TOTAL_STEP, "用户临时目录 Local\\Temp", "rd /s /q \"%localappdata%\\Temp\"", "%localappdata%\\Temp");
    curStep++; RunDangerStep(curStep, TOTAL_STEP, "低权限临时目录 LocalLow\\Temp", "rd /s /q \"%localappdata%\\LocalLow\\Temp\"", "%localappdata%\\LocalLow\\Temp");
    curStep++; RunSafeStep(curStep, TOTAL_STEP, "系统预读缓存 Prefetch", "del /f /s /q \"%windir%\\prefetch\\*\"");
    curStep++; RunSafeStep(curStep, TOTAL_STEP, "用户最近访问记录", "del /f /s /q \"%userprofile%\\Recent\\*\"");

    // ========== 阶段2：深度系统缓存冗余清理 ==========
    std::cout << "\n===== 第二阶段：深度系统冗余清理 =====" << std::endl;
    curStep++; RunSafeStep(curStep, TOTAL_STEP, "终止资源管理器", "taskkill /f /im explorer.exe");
    curStep++; RunDangerStep(curStep, TOTAL_STEP, "图片视频缩略图缓存", "rd /s /q \"%localappdata%\\Microsoft\\Windows\\Explorer\"", "%localappdata%\\Microsoft\\Windows\\Explorer");
    Sleep(500);
    curStep++; RunSafeStep(curStep, TOTAL_STEP, "重启资源管理器", "start explorer.exe");
    curStep++; RunSafeStep(curStep, TOTAL_STEP, "停止Windows更新服务", "net stop wuauserv");
    curStep++; RunDangerStep(curStep, TOTAL_STEP, "更新下载缓存 SoftwareDistribution\\Download", "rd /s /q \"%windir%\\SoftwareDistribution\\Download\"", "%windir%\\SoftwareDistribution\\Download");
    curStep++; RunDangerStep(curStep, TOTAL_STEP, "更新数据库缓存 DataStore", "rd /s /q \"%windir%\\SoftwareDistribution\\DataStore\"", "%windir%\\SoftwareDistribution\\DataStore");
    curStep++; RunSafeStep(curStep, TOTAL_STEP, "重启Windows更新服务", "net start wuauserv");
    curStep++; RunDangerStep(curStep, TOTAL_STEP, "系统蓝屏转储 Minidump", "rd /s /q \"%windir%\\Minidump\"", "%windir%\\Minidump");
    curStep++; RunDangerStep(curStep, TOTAL_STEP, "微软商店缓存", "rd /s /q \"%localappdata%\\Microsoft\\Windows\\Store\\Cache\"", "%localappdata%\\Microsoft\\Windows\\Store\\Cache");
    curStep++; RunSafeStep(curStep, TOTAL_STEP, "重置微软商店", "wsreset.exe");
    curStep++; RunDangerStep(curStep, TOTAL_STEP, "Edge浏览器缓存", "rd /s /q \"%localappdata%\\Microsoft\\Edge\\User Data\\Cache\"", "%localappdata%\\Microsoft\\Edge\\User Data\\Cache");
    curStep++; RunDangerStep(curStep, TOTAL_STEP, "Chrome浏览器缓存", "rd /s /q \"%localappdata%\\Google\\Chrome\\User Data\\Cache\"", "%localappdata%\\Google\\Chrome\\User Data\\Cache");

    // ========== 阶段3：巨型文件空间释放 ==========
    std::cout << "\n===== 第三阶段：巨型文件强力清理 =====" << std::endl;
    curStep++; RunSafeStep(curStep, TOTAL_STEP, "WinSxS旧组件清理", "dism /online /cleanup-image /startcomponentcleanup");
    curStep++; RunSafeStep(curStep, TOTAL_STEP, "深度清理废弃更新备份", "dism /online /cleanup-image /spsuperseded");
    curStep++; RunSafeStep(curStep, TOTAL_STEP, "分析组件存储冗余", "dism /online /cleanup-image /analyzecomponentstore");
    curStep++; RunDangerStep(curStep, TOTAL_STEP, "ResetBase重置组件基准（无法卸载历史更新）", "dism /online /cleanup-image /startcomponentcleanup /resetbase");
    curStep++; RunDangerStep(curStep, TOTAL_STEP, "删除旧系统备份 Windows.old", "rd /s /q C:\\Windows.old");
    curStep++; RunSafeStep(curStep, TOTAL_STEP, "关闭休眠释放内存空间", "powercfg /hibernate off");
    curStep++; RunDangerStep(curStep, TOTAL_STEP, "清空全盘回收站", "rd /s /q C:\\$Recycle.Bin");
    curStep++; RunSafeStep(curStep, TOTAL_STEP, "查询C盘剩余空间", "fsutil volume diskfree C:");

    // ========== 阶段4：系统修复（区分网络次级风险） ==========
    std::cout << "\n===== 第四阶段：系统完整性修复 =====" << std::endl;
    curStep++; RunSafeStep(curStep, TOTAL_STEP, "DISM 镜像轻度检测", "dism /Online /Cleanup-Image /CheckHealth");
    curStep++; RunSafeStep(curStep, TOTAL_STEP, "DISM 深度扫描镜像", "dism /Online /Cleanup-Image /ScanHealth");
    curStep++; RunSafeStep(curStep, TOTAL_STEP, "DISM 在线修复系统镜像", "dism /Online /Cleanup-Image /RestoreHealth");
    curStep++; RunSafeStep(curStep, TOTAL_STEP, "SFC校验修复系统文件", "sfc /scannow");

    // 网络重置：次级风险独立确认
    std::cout << "\n===== 网络修复模块（企业设备慎选） =====" << std::endl;
    curStep++; RunNetRiskStep(curStep, TOTAL_STEP, "重置Winsock网络套接字", "netsh winsock reset");
    curStep++; RunNetRiskStep(curStep, TOTAL_STEP, "重置IP栈（清空静态IP/网关/DNS）", "netsh int ip reset");
    curStep++; RunSafeStep(curStep, TOTAL_STEP, "刷新本地DNS缓存", "ipconfig /flushdns");

    // 收尾磁盘与更新修复
    curStep++; RunSafeStep(curStep, TOTAL_STEP, "磁盘离线文件系统修复", "chkdsk C: /f /offlinescanandfix");
    curStep++; RunDangerStep(curStep, TOTAL_STEP, "清理老旧系统还原点", "vssadmin delete shadows /for=C: /oldest /quiet");

    CleanupAndDestroySelf();
    return 0;
}