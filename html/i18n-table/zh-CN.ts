import moment from 'moment';

// @ts-ignore
function formatInt(int: number) {
    if (int < 10) {
        return `0${int}`;
    }
    return `${int}`;
}


// @ts-ignore
window.i18n = window.i18n || {};
// @ts-ignore
window.i18n.zhCN = (() => {
    const tableState: { [key: string]: { s: string, f?: CallableFunction } } = {
        formatDurationFunction: {
            s: "formatDurationFunction", f: (time: number) => {
                const seconds = moment.duration(time).seconds();
                const minutes = moment.duration(time).minutes();
                const hours = moment.duration(time).hours();
                const days = moment.duration(time).days();
                const months = moment.duration(time).months();
                const years = moment.duration(time).years();
                if (years > 0) {
                    return `${years}年-${months}月-${days}天 ${formatInt(hours)}时:${formatInt(minutes)}分:${formatInt(seconds)}秒`;
                }
                if (months > 0) {
                    return `${months}月-${days}天 ${formatInt(hours)}时:${formatInt(minutes)}分:${formatInt(seconds)}秒`;
                }
                if (days > 0) {
                    return `${days}天 ${formatInt(hours)}时:${formatInt(minutes)}分:${formatInt(seconds)}秒`;
                }
                if (hours > 0) {
                    return `${formatInt(hours)}时:${formatInt(minutes)}分:${formatInt(seconds)}秒`;
                }
                if (minutes > 0) {
                    return `${formatInt(minutes)}分:${formatInt(seconds)}秒`;
                }
                return `00分:${formatInt(seconds)}秒`;
            }
        },
        emptyFilterFunction: {
            s: "emptyFilterFunction", f: (s: string) => {
                return s === "<empty>" ? "无" : s;
            }
        },
        emptyFilterPingFunction: {
            s: "emptyFilterPingFunction", f: (s: string) => {
                return s === "<empty>" ? "无" : (s + "ms");
            }
        },
        Socks5BalancerAsio: {s: "Socks5BalancerAsio"},
        allConnectCountNotice: {s: "当前运行中的连接数:"},
        nowRuleNotice: {s: "当前规则:"},
        buttonChange: {s: "修改规则"},
        backend: {s: "当前管理后端:"},
        Flush: {s: "刷新"},
        AutoFlush: {s: "自动刷新", f: (b: boolean) => b ? `自动刷新 ⚪启用` : `自动刷新 ⚫禁用`},
        NoUsableServer: {s: "⚠警告: 完全没有可用的服务器 !!! "},
        NoServer: {s: "未配置上游服务器列表 ..."},
        NoDataComeFromServer: {s: "无法从服务器获取信息."},
        DisableConnectTestMode: {s: "当前运行在 [DisableConnectTest] 模式."},
        NumberSerial: {s: "编号："},
        Host_Port: {s: "主机:端口"},
        name: {s: "服务器名称"},
        online: {s: "在线"},
        work: {s: "工作中"},
        runCount: {s: "连接数"},
        lastTCPCheckTime: {s: "上次TCP检查时间"},
        lastTCPPing: {s: "后端Ping"},
        afterLastTCPCheckTime: {s: "已过去"},
        lastConnectCheckTime: {s: "上次可连通性检查时间"},
        lastConnectPing: {s: "代理Ping"},
        afterLastConnectCheckTime: {s: "已过去"},
        ManualDisable: {s: "手动禁用"},
        Usable: {s: "可用性"},
        CloseConnect: {s: "&emsp;关闭连接&emsp;"},
        Select: {s: "选择"},
        Check: {s: "检查"},
        data: {s: "总数据量"},
        speedMax: {s: "最大速度"},
        speed: {s: "当前速度"},
        online_Unknown: {s: "未知"},
        online_True: {s: "在线"},
        online_False: {s: "离线"},
        work_Unknown: {s: "未知"},
        work_True: {s: "工作中"},
        work_False: {s: "不工作"},
        Disabled: {s: "已禁用"},
        Enabled: {s: "已启用"},
        EnableIt: {s: "启用它"},
        Enable: {s: "启用"},
        Disable: {s: "禁用"},
        Usable_True: {s: "可用"},
        Usable_False: {s: "不可用"},
        // CloseConnect: {s: "Close Connect"},
        UseNow: {s: "&emsp;&emsp;使用&emsp;&emsp;"},
        CheckNow: {s: "&emsp;&emsp;检测&emsp;&emsp;"},
        CleanCheckState: {s: "清除检测状态"},
        ForceCloseAllConnect: {s: "强制关闭所有连接"},
        ForceCheckAllNow: {s: "强制检测所有连接"},
        lastConnectServer: {s: "上次连接的上游服务器:"},
        lastUseUpstreamIndex: {s: "上次使用的上游服务器编号:"},
        TraditionMode: {s: "传统模式"},
        MixedMode: {s: "混合模式"},
        TcpRelayMode: {s: "Tcp中继模式:"},
        nowTime: {s: "当前时间:"},
        startTime: {s: "启动时间:"},
        runTime: {s: "运行时长:"},
        lastConnectComeTime: {s: "上一连接创建时间:"},
        isSleeping: {s: "是否休眠:"},
        sleeping: {s: "休眠中"},
        listenOn: {s: "监听于:"},
        totalHistoryConnectCount: {s: "历史累计连接数:"},
        ClientConnectInfo: {s: "客户端连接信息"},
        Host: {s: "主机"},
        lastServer: {s: "上次连接的上游服务器"},
        newServer: {s: "下一个上游服务器"},
        nowRule: {s: "当前规则"},
        newRule: {s: "新规则"},
        running: {s: "运行中"},
        closeConnect: {s: "关闭连接"},
        // data: {s: "data"},
        // speedMax: {s: "speedMax"},
        // speed: {s: "speed"},
        // detail: {s: "detail"},
        // CloseConnect: {s: "CloseConnect"},
        // detail: {s: "detail"},
        ListenPortInfo: {s: "端口连接信息"},
        AuthUserInfo: {s: "用户验证信息"},
        AuthUserInfoId: {s: "编号"},
        AuthUserInfoName: {s: "用户名"},
        AuthUserInfoPwd: {s: "密码"},
        AuthUserInfoBase64String: {s: "Base64验证字符串"},
        FastIssueResolve: {s: "快速解决问题"},
        WebPageOpenVerySlow: {s: "网页打开太慢啦 :"},
        // ForceCloseAllConnect: {s: "Force Close All Connect"},
        SeemsLikeServerStateNotUpdate: {s: "服务器状态信息好像没有更新 :"},
        IWantToDisableAllServer: {s: "我想禁用所有服务器 :"},
        DisableAllServer: {s: "禁用所有服务器"},
        IWantToEnableAllServer: {s: "我想启用所有服务器 :"},
        EnableAllServer: {s: "启用所有服务器"},
        clickToShowDetail: {s: "点击查看详情"},
        timeMs: {s: "毫秒"},
        total: {s: "累计"},
    };
    return tableState;
})();



