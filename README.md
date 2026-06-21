# PureWipe v1.2.1
LEATHY™ Studio 出品 Windows 纯净强力系统清理工具

## 项目特点
1. 极致轻量化
   - 编译成品仅 64KB+，运行内存占用不足 1MB
   - 无后台驻留、无广告、无捆绑、不读写隐私
2. 全自动管理员提权
   未获取管理员权限时自动弹窗申请权限，无需手动右键运行
3. 四层深度清理+系统修复
   - 阶段1：全盘临时文件、日志、崩溃转储、系统/用户临时目录
   - 阶段2：缩略图缓存、Windows更新缓存、Edge/Chrome浏览器缓存、商店缓存
   - 阶段3：WinSxS组件清理、休眠文件、Windows.old、回收站、大容量冗余文件
   - 阶段4：DISM系统镜像修复、SFC系统文件校验、网络重置、磁盘修复、DLL重新注册
4. 高危操作二次确认
   Windows.old、全盘删除、组件重置等高风险操作单独弹窗确认，误操作可一键跳过
5. 兼容全版本 Windows
   Win10 全版本 / Win11 正式版 / Win11 Canary 预览版(26200+)
   ResetBase 设为可选高危项，新版系统报错不阻断流程

## 清理收益
- 休眠关闭：释放与内存同等大小空间
- Windows.old：10~35GB
- WinSxS + 更新缓存：5~15GB
- 浏览器/缩略图/临时日志：2~8GB

## 编译方式
环境：MinGW-w64
```bash
g++ PureWipe.cpp -o PureWipe.exe -luser32 -lshell32 -ladvapi32