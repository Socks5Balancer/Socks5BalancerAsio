"use strict";
function formatInt(int) {
    if (int < 10) {
        return `0${int}`;
    }
    return `${int}`;
}
// @ts-ignore
window.i18n = window.i18n || {};
// @ts-ignore
window.i18n.enUS = (() => {
    const tableState = {
        formatDurationFunction: {
            s: "formatDurationFunction", f: (time) => {
                const seconds = globalThis.moment.duration(time).seconds();
                const minutes = globalThis.moment.duration(time).minutes();
                const hours = globalThis.moment.duration(time).hours();
                const days = globalThis.moment.duration(time).days();
                const months = globalThis.moment.duration(time).months();
                const years = globalThis.moment.duration(time).years();
                if (years > 0) {
                    return `${years}Y-${months}M-${days}Day ${formatInt(hours)}h:${formatInt(minutes)}m:${formatInt(seconds)}s`;
                }
                if (months > 0) {
                    return `${months}M-${days}Day ${formatInt(hours)}h:${formatInt(minutes)}m:${formatInt(seconds)}s`;
                }
                if (days > 0) {
                    return `${days}Day ${formatInt(hours)}h:${formatInt(minutes)}m:${formatInt(seconds)}s`;
                }
                if (hours > 0) {
                    return `${formatInt(hours)}h:${formatInt(minutes)}m:${formatInt(seconds)}s`;
                }
                if (minutes > 0) {
                    return `${formatInt(minutes)}m:${formatInt(seconds)}s`;
                }
                return `00:${formatInt(seconds)}`;
            }
        },
        emptyFilterFunction: {
            s: "emptyFilterFunction", f: (s) => {
                return s === "<empty>" ? "<empty>" : s;
            }
        },
        Socks5BalancerAsio: { s: "Socks5BalancerAsio" },
        allConnectCountNotice: { s: "now running connect:" },
        nowRuleNotice: { s: "now rule:" },
        buttonChange: { s: "Change It" },
        backend: { s: "backend:" },
        Flush: { s: "Flush" },
        AutoFlush: { s: "AutoFlush" },
        NoUsableServer: { s: "Warning: we don't have Usable Server !!! " },
        NoServer: { s: "No Server ..." },
        NoDataComeFromServer: { s: "No Data Come From Server." },
        DisableConnectTestMode: { s: "Now Running On [DisableConnectTest] Mode." },
        NumberSerial: { s: "No." },
        Host_Port: { s: "Host:Port" },
        name: { s: "name" },
        online: { s: "online" },
        work: { s: "work" },
        runCount: { s: "run" },
        lastTCPCheckTime: { s: "lastTCPCheckTime" },
        lastConnectCheckTime: { s: "lastConnectCheckTime" },
        ManualDisable: { s: "ManualDisable" },
        Usable: { s: "Usable" },
        CloseConnect: { s: "Close Connect" },
        Select: { s: "Select" },
        Check: { s: "Check" },
        data: { s: "data" },
        speedMax: { s: "speedMax" },
        speed: { s: "speed" },
        online_Unknown: { s: "Unknown" },
        online_True: { s: "True" },
        online_False: { s: "False" },
        work_Unknown: { s: "Unknown" },
        work_True: { s: "True" },
        work_False: { s: "False" },
        Disabled: { s: "Disabled" },
        Enabled: { s: "Enabled" },
        EnableIt: { s: "Enable It" },
        Enable: { s: "Enable" },
        Disable: { s: "Disable" },
        Usable_True: { s: "True" },
        Usable_False: { s: "False" },
        // CloseConnect: {s: "Close Connect"},
        UseNow: { s: "Use Now" },
        CheckNow: { s: "Check Now" },
        CleanCheckState: { s: "Clean Check State" },
        ForceCloseAllConnect: { s: "Force Close All Connect" },
        ForceCheckAllNow: { s: "Force Check All Now" },
        lastConnectServer: { s: "lastConnectServer:" },
        lastUseUpstreamIndex: { s: "lastUseUpstreamIndex:" },
        TraditionMode: { s: "Tradition Mode" },
        MixedMode: { s: "Mixed Mode" },
        TcpRelayMode: { s: "Tcp Relay Mode:" },
        nowTime: { s: "now time:" },
        startTime: { s: "startTime:" },
        runTime: { s: "runTime:" },
        lastConnectComeTime: { s: "lastConnectComeTime:" },
        isSleeping: { s: "isSleeping:" },
        sleeping: { s: "sleeping" },
        listenOn: { s: "listen On:" },
        ClientConnectInfo: { s: "Client Connect Info" },
        Host: { s: "Host" },
        lastServer: { s: "lastServer" },
        newServer: { s: "newServer" },
        nowRule: { s: "nowRule" },
        newRule: { s: "newRule" },
        running: { s: "running" },
        closeConnect: { s: "closeConnect" },
        // data: {s: "data"},
        // speedMax: {s: "speedMax"},
        // speed: {s: "speed"},
        // detail: {s: "detail"},
        // CloseConnect: {s: "CloseConnect"},
        // detail: {s: "detail"},
        ListenPortInfo: { s: "Listen Port Info" },
        FastIssueResolve: { s: "Fast Issue Resolve" },
        WebPageOpenVerySlow: { s: "Web page Open very Sloooow :" },
        // ForceCloseAllConnect: {s: "Force Close All Connect"},
        SeemsLikeServerStateNotUpdate: { s: "Seems like Server State not update :" },
        IWantToDisableAllServer: { s: "I Want To Disable All Server :" },
        DisableAllServer: { s: "Disable All Server" },
        IWantToEnableAllServer: { s: "I Want To Enable All Server :" },
        EnableAllServer: { s: "Enable All Server" },
        clickToShowDetail: { s: "click to show detail" },
        timeMs: { s: "ms" },
    };
    return tableState;
})();
//# sourceMappingURL=en-US.js.map