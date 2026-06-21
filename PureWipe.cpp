#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0600
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma execution_character_set("utf-8")

// 修复 REFIID 未定义，必须提前引入COM头
#include <objbase.h>
#include <windows.h>
#include <shellapi.h>
#include <conio.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <cstdio>

// 控制台UTF8设置
void SetConsoleUTF8()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::ios::sync_with_stdio(false);
}

// 管理员权限检测：分离自定义枚举，杜绝类型转换冲突
bool IsAdmin()
{
    // 自定义局部枚举值，不与系统原生TOKEN_INFORMATION_CLASS混用
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
        // 直接传数字DWORD，彻底规避枚举类型转换报错
        if (!GetTokenInformation(hToken, (TOKEN_INFORMATION_CLASS)TokenElevationIdx, &elevation, cbSize, &cbSize))
            break;

        bIsAdmin = elevation.TokenIsElevated;
    } while (false);

    if (hToken)
    {
        CloseHandle(hToken);
    }
    return bIsAdmin;
}

// 提权重启（ANSI兼容，去除宽字符依赖）
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

// 执行系统命令（标准system，废弃_wsystem）
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

// 删除重建目录
void SafeRecreateDirectory(const char* dirPath)
{
    if (!dirPath || strlen(dirPath) == 0) return;
    std::string rdCmd = "rd /s /q \"" + std::string(dirPath) + "\"";
    ExecuteSystemCommand(rdCmd.c_str());
    std::string mdCmd = "md \"" + std::string(dirPath) + "\"";
    ExecuteSystemCommand(mdCmd.c_str());
}

// 安全步骤输出
void RunSafeStep(const char* desc, const char* cmd)
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "【正在处理】" << desc << std::endl;
    std::cout << "执行指令：" << cmd << std::endl;
    std::cout << "========================================" << std::endl;

    int retCode = ExecuteSystemCommand(cmd);
    std::cout << "\n【完成】" << desc << " 状态码：" << retCode << std::endl;
    fflush(stdout);
    fflush(stderr);
}

// 高危删除步骤
void RunDangerStep(const char* desc, const char* cmd, const char* recreateDir = nullptr)
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "【⚠️ 高危删除操作】" << desc << std::endl;
    std::cout << "指令：" << cmd << std::endl;
    std::cout << "该操作永久删除文件，无法恢复！确认执行(Y/y)，跳过(N/n)：";

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
        std::cout << "\n【完成】" << desc << " 状态码：" << retCode << std::endl;
        if (recreateDir)
        {
            SafeRecreateDirectory(recreateDir);
            std::cout << "【重建目录】" << recreateDir << " 完成" << std::endl;
        }
    }
    else
    {
        std::cout << "已跳过本条高危清理。" << std::endl;
    }
    std::cout << "========================================\n";
    fflush(stdout);
    fflush(stderr);
}

// 程序收尾退出
void CleanupAndDestroySelf()
{
    fflush(stdin);
    fflush(stdout);
    fflush(stderr);
    std::cout << "\n按任意键退出程序..." << std::endl;
    _getch();
    FreeConsole();
    ExitProcess(0);
}

// 换回标准main，移除wmain，不再需要-municode参数
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
    std::cout << "              PureWipe 纯净强力系统清理工具 v1.2.2 [稳定版]          " << std::endl;
    std::cout << "==================================================================" << std::endl;
    std::cout << "运行机制：所有清理运算交由系统CPU独立执行，主程序极低内存占用" << std::endl;
    std::cout << "四大模块：基础临时垃圾 | 深度系统冗余 | 巨型文件空间释放 | 全量系统修复" << std::endl;
    std::cout << "==================================================================" << std::endl;
    std::cout << "⚠️ 全局风险说明：" << std::endl;
    std::cout << "1. 工具使用 rd /s /q 强制删除，文件删除后无法恢复；" << std::endl;
    std::cout << "2. 所有大容量删除高危项会单独确认，不同意则自动跳过；" << std::endl;
    std::cout << "3. ResetBase极致清理执行后，将无法卸载已安装的历史Windows更新；" << std::endl;
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
        std::cout << "操作已取消，清空缓存销毁程序..." << std::endl;
        CleanupAndDestroySelf();
    }

    // 阶段1 基础临时垃圾清理
    std::cout << "\n==================== 第一阶段：基础临时垃圾清理 ====================" << std::endl;
    RunDangerStep(
        "全盘tmp、备份、日志、崩溃转储、校验临时文件",
        "del /f /s /q C:\\*.tmp C:\\*._mp C:\\*.log C:\\*.chk C:\\*.old C:\\*.bak C:\\*.syd C:\\*.dmp"
    );
    RunDangerStep("系统临时目录 C:\\Windows\\Temp", "rd /s /q \"%windir%\\temp\"", "%windir%\\temp");
    RunDangerStep("用户本地临时文件夹 Local\\Temp", "rd /s /q \"%localappdata%\\Temp\"", "%localappdata%\\Temp");
    RunDangerStep("低权限程序临时目录 LocalLow\\Temp", "rd /s /q \"%localappdata%\\LocalLow\\Temp\"", "%localappdata%\\LocalLow\\Temp");
    RunSafeStep("系统预读加速缓存 Prefetch", "del /f /s /q \"%windir%\\prefetch\\*\"");
    RunSafeStep("用户最近访问记录快捷方式", "del /f /s /q \"%userprofile%\\Recent\\*\"");

    // 阶段2 深度系统缓存冗余清理
    std::cout << "\n==================== 第二阶段：深度系统冗余清理 ====================" << std::endl;
    RunSafeStep("终止资源管理器进程，准备清空缩略图缓存", "taskkill /f /im explorer.exe");
    RunDangerStep("图片视频缩略图缓存文件夹", "rd /s /q \"%localappdata%\\Microsoft\\Windows\\Explorer\"", "%localappdata%\\Microsoft\\Windows\\Explorer");
    Sleep(500);
    RunSafeStep("重新启动资源管理器", "start explorer.exe");
    RunSafeStep("停止Windows更新服务", "net stop wuauserv");
    RunDangerStep("Windows更新下载安装包缓存", "rd /s /q \"%windir%\\SoftwareDistribution\\Download\"", "%windir%\\SoftwareDistribution\\Download");
    RunDangerStep("Windows更新数据库缓存", "rd /s /q \"%windir%\\SoftwareDistribution\\DataStore\"", "%windir%\\SoftwareDistribution\\DataStore");
    RunSafeStep("重启Windows更新服务", "net start wuauserv");
    RunDangerStep("系统蓝屏崩溃小型转储文件 Minidump", "rd /s /q \"%windir%\\Minidump\"", "%windir%\\Minidump");
    RunDangerStep("微软应用商店缓存目录", "rd /s /q \"%localappdata%\\Microsoft\\Windows\\Store\\Cache\"", "%localappdata%\\Microsoft\\Windows\\Store\\Cache");
    RunSafeStep("重置微软商店组件", "wsreset.exe");
    RunDangerStep("Edge浏览器网页缓存", "rd /s /q \"%localappdata%\\Microsoft\\Edge\\User Data\\Cache\"", "%localappdata%\\Microsoft\\Edge\\User Data\\Cache");
    RunDangerStep("Chrome谷歌浏览器网页缓存", "rd /s /q \"%localappdata%\\Google\\Chrome\\User Data\\Cache\"", "%localappdata%\\Google\\Chrome\\User Data\\Cache");
    RunDangerStep("下载文件夹安装包、压缩包垃圾", "del /f /s /q \"%userprofile%\\Downloads\\*.exe\" \"%userprofile%\\Downloads\\*.zip\" \"%userprofile%\\Downloads\\*.rar\" \"%userprofile%\\Downloads\\*.7z\"");

// ====================== 第三阶段：巨型文件强力清理 ======================
std::cout << "\n==================== 第三阶段：巨型文件强力清理（释放最大空间） ====================" << std::endl;
RunSafeStep("清理系统过期组件库 WinSxS（安全删除旧补丁、旧驱动备份）",
    "dism /online /cleanup-image /startcomponentcleanup");
RunSafeStep("深度清理WinSxS废弃更新备份",
    "dism /online /cleanup-image /spsuperseded");
RunSafeStep("分析组件存储冗余，给出清理建议",
    "dism /online /cleanup-image /analyzecomponentstore");

// 保留ResetBase，设为高危确认项，新版系统会报错但不阻断流程
RunDangerStep("【极致空间释放】重置组件基准ResetBase（执行后无法卸载历史更新）",
    "dism /online /cleanup-image /startcomponentcleanup /resetbase");

RunDangerStep("删除系统升级旧系统备份 Windows.old（可释放10-35G）",
    "rd /s /q C:\\Windows.old");
RunSafeStep("关闭休眠功能，删除hiberfil.sys（释放和内存同等大小空间）",
    "powercfg /hibernate off");
RunDangerStep("全盘回收站所有文件强制清空",
    "rd /s /q C:\\$Recycle.Bin");
RunSafeStep("查询当前C盘剩余空间",
    "fsutil volume diskfree C:");
    // 阶段4 全套完整系统修复
    std::cout << "\n==================== 第四阶段：全维度系统完整修复 ====================" << std::endl;
    RunSafeStep("DISM：检测系统镜像轻微损坏", "dism /Online /Cleanup-Image /CheckHealth");
    RunSafeStep("DISM：全盘深度扫描系统镜像完整性", "dism /Online /Cleanup-Image /ScanHealth");
    RunSafeStep("DISM：在线下载修复损坏系统镜像", "dism /Online /Cleanup-Image /RestoreHealth");
    RunSafeStep("SFC：校验并修复全部系统核心文件", "sfc /scannow");
    RunSafeStep("重置Windows网络套接字Winsock，修复网络异常", "netsh winsock reset");
    RunSafeStep("重置IP、DNS网络配置", "netsh int ip reset");
    RunSafeStep("刷新DNS缓存", "ipconfig /flushdns");
    RunSafeStep("重置Windows更新服务修复异常", "net stop wuauserv && net start wuauserv");
    RunSafeStep("磁盘自动校验修复文件系统错误", "chkdsk C: /f /offlinescanandfix");
    RunSafeStep("清理老旧系统还原点释放空间", "vssadmin delete shadows /for=C: /oldest /quiet");
    RunSafeStep("批量重新注册系统DLL组件，修复各类弹窗报错", "for %i in (%windir%\\system32\\*.dll) do regsvr32 /s %i");
    RunSafeStep("清理DISM在线修复残留下载缓存","net stop wuauserv && rd /s /q \"%windir%\\SoftwareDistribution\\Download\" && net start wuauserv");

    std::cout << "\n======================================================================" << std::endl;
    std::cout << "✅ 全部清理&修复流程执行完毕，即将清空程序内存并销毁进程" << std::endl;
    std::cout << "LEATHY™ 工作室 版权所有" << std::endl;
    std::cout << "提示：如需恢复休眠功能，管理员CMD执行 powercfg /hibernate on" << std::endl;
    std::cout << "======================================================================" << std::endl;

    CleanupAndDestroySelf();
    return 0;
}