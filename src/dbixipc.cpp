    /*
    This file is part of dbixwall.
    dbixwall is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    dbixwall is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with dbixwall. If not, see <http://www.gnu.org/licenses/>.
*/
/** @file dbixipc.cpp
 * @author Ales Katona <almindor@gmail.com> Etherwall
 * @date 2015
 *
 * Dubaicoin IPC client implementation
 */

#include "dbixipc.h"
#include "helpers.h"
#include <QSettings>
#include <QFileInfo>

// windblows hacks coz windblows sucks
#ifdef Q_OS_WIN32
    #include <windows.h>
#endif

namespace Dbixwall {

// *************************** RequestIPC **************************** //
    int RequestIPC::sCallID = 0;

    RequestIPC::RequestIPC(RequestBurden burden, RequestTypes type, const QString method, const QJsonArray params, int index) :
        fCallID(sCallID++), fType(type), fMethod(method), fParams(params), fIndex(index), fBurden(burden)
    {
    }

    RequestIPC::RequestIPC(RequestTypes type, const QString method, const QJsonArray params, int index) :
        fCallID(sCallID++), fType(type), fMethod(method), fParams(params), fIndex(index), fBurden(Full)
    {
    }

    RequestIPC::RequestIPC(RequestBurden burden) : fBurden(burden)
    {
    }

    RequestTypes RequestIPC::getType() const {
        return fType;
    }

    const QString& RequestIPC::getMethod() const {
        return fMethod;
    }

    const QJsonArray& RequestIPC::getParams() const {
        return fParams;
    }

    int RequestIPC::getIndex() const {
        return fIndex;
    }

    int RequestIPC::getCallID() const {
        return fCallID;
    }

    RequestBurden RequestIPC::burden() const {
        return fBurden;
    }

// *************************** DbixIPC **************************** //

    DbixIPC::DbixIPC(const QString& ipcPath, GdbixLog& gdbixLog) :
        fPath(ipcPath), fBlockFilterID(), fClosingApp(false), fPeerCount(0), fActiveRequest(None),
        fGdbix(), fStarting(0), fGdbixLog(gdbixLog),
        fSyncing(false), fCurrentBlock(0), fHighestBlock(0), fStartingBlock(0),
        fConnectAttempts(0), fKillTime(), fExternal(false), fEventFilterID()
    {
        connect(&fSocket, (void (QLocalSocket::*)(QLocalSocket::LocalSocketError))&QLocalSocket::error, this, &DbixIPC::onSocketError);
        connect(&fSocket, &QLocalSocket::readyRead, this, &DbixIPC::onSocketReadyRead);
        connect(&fSocket, &QLocalSocket::connected, this, &DbixIPC::connectedToServer);
        connect(&fSocket, &QLocalSocket::disconnected, this, &DbixIPC::disconnectedFromServer);
        connect(&fGdbix, &QProcess::started, this, &DbixIPC::connectToServer);

        const QSettings settings;

        //fTimer.setInterval(settings.value("ipc/interval", 10).toInt() * 1000);
        connect(&fTimer, &QTimer::timeout, this, &DbixIPC::onTimer);
    }

    DbixIPC::~DbixIPC() {
        fGdbix.kill();
    }

    void DbixIPC::init() {        
        fConnectAttempts = 0;
        if ( fStarting <= 0 ) { // try to connect without starting gdbix
            DbixLog::logMsg("Dbixwall starting", LS_Info);
            fStarting = 1;
            emit startingChanged(fStarting);
            return connectToServer();
        }

        const QSettings settings;

        const QString progStr = settings.value("gdbix/path", DefaultGdbixPath()).toString();
        const QString argStr = settings.value("gdbix/args", DefaultGdbixArgs).toString();
        const QString ddStr = settings.value("gdbix/datadir", DefaultDataDir).toString();
        QStringList args = (argStr + " --datadir " + ddStr).split(' ', QString::SkipEmptyParts);
        bool testnet = settings.value("gdbix/testnet", false).toBool();
        if ( testnet ) {
            args.append("--testnet");
        }
        /*bool hardfork = settings.value("gdbix/hardfork", true).toBool();
        if ( hardfork ) {
            args.append("--support-dao-fork");
        } else {
            args.append("--oppose-dao-fork");
        }*/

        QFileInfo info(progStr);
        if ( !info.exists() || !info.isExecutable() ) {
            fStarting = -1;
            emit startingChanged(-1);
            setError("Could not find Gdbix. Please check Gdbix path and try again.");
            return bail();
        }

        DbixLog::logMsg("Gdbix starting " + progStr + " " + args.join(" "), LS_Info);
        fStarting = 2;

        fGdbixLog.attach(&fGdbix);
        fGdbix.start(progStr, args);
        emit startingChanged(0);
    }

    void DbixIPC::waitConnect() {
        if ( fStarting == 1 ) {
            return init();
        }

        if ( fStarting != 1 && fStarting != 2 ) {
            return connectionTimeout();
        }

        if ( ++fConnectAttempts < 20 ) {
            if ( fSocket.state() == QLocalSocket::ConnectingState ) {
                fSocket.abort();
            }

            connectToServer();
        } else {
            connectionTimeout();
        }
    }

    void DbixIPC::connectToServer() {
        fActiveRequest = RequestIPC(Full);
        emit busyChanged(getBusy());
        if ( fSocket.state() != QLocalSocket::UnconnectedState ) {
            setError("Already connected");
            return bail();
        }

        fSocket.connectToServer(fPath);
        if ( fConnectAttempts == 0 ) {
            if ( fStarting == 1 ) {
                DbixLog::logMsg("Checking to see if there is an already running gdbix...");
            } else {
                DbixLog::logMsg("Connecting to IPC socket " + fPath);
            }
        }

        QTimer::singleShot(2000, this, SLOT(waitConnect()));
    }

    void DbixIPC::connectedToServer() {
        done();

        getClientVersion();
        getBlockNumber(); // initial
        newBlockFilter();
        getNetVersion();

        if ( fStarting == 1 ) {
            fExternal = true;
            emit externalChanged(true);
            fGdbixLog.append("Attached to external gdbix, see logs in terminal window.");
        }
        fStarting = 3;
        DbixLog::logMsg("Connected to IPC socket");
    }

    void DbixIPC::connectionTimeout() {
        if ( fSocket.state() != QLocalSocket::ConnectedState ) {
            fSocket.abort();
            fStarting = -1;
            setError("Unable to establish IPC connection to Gdbix. Fix path to Gdbix and try again.");
            bail();
        }
    }

    bool DbixIPC::getBusy() const {
        return (fActiveRequest.burden() != None);
    }

    bool DbixIPC::getExternal() const {
        return fExternal;
    }

    bool DbixIPC::getStarting() const {
        return (fStarting == 1 || fStarting == 2);
    }

    bool DbixIPC::getClosing() const {
        return fClosingApp;
    }

    /*bool DbixIPC::getHardForkReady() const {
        const int vn = parseVersionNum();
        return ( vn >= 104010 );
    }*/

    const QString& DbixIPC::getError() const {
        return fError;
    }

    int DbixIPC::getCode() const {
        return fCode;
    }


    /*void DbixIPC::setInterval(int interval) {
        fTimer.setInterval(interval);
    }*/
	
	void DbixIPC::setInterval(int interval) {
        QSettings settings;
        settings.setValue("ipc/interval", (int) (interval / 1000));
        fTimer.setInterval(interval);
    }

    bool DbixIPC::getTestnet() const {
        return fNetVersion == 4;
    }
	
	const QString DbixIPC::getNetworkPostfix() const {
        return Helpers::networkPostfix(fNetVersion);
    }

    /*const QString DbixIPC::getNetworkPostfix() const {
        QSettings settings;
        QString postfix; //= settings.value("gdbix/hardfork", true).toBool() ? "/eth" : "/etc";
        if ( getTestnet() ) {
            postfix += "/morden";
        } else {
            postfix += "/homestead";
        }

        return postfix;
    }*/

    bool DbixIPC::killGdbix() {
        if ( fGdbix.state() == QProcess::NotRunning ) {
            return true;
        }

        if ( fKillTime.elapsed() == 0 ) {
            fKillTime.start();
#ifdef Q_OS_WIN32
            SetConsoleCtrlHandler(NULL, true);
            AttachConsole(fGdbix.processId());
            GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
            FreeConsole();
#else
            fGdbix.terminate();
#endif
        } else if ( fKillTime.elapsed() > 6000 ) {
            qDebug() << "Gdbix did not exit in 6 seconds. Killing...\n";
            fGdbix.kill();
            return true;
        }

        return false;
    }

    bool DbixIPC::closeApp() {
        DbixLog::logMsg("Closing dbixwall");
        fClosingApp = true;
        fTimer.stop();
        emit closingChanged(true);

        if ( fSocket.state() == QLocalSocket::ConnectedState && getBusy() ) { // wait for operation first if we're still connected
            return false;
        }

        if ( fSocket.state() == QLocalSocket::ConnectedState ) {
            bool removed = false;
            if ( !fBlockFilterID.isEmpty() ) { // remove block filter if still connected
                uninstallFilter(fBlockFilterID);
                fBlockFilterID.clear();
                removed = true;
            }

            if ( !fEventFilterID.isEmpty() ) { // remove event filter if still connected
                uninstallFilter(fEventFilterID);
                fEventFilterID.clear();
                removed = true;
            }

            if ( removed ) {
                return false;
            }
        }

        if ( fSocket.state() != QLocalSocket::UnconnectedState ) { // wait for clean disconnect
            fActiveRequest = RequestIPC(Full);
            fSocket.disconnectFromServer();
            return false;
        }

        return killGdbix();
    }

    void DbixIPC::registerEventFilters(const QStringList& addresses, const QStringList& topics) {
        if ( !fEventFilterID.isEmpty() ) {
            uninstallFilter(fEventFilterID);
            fEventFilterID.clear();
        }

        if ( addresses.length() > 0 ) {
            newEventFilter(addresses, topics);
        }
    }

    void DbixIPC::loadLogs(const QStringList& addresses, const QStringList& topics, quint64 fromBlock) {
        if ( addresses.length() > 0 ) {
            getLogs(addresses, topics, fromBlock);
        }
    }

    void DbixIPC::disconnectedFromServer() {
        if ( fClosingApp ) { // expected
            return;
        }

        fError = fSocket.errorString();
        bail();
    }

    void DbixIPC::getAccounts() {
        fAccountList.clear();
        if ( !queueRequest(RequestIPC(GetAccountRefs, "personal_listAccounts", QJsonArray())) ) {
            return bail();
        }
    }

    void DbixIPC::handleGetAccounts() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        QJsonArray refs = jv.toArray();
        foreach( QJsonValue r, refs ) {
            const QString hash = r.toString("INVALID");
            fAccountList.append(AccountInfo(hash, QString(), 0));
        }

        emit getAccountsDone(fAccountList);
        done();
    }

    bool DbixIPC::refreshAccount(const QString& hash, int index) {
        if ( getBalance(hash, index) ) {
            return getTransactionCount(hash, index);
        }

        return false;
    }

    bool DbixIPC::getBalance(const QString& hash, int index) {
        QJsonArray params;
        params.append(hash);
        params.append(QString("latest"));
        if ( !queueRequest(RequestIPC(GetBalance, "eth_getBalance", params, index)) ) {
            bail();
            return false;
        }

        return true;
    }

    bool DbixIPC::getTransactionCount(const QString& hash, int index) {
        QJsonArray params;
        params.append(hash);
        params.append(QString("latest"));
        if ( !queueRequest(RequestIPC(GetTransactionCount, "eth_getTransactionCount", params, index)) ) {
            bail();
            return false;
        }

        return true;
    }


    void DbixIPC::handleAccountBalance() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        const QString decStr = Helpers::toDecStrDbix(jv);
        const int index = fActiveRequest.getIndex();
        fAccountList[index].setBalance(decStr);

        emit accountChanged(fAccountList.at(index));
        done();
    }

    void DbixIPC::handleAccountTransactionCount() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        std::string hexStr = jv.toString("0x0").remove(0, 2).toStdString();
        const BigInt::Vin bv(hexStr, 16);
        quint64 count = bv.toUlong();
        const int index = fActiveRequest.getIndex();
        fAccountList[index].setTransactionCount(count);

        emit accountChanged(fAccountList.at(index));
        done();
    }

    void DbixIPC::newAccount(const QString& password, int index) {
        QJsonArray params;
        params.append(password);
        if ( !queueRequest(RequestIPC(NewAccount, "personal_newAccount", params, index)) ) {
            return bail();
        }
    }

    void DbixIPC::handleNewAccount() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        const QString result = jv.toString();
        emit newAccountDone(result, fActiveRequest.getIndex());
        done();
    }

    void DbixIPC::deleteAccount(const QString& hash, const QString& password, int index) {
        QJsonArray params;
        params.append(hash);
        params.append(password);        
        if ( !queueRequest(RequestIPC(DeleteAccount, "personal_deleteAccount", params, index)) ) {
            return bail();
        }
    }

    void DbixIPC::handleDeleteAccount() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        const bool result = jv.toBool(false);
        emit deleteAccountDone(result, fActiveRequest.getIndex());
        done();
    }

    void DbixIPC::getBlockNumber() {
        if ( !queueRequest(RequestIPC(NonVisual, GetBlockNumber, "eth_blockNumber")) ) {
            return bail();
        }
    }

    void DbixIPC::handleGetBlockNumber() {
        quint64 result;
        if ( !readNumber(result) ) {
             return bail();
        }

        fBlockNumber = result;
        emit getBlockNumberDone(result);
        done();
    }

    quint64 DbixIPC::blockNumber() const {
        return fBlockNumber;
    }

	int DbixIPC::network() const
    {
        return fNetVersion;
    }
	
    void DbixIPC::getPeerCount() {
        if ( !queueRequest(RequestIPC(NonVisual, GetPeerCount, "net_peerCount")) ) {
            return bail();
        }
    }

    void DbixIPC::handleGetPeerCount() {
        if ( !readNumber(fPeerCount) ) {
             return bail();
        }

        emit peerCountChanged(fPeerCount);
        done();
    }

    void DbixIPC::sendTransaction(const QString& from, const QString& to, const QString& valStr, const QString& password,
                                   const QString& gas, const QString& gasPrice, const QString& data) {
        QJsonArray params;
        const QString valHex = Helpers::toHexWeiStr(valStr);
        QJsonObject p;
        p["from"] = from;
        p["value"] = valHex;
        if ( !to.isEmpty() ) {
            p["to"] = to;
        }
        if ( !gas.isEmpty() ) {
            const QString gasHex = Helpers::decStrToHexStr(gas);
            p["gas"] = gasHex;
        }
        if ( !gasPrice.isEmpty() ) {
            const QString gasPriceHex = Helpers::toHexWeiStr(gasPrice);
            p["gasPrice"] = gasPriceHex;
            DbixLog::logMsg(QString("Trans gasPrice: ") + gasPrice + QString(" HexValue: ") + gasPriceHex);
        }
        if ( !data.isEmpty() ) {
            p["data"] = data;
        }

        params.append(p);
        params.append(password);

        if ( !queueRequest(RequestIPC(SendTransaction, "personal_signAndSendTransaction", params)) ) {
            return bail(true); // softbail
        }
    }

    void DbixIPC::handleSendTransaction() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail(true); // softbail
        }

        const QString hash = jv.toString();
        emit sendTransactionDone(hash);
        done();
    }

    int DbixIPC::getConnectionState() const {
        if ( fSocket.state() == QLocalSocket::ConnectedState ) {
            return 1; // TODO: add higher states per peer count!
        }

        return 0;
    }

    void DbixIPC::unlockAccount(const QString& hash, const QString& password, int duration, int index) {
        QJsonArray params;
        params.append(hash);
        params.append(password);

        BigInt::Vin vinVal(duration);
        QString strHex = QString(vinVal.toStr0xHex().data());
        params.append(strHex);

        if ( !queueRequest(RequestIPC(UnlockAccount, "personal_unlockAccount", params, index)) ) {
            return bail();
        }
    }

    void DbixIPC::handleUnlockAccount() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            // special case, we def. need to remove all subrequests, but not stop timer
            fRequestQueue.clear();
            return bail(true);
        }

        const bool result = jv.toBool(false);

        if ( !result ) {
            setError("Unlock account failure");
            emit error();
        }
        emit unlockAccountDone(result, fActiveRequest.getIndex());
        done();
    }

    void DbixIPC::getGasPrice() {
        if ( !queueRequest(RequestIPC(GetGasPrice, "eth_gasPrice")) ) {
            return bail();
        }
    }

    void DbixIPC::handleGetGasPrice() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        const QString decStr = Helpers::toDecStrDbix(jv);

        emit getGasPriceDone(decStr);
        done();
    }

    quint64 DbixIPC::peerCount() const {
        return fPeerCount;
    }
	
	void DbixIPC::ipcReady()
    {
        const QSettings settings;
        setInterval(settings.value("ipc/interval", 10).toInt() * 1000); // re-set here, used for inheritance purposes
        fTimer.start(); // should happen after filter creation, might need to move into last filter response handler
        // if we connected to external gdbix, put that info in gdbix log
        emit startingChanged(fStarting);
        emit connectToServerDone();
        emit connectionStateChanged();
    }

    void DbixIPC::estimateGas(const QString& from, const QString& to, const QString& valStr,
                                   const QString& gas, const QString& gasPrice, const QString& data) {
        const QString valHex = Helpers::toHexWeiStr(valStr);
        QJsonArray params;
        QJsonObject p;
        p["from"] = from;
        p["value"] = valHex;
        if ( !to.isEmpty() ) {
            p["to"] = to;
        }
        if ( !gas.isEmpty() ) {
            const QString gasHex = Helpers::decStrToHexStr(gas);
            p["gas"] = gasHex;
        }
        if ( !gasPrice.isEmpty() ) {
            const QString gasPriceHex = Helpers::toHexWeiStr(gasPrice);
            p["gasPrice"] = gasPriceHex;
        }
        if ( !data.isEmpty() ) {
            p["data"] = Helpers::hexPrefix(data);
        }

        params.append(p);
        if ( !queueRequest(RequestIPC(EstimateGas, "eth_estimateGas", params)) ) {
            return bail();
        }
    }

    void DbixIPC::handleEstimateGas() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail(true); // probably gas too low
        }

        const QString price = Helpers::toDecStr(jv);

        emit estimateGasDone(price);

        done();
    }

    void DbixIPC::newBlockFilter() {
        if ( !fBlockFilterID.isEmpty() ) {
            setError("Filter already set");
            return bail(true);
        }

        if ( !queueRequest(RequestIPC(NewBlockFilter, "eth_newBlockFilter")) ) {
            return bail();
        }
    }

    void DbixIPC::newEventFilter(const QStringList& addresses, const QStringList& topics) {
        QJsonArray params;
        QJsonObject o;
        o["address"] = QJsonArray::fromStringList(addresses);
        if ( topics.length() > 0 && topics.at(0).length() > 0 ) {
            o["topics"] = QJsonArray::fromStringList(topics);
        }
        params.append(o);

        if ( !queueRequest(RequestIPC(NewEventFilter, "eth_newFilter", params)) ) {
            return bail();
        }
    }

    void DbixIPC::handleNewBlockFilter() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }
        fBlockFilterID = jv.toString();

        if ( fBlockFilterID.isEmpty() ) {
            setError("Block filter ID invalid");
            return bail();
        }

        done();
    }

    void DbixIPC::handleNewEventFilter() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }
        fEventFilterID = jv.toString();

        if ( fEventFilterID.isEmpty() ) {
            setError("Event filter ID invalid");
            return bail();
        }

        done();
    }

    void DbixIPC::onTimer() {
        getPeerCount();
        getSyncing();

        if ( !fBlockFilterID.isEmpty() && !fSyncing ) {
            getFilterChanges(fBlockFilterID);
        } else {
            getBlockNumber();
        }

        if ( !fEventFilterID.isEmpty() ) {
            getFilterChanges(fEventFilterID);
        }
    }

    int DbixIPC::parseVersionNum() const {
        QRegExp reg("^Gdbix/v([0-9]+)\\.([0-9]+)\\.([0-9]+).*$");
        reg.indexIn(fClientVersion);
        if ( reg.captureCount() == 3 ) try { // it's gdbix
            return reg.cap(1).toInt() * 100000 + reg.cap(2).toInt() * 1000 + reg.cap(3).toInt();
        } catch ( ... ) {
            return 0;
        }

        return 0;
    }

    void DbixIPC::getSyncing() {
        if ( !queueRequest(RequestIPC(NonVisual, GetSyncing, "eth_syncing")) ) {
            return bail();
        }
    }

    void DbixIPC::getFilterChanges(const QString& filterID) {
        if ( filterID < 0 ) {
            setError("Filter ID invalid");
            return bail();
        }

        QJsonArray params;
        params.append(filterID);

        if ( !queueRequest(RequestIPC(NonVisual, GetFilterChanges, "eth_getFilterChanges", params)) ) {
            return bail();
        }
    }

    void DbixIPC::handleGetFilterChanges() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        QJsonArray ar = jv.toArray();

        foreach( const QJsonValue v, ar ) {
            if ( v.isObject() ) { // event filter result
                const QJsonObject logs = v.toObject();
                emit newEvent(logs, fActiveRequest.getType() == GetFilterChanges); // get logs is not "new"
            } else { // block filter (we don't use transaction filters yet)
                const QString hash = v.toString("bogus");
                getBlockByHash(hash);
            }
        }

        done();
    }

    void DbixIPC::uninstallFilter(const QString& filter) {
        if ( filter.isEmpty() ) {
            setError("Filter not set");
            return bail(true);
        }

        //qDebug() << "uninstalling filter: " << fBlockFilterID << "\n";

        QJsonArray params;
        params.append(filter);

        if ( !queueRequest(RequestIPC(UninstallFilter, "eth_uninstallFilter", params)) ) {
            return bail();
        }
    }

    void DbixIPC::getLogs(const QStringList& addresses, const QStringList& topics, quint64 fromBlock) {
        QJsonArray params;
        QJsonObject o;
        o["fromBlock"] = fromBlock == 0 ? "latest" : Helpers::toHexStr(fromBlock);
        o["address"] = QJsonArray::fromStringList(addresses);
        if ( topics.length() > 0 && topics.at(0).length() > 0 ) {
            o["topics"] = QJsonArray::fromStringList(topics);
        }
        params.append(o);

        // we can use getFilterChanges as result is the same
        if ( !queueRequest(RequestIPC(GetLogs, "eth_getLogs", params)) ) {
            return bail();
        }
    }

    void DbixIPC::handleUninstallFilter() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        done();
    }

    void DbixIPC::getClientVersion() {
        if ( !queueRequest(RequestIPC(NonVisual, GetClientVersion, "web3_clientVersion")) ) {
            return bail();
        }
    }

    void DbixIPC::getNetVersion() {
        if ( !queueRequest(RequestIPC(NonVisual, GetNetVersion, "net_version")) ) {
            return bail();
        }
    }

    bool DbixIPC::getSyncingVal() const {
        return fSyncing;
    }

    quint64 DbixIPC::getCurrentBlock() const {
        return fCurrentBlock;
    }

    quint64 DbixIPC::getHighestBlock() const {
        return fHighestBlock;
    }

    quint64 DbixIPC::getStartingBlock() const {
        return fStartingBlock;
    }

    void DbixIPC::getTransactionByHash(const QString& hash) {
        QJsonArray params;
        params.append(hash);

        if ( !queueRequest(RequestIPC(GetTransactionByHash, "eth_getTransactionByHash", params)) ) {
            return bail();
        }
    }

    void DbixIPC::handleGetTransactionByHash() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        emit newTransaction(TransactionInfo(jv.toObject()));
        done();
    }

    void DbixIPC::getBlockByHash(const QString& hash) {
        QJsonArray params;
        params.append(hash);
        params.append(true); // get transaction bodies

        if ( !queueRequest(RequestIPC(GetBlock, "eth_getBlockByHash", params)) ) {
            return bail();
        }
    }

    void DbixIPC::getBlockByNumber(quint64 blockNum) {
        QJsonArray params;
        params.append(Helpers::toHexStr(blockNum));
        params.append(true); // get transaction bodies

        if ( !queueRequest(RequestIPC(GetBlock, "eth_getBlockByNumber", params)) ) {
            return bail();
        }
    }

    void DbixIPC::handleGetBlock() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        const QJsonObject block = jv.toObject();
        const quint64 num = Helpers::toQUInt64(block.value("number"));
        emit getBlockNumberDone(num);
        emit newBlock(block);
        done();
    }

    void DbixIPC::getTransactionReceipt(const QString& hash) {
        QJsonArray params;
        params.append(hash);

        if ( !queueRequest(RequestIPC(GetTransactionReceipt, "eth_getTransactionReceipt", params)) ) {
            return bail();
        }
    }

    void DbixIPC::handleGetTransactionReceipt() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }


        emit getTransactionReceiptDone(jv.toObject());
        done();
    }

    void DbixIPC::handleGetClientVersion() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        fClientVersion = jv.toString();

        const int vn = parseVersionNum();
        if ( vn > 0 && vn < 104092 ) {
            setError("Gdbix version 1.4.92 and older are not ready for the upcoming 2nd hard fork. Please update Gdbix to ensure you are ready.");
            emit error();
        }

        if ( vn > 0 && vn < 105001 ) {
            setError("Gdbix version older than 1.5.1 is no longer supported. Please upgrade gdbix to 1.5.1+.");
            emit error();
        }

        emit clientVersionChanged(fClientVersion);
        done();
    }

    void DbixIPC::handleGetNetVersion() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        bool ok = false;
        fNetVersion = jv.toString().toInt(&ok);

        if ( !ok ) {
            setError("Unable to parse net version string: " + jv.toString());
            return bail(true);
        }

        emit netVersionChanged(fNetVersion);
        done();

        /*fTimer.start(); // should happen after filter creation, might need to move into last filter response handler
        // if we connected to external gdbix, put that info in gdbix log

        emit startingChanged(fStarting);
        emit connectToServerDone();
        emit connectionStateChanged();
        emit hardForkReadyChanged(getHardForkReady());*/
		
		ipcReady();
    }

    void DbixIPC::handleGetSyncing() {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return bail();
        }

        if ( jv.isNull() || ( jv.isBool() && !jv.toBool(false) ) ) {
            if ( fSyncing ) {
                fSyncing = false;
                if ( fBlockFilterID.isEmpty() ) {
                    newBlockFilter();
                }
                emit syncingChanged(fSyncing);
            }

            return done();
        }

        const QJsonObject syncing = jv.toObject();
        fCurrentBlock = Helpers::toQUInt64(syncing.value("currentBlock"));
        fHighestBlock = Helpers::toQUInt64(syncing.value("highestBlock"));
        fStartingBlock = Helpers::toQUInt64(syncing.value("startingBlock"));
        if ( !fSyncing ) {
            if ( !fBlockFilterID.isEmpty() ) {
                uninstallFilter(fBlockFilterID);
                fBlockFilterID.clear();
            }
            fSyncing = true;
        }

        emit syncingChanged(fSyncing);
        done();
    }

    void DbixIPC::bail(bool soft) {
        qDebug() << "BAIL[" << soft << "]: " << fError << "\n";

        if ( !soft ) {
            fTimer.stop();
            fRequestQueue.clear();
        }

        fActiveRequest = RequestIPC(None);
        errorOut();
    }

    void DbixIPC::setError(const QString& error) {
        fError = error;
        DbixLog::logMsg(error, LS_Error);
    }

    void DbixIPC::errorOut() {
        emit error();
        emit connectionStateChanged();
        done();
    }

    void DbixIPC::done() {
        fActiveRequest = RequestIPC(None);
        if ( !fRequestQueue.isEmpty() ) {
            const RequestIPC request = fRequestQueue.first();
            fRequestQueue.removeFirst();
            writeRequest(request);
        } else {
            emit busyChanged(getBusy());
        }
    }

    QJsonObject DbixIPC::methodToJSON(const RequestIPC& request) {
        QJsonObject result;

        result.insert("jsonrpc", QJsonValue(QString("2.0")));
        result.insert("method", QJsonValue(request.getMethod()));
        result.insert("id", QJsonValue(request.getCallID()));
        result.insert("params", QJsonValue(request.getParams()));

        return result;
    }

    bool DbixIPC::queueRequest(const RequestIPC& request) {
        if ( fActiveRequest.burden() == None ) {
            return writeRequest(request);
        } else {
            fRequestQueue.append(request);
            return true;
        }
    }

    bool DbixIPC::writeRequest(const RequestIPC& request) {
        fActiveRequest = request;
        if ( fActiveRequest.burden() == Full ) {
            emit busyChanged(getBusy());
        }

        QJsonDocument doc(methodToJSON(fActiveRequest));
        const QString msg(doc.toJson());

        if ( !fSocket.isWritable() ) {
            setError("Socket not writeable");
            fCode = 0;
            return false;
        }

        const QByteArray sendBuf = msg.toUtf8();
        DbixLog::logMsg("Sent: " + msg, LS_Debug);
        const int sent = fSocket.write(sendBuf);

        if ( sent <= 0 ) {
            setError("Error on socket write: " + fSocket.errorString());
            fCode = 0;
            return false;
        }

        return true;
    }

    bool DbixIPC::readData() {
        fReadBuffer += QString(fSocket.readAll()).trimmed();

        if ( fReadBuffer.at(0) == '{' && fReadBuffer.at(fReadBuffer.length() - 1) == '}' && fReadBuffer.count('{') == fReadBuffer.count('}') ) {
            DbixLog::logMsg("Received: " + fReadBuffer, LS_Debug);
            return true;
        }

        return false;
    }

    bool DbixIPC::readReply(QJsonValue& result) {
        const QString data = fReadBuffer;
        fReadBuffer.clear();

        if ( data.isEmpty() ) {
            setError("Error on socket read: " + fSocket.errorString());
            fCode = 0;
            return false;
        }

        QJsonParseError parseError;
        QJsonDocument resDoc = QJsonDocument::fromJson(data.toUtf8(), &parseError);

        if ( parseError.error != QJsonParseError::NoError ) {
            qDebug() << data << "\n";
            setError("Response parse error: " + parseError.errorString());
            fCode = 0;
            return false;
        }

        const QJsonObject obj = resDoc.object();
        const int objID = obj["id"].toInt(-1);

        if ( objID != fActiveRequest.getCallID() ) { // TODO
            setError("Call number mismatch " + QString::number(objID) + " != " + QString::number(fActiveRequest.getCallID()));
            fCode = 0;
            return false;
        }

        result = obj["result"];

        // get filter changes bugged, returns null on result array, see https://github.com/ethereum/go-ethereum/issues/2746
        if ( result.isNull() && fActiveRequest.getType() == GetFilterChanges ) {
            result = QJsonValue(QJsonArray());
        }

        if ( result.isUndefined() || result.isNull() ) {
            if ( obj.contains("error") ) {
                if ( obj["error"].toObject().contains("message") ) {
                    fError = obj["error"].toObject()["message"].toString();
                }

                if ( obj["error"].toObject().contains("code") ) {
                    fCode = obj["error"].toObject()["code"].toInt();
                }

                return false;
            }

            if ( fActiveRequest.getType() != GetTransactionByHash ) { // this can happen if out of sync, it's not fatal for transaction get
                setError("Result object undefined in IPC response for request: " + fActiveRequest.getMethod());
                qDebug() << data << "\n";
                return false;
            }
        }

        return true;
    }

    bool DbixIPC::readVin(BigInt::Vin& result) {
        QJsonValue jv;
        if ( !readReply(jv) ) {
            return false;
        }

        std::string hexStr = jv.toString("0x0").remove(0, 2).toStdString();
        result = BigInt::Vin(hexStr, 16);

        return true;
    }

    bool DbixIPC::readNumber(quint64& result) {
        BigInt::Vin r;
        if ( !readVin(r) ) {
            return false;
        }

        result = r.toUlong();
        return true;
    }

    void DbixIPC::onSocketError(QLocalSocket::LocalSocketError err) {
        fError = fSocket.errorString();
        fCode = err;
    }

    void DbixIPC::onSocketReadyRead() {
        if ( !getBusy() ) {
            return; // probably error-ed out
        }

        if ( !readData() ) {
            return; // not finished yet
        }

        switch ( fActiveRequest.getType() ) {
        case NewAccount: {
                handleNewAccount();
                break;
            }
        case DeleteAccount: {
                handleDeleteAccount();
                break;
            }
        case GetBlockNumber: {
                handleGetBlockNumber();
                break;
            }
        case GetAccountRefs: {
                handleGetAccounts();
                break;
            }
        case GetBalance: {
                handleAccountBalance();
                break;
            }
        case GetTransactionCount: {
                handleAccountTransactionCount();
                break;
            } 
        case GetPeerCount: {
                handleGetPeerCount();
                break;
            }
        case SendTransaction: {
                handleSendTransaction();
                break;
            }
        case UnlockAccount: {
                handleUnlockAccount();
                break;
            }
        case GetGasPrice: {
                handleGetGasPrice();
                break;
            }
        case EstimateGas: {
                handleEstimateGas();
                break;
            }
        case NewBlockFilter: {
                handleNewBlockFilter();
                break;
            }
        case NewEventFilter: {
                handleNewEventFilter();
                break;
            }
        case GetFilterChanges: {
                handleGetFilterChanges();
                break;
            }
        case UninstallFilter: {
                handleUninstallFilter();
                break;
            }
        case GetTransactionByHash: {
                handleGetTransactionByHash();
                break;
            }
        case GetBlock: {
                handleGetBlock();
                break;
            }
        case GetClientVersion: {
                handleGetClientVersion();
                break;
            }
        case GetNetVersion: {
                handleGetNetVersion();
                break;
            }
        case GetSyncing: {
                handleGetSyncing();
                break;
            }
        case GetLogs: {
                handleGetFilterChanges();
                break;
            }
        case GetTransactionReceipt: {
                handleGetTransactionReceipt();
                break;
            }
        default: qDebug() << "Unknown reply: " << fActiveRequest.getType() << "\n"; break;
        }
    }

}