// https://github.com/moment/moment/issues/463#issuecomment-552498641
function formatInt(int) {
    if (int < 10) {
        return `0${int}`;
    }
    return `${int}`;
}

function formatDuration(time) {
    const seconds = moment.duration(time).seconds();
    const minutes = moment.duration(time).minutes();
    const hours = moment.duration(time).hours();
    const days = moment.duration(time).days();
    const months = moment.duration(time).months();
    const years = moment.duration(time).years();
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

function formatNumber2FixedLength(n) {
    return n.toFixed(3);
}

function speed2String(s) {
    if (s < 1024) {
        return '' + s + 'Byte/s';
    } else if (s < Math.pow(1024, 2)) {
        return '' + formatNumber2FixedLength(s / Math.pow(1024, 1)) + 'KB/s';
    } else if (s < Math.pow(1024, 3)) {
        return '' + formatNumber2FixedLength(s / Math.pow(1024, 2)) + 'MB/s';
    } else if (s < Math.pow(1024, 4)) {
        return '' + formatNumber2FixedLength(s / Math.pow(1024, 3)) + 'GB/s';
    } else if (s < Math.pow(1024, 5)) {
        return '' + formatNumber2FixedLength(s / Math.pow(1024, 4)) + 'TB/s';
    } else if (s < Math.pow(1024, 6)) {
        return '' + formatNumber2FixedLength(s / Math.pow(1024, 5)) + 'EB/s';
    }
}

function dataCount2String(d) {
    if (d < 1024) {
        return '' + d + 'Byte';
    } else if (d < Math.pow(1024, 2)) {
        return '' + formatNumber2FixedLength(d / Math.pow(1024, 1)) + 'KB';
    } else if (d < Math.pow(1024, 3)) {
        return '' + formatNumber2FixedLength(d / Math.pow(1024, 2)) + 'MB';
    } else if (d < Math.pow(1024, 4)) {
        return '' + formatNumber2FixedLength(d / Math.pow(1024, 3)) + 'GB';
    } else if (d < Math.pow(1024, 5)) {
        return '' + formatNumber2FixedLength(d / Math.pow(1024, 4)) + 'TB';
    } else if (d < Math.pow(1024, 6)) {
        return '' + formatNumber2FixedLength(d / Math.pow(1024, 5)) + 'EB';
    }
}

function formatSpeedMax(u) {
    if (u.byteInfo === 'true') {
        return '↑' + speed2String(_.parseInt(u.byteUpChangeMax)) + ' ↓' + speed2String(_.parseInt(u.byteDownChangeMax));
    } else {
        return '↑' + speed2String(0) + ' ↓' + speed2String(0);
    }
}

function formatSpeed(u) {
    if (u.byteInfo === 'true') {
        return '↑' + speed2String(_.parseInt(u.byteUpChange)) + ' ↓' + speed2String(_.parseInt(u.byteDownChange));
    } else {
        return '↑' + speed2String(0) + ' ↓' + speed2String(0);
    }
}

function formatData(u) {
    if (u.byteInfo === 'true') {
        return '↑' + dataCount2String(_.parseInt(u.byteUpLast)) + ' ↓' + dataCount2String(_.parseInt(u.byteDownLast));
    } else {
        return '↑' + dataCount2String(0) + ' ↓' + dataCount2String(0);
    }
}

function reduceField(T, F) {
    return _.reduce(T, function (acc, n) {
        return acc + _.parseInt(_.get(n, F, "0"));
    }, 0);
}

function getSearchParams(key) {
    var q = (new URL(document.location)).searchParams;
    return q.get(key);
}

function setSearchParams(key, value) {
    var newQ = (new URL(document.location)).searchParams;
    newQ.set(key, value);
    window.history.pushState(null, null, '?' + newQ.toString());
}

function tryGetBackendConfigFromServer() {
    fetch('backend', {
        credentials: 'omit'
    }).then(function (T) {
        if (T.ok) {
            return T.json();
        }
        return Promise.reject(T);
    }).then(function (T) {
        var s = getSearchParams('backend');
        console.log('getSearchParams(\'backend\'):', s);
        if (s) {
            setSearchParams('backend', s);
            return;
        } else {
            console.log('tryGetBackendConfigFromServer T:', T);
            var host = _.get(T, 'host', defaultBackendHost);
            var port = _.get(T, 'port', defaultBackendPort);
            if (!(_.isString(host) && host.length > 0)) {
                host = document.location.hostname;
            }
            if (_.isString(port)) {
                port = _.parseInt(port);
            }
            if (!(port > 0 && port < 65536)) {
                port = defaultBackendPort;
            }
            console.log('tryGetBackendConfigFromServer [host, port]:', [host, port]);
            setSearchParams('backend', host + ':' + port);
            return;
        }
    }).catch(function (e) {
        console.warn(e);
        var s = getSearchParams('backend');
        if (s) {
            setSearchParams('backend', s);
        } else {
            setSearchParams('backend', defaultBackendHost + ':' + defaultBackendPort);
        }
    }).then(function () {
        app.flush();
    })
}

function createChart(target2DContext) {
    return new Chart(target2DContext, {
        type: 'line',
    })
}


var defaultBackendHost = "127.0.0.1";
var defaultBackendPort = 5010;

moment.locale('zh-cn');

var getI18nTable = () => {
    if (window.navigator.language === "zh-CN") {
        // chinese
        window.i18nTable = window.i18n.zhCN;
    } else {
        // english
        window.i18nTable = window.i18n.enUS;
    }
};
getI18nTable();

var app = new Vue({
    el: '#body',
    data: {
        ServerJsonInfo_Show: false,
        test: 11244,
        ServerJsonInfo: {},
        upstreamPool: [],
        //monitorCenter: refMonitorCenter(),
        //formatTime: (m: moment.Moment | undefined) => {
        //  return m ? m.format('ll HH:mm:ss') : 'undefined';
        //},
        rule: "",
        nowTime: "",
        startTime: "",
        runTime: 0,
        relayId: 0,
        relayIdMod: 0,
        runTimeString: "",
        runTimeString2: "",
        lastConnectComeTime: "",
        lastConnectComeTimeAgo: 0,
        lastConnectComeTimeAgoString: "",
        lastConnectComeTimeAgoString2: "",
        listenOn: "",
        haveUsableServer: true,
        UpstreamSelectRuleList: [],
        lastUseUpstreamIndex: 0,
        lastConnectServerIndex: 0,
        lastConnectServer: {},
        //isWork: checkServer,
        //speedArray: speedArray,
        //dataArray: dataArray,
        newRule: "",
        allConnectCount: 0,
        totalUp: 0,
        disableConnectTest: false,
        traditionTcpRelay: false,
        totalDown: 0,
        totalUpSpeed: 0,
        totalDownSpeed: 0,
        totalUpSpeedMax: 0,
        totalDownSpeedMax: 0,
        sleepTime: 0,
        UpstreamIndex: [],
        ClientIndex: [],
        ListenIndex: [],
        get backend() {
            var s = getSearchParams('backend');
            if (s) {
                return s;
            } else {
                // setSearchParams('backend', defaultBackendHost + ':' + defaultBackendPort);
                return defaultBackendHost + ':' + defaultBackendPort;
            }
        },
        set backend(s) {
            setSearchParams('backend', s);
        },
        InternetState: {
            isOk: false,
            error: undefined,
        },
        autoFlushState: false,
        autoFlushHandle: -1,
        tableState: window.i18nTable,
    },
    computed: {},
    methods: {
        formatSpeedMax: formatSpeedMax,
        formatSpeed: formatSpeed,
        formatData: formatData,
        speed2String: speed2String,
        dataCount2String: dataCount2String,
        reduceField: reduceField,
        autoFlush: function () {
            app.autoFlushState = !app.autoFlushState;
            console.log('autoFlushHandle app.autoFlushState', app.autoFlushState);
            if (app.autoFlushState) {
                console.log('autoFlushHandle setInterval');
                app.autoFlushHandle = setInterval(function () {
                    console.log('autoFlushHandle flush');
                    app.flush()
                        .then(() => {
                            app.$forceUpdate();
                        });
                }, 1000);
            } else {
                console.log('autoFlushHandle clearTimeout');
                clearInterval(app.autoFlushHandle)
            }
        },
        flush: function () {
            return fetch('http://' + app.backend + '/', {
                credentials: 'omit'
            }).then(function (T) {
                if (T.ok) {
                    return T.json();
                }
                return Promise.reject(T);
            }).then(function (T) {
                console.log(T);
                if (
                    _.has(T, 'pool') &&
                    _.has(T, 'config') &&
                    _.isObject(T.pool) &&
                    _.isArray(T.pool.upstream) &&
                    _.isObject(T.config)
                ) {
                    app.ServerJsonInfo = T;
                    app.lastUseUpstreamIndex = T.pool.getLastUseUpstreamIndex;
                    if (_.isArray(T.pool.upstream)) {
                        app.upstreamPool = T.pool.upstream;
                    } else {
                        app.upstreamPool = [];
                    }
                    app.upstreamPool = app.upstreamPool.map(N => {
                        N.index = _.parseInt(N.index);
                        return N;
                    })
                    app.UpstreamSelectRuleList = T.RuleEnumList;
                    app.rule = T.nowRule;
                    app.newRule = T.nowRule;
                    app.totalUp = _.reduce(app.upstreamPool, function (acc, n) {
                        return acc + _.parseInt(n.byteUpLast);
                    }, 0);
                    app.totalUp = reduceField(app.upstreamPool, 'byteUpLast');
                    app.totalDown = reduceField(app.upstreamPool, 'byteDownLast');
                    app.totalUpSpeed = reduceField(app.upstreamPool, 'byteUpChange');
                    app.totalDownSpeed = reduceField(app.upstreamPool, 'byteDownChange');
                    app.totalUpSpeedMax = reduceField(app.upstreamPool, 'byteUpChangeMax');
                    app.totalDownSpeedMax = reduceField(app.upstreamPool, 'byteDownChangeMax');

                    app.sleepTime = _.parseInt(_.get(T, 'config.sleepTime', '' + Number.MAX_SAFE_INTEGER));
                    app.nowTime = T.nowTime;

                    app.relayIdMod = T.config.relayIdMod;
                    app.relayId = T.config.relayId;

                    app.runTime = T.runTime;
                    app.runTimeString = moment.duration(_.parseInt(T.runTime)).humanize();
                    if (window.i18nTable && window.i18nTable.formatDurationFunction) {
                        app.runTimeString2 = window.i18nTable.formatDurationFunction.f(_.parseInt(T.runTime));
                    } else {
                        app.runTimeString2 = formatDuration(_.parseInt(T.runTime));
                    }

                    app.startTime = T.startTime;

                    if (_.isArray(T.UpstreamIndex)) {
                        app.UpstreamIndex = T.UpstreamIndex.map(N => {
                            N.byteInfo = 'true';
                            N.index = _.parseInt(N.index);
                            return N;
                        });
                    } else {
                        app.UpstreamIndex = [];
                    }
                    if (_.isArray(T.ClientIndex)) {
                        app.ClientIndex = T.ClientIndex.map(N => {
                            N.byteInfo = 'true';
                            N.lastUseUpstreamIndex = _.parseInt(N.lastUseUpstreamIndex);
                            return N;
                        });
                    } else {
                        app.ClientIndex = [];
                    }
                    if (_.isArray(T.ListenIndex)) {
                        app.ListenIndex = T.ListenIndex.map(N => {
                            N.byteInfo = 'true';
                            N.lastUseUpstreamIndex = _.parseInt(N.lastUseUpstreamIndex);
                            return N;
                        });
                    } else {
                        app.ListenIndex = [];
                    }

                    app.lastConnectComeTime = T.pool.lastConnectComeTime;
                    app.lastConnectComeTimeAgo = _.parseInt(T.pool.lastConnectComeTimeAgo);
                    app.lastConnectComeTimeAgoString = moment.duration(app.lastConnectComeTimeAgo).humanize();
                    if (window.i18nTable && window.i18nTable.formatDurationFunction) {
                        app.lastConnectComeTimeAgoString2 = window.i18nTable.formatDurationFunction.f(app.lastConnectComeTimeAgo);
                    } else {
                        app.lastConnectComeTimeAgoString2 = formatDuration(app.lastConnectComeTimeAgo);
                    }

                    app.lastConnectServerIndex = _.parseInt(T.lastConnectServerIndex);
                    app.lastConnectServer = app.upstreamPool.find(function (n) {
                        return _.parseInt(n.index) === app.lastConnectServerIndex;
                    });
                    app.listenOn = ' ' + T.config.listenHost + ' : ' + T.config.listenPort;

                    app.allConnectCount = _.reduce(T.pool.upstream, function (acc, T) {
                        return acc + _.parseInt(T.connectCount);
                    }, 0);

                    if (T.config.disableConnectTest === 'true') {
                        app.disableConnectTest = true;
                    }
                    if (T.config.traditionTcpRelay === 'true') {
                        app.traditionTcpRelay = true;
                    }

                    app.InternetState.isOk = true;
                    app.InternetState.error = undefined;
                }
            }).then(function () {
                console.log(app);
            }).catch(function (e) {
                console.error(e);
                app.InternetState.isOk = false;
                app.InternetState.error = e;
            });
        },
        sendCommand: function (cmd) {
            fetch('http://' + app.backend + cmd, {
                credentials: 'omit'
            }).then(function (T) {
                if (T.ok) {
                    return T;
                }
                return Promise.reject(T);
            }).catch(function (e) {
                console.error(e);
            }).then(function () {
                app.flush();
            });
        },
    }
});
tryGetBackendConfigFromServer();