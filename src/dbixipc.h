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
/** @file dbixipc.h
 * @author Ales Katona <almindor@gmail.com> Etherwall
 * @date 2015
 *
 * Dubaicoin IPC client header
 */

#ifndef ETHERIPC_H
#define ETHERIPC_H

#include <QObject>
#include <QList>
#include <QLocalSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonDocument>
#include <QTimer>
#include <QThread>
#include <QProcess>
#include <QTime>
#include "types.h"
#include "dbixlog.h"
#include "gdbixlog.h"
#include "bigint.h"

namespace Dbixwall {

    enum RequestBurden {
        Full,
        NonVisual,
        None
    };

    class RequestIPC {
    public:
        RequestIPC(RequestBurden burden, RequestTypes type, const QString method, const QJsonArray params = QJsonArray(), int index = -1);
        RequestIPC(RequestTypes type, const QString method, const QJsonArray params = QJsonArray(), int index = -1);
        RequestIPC(RequestBurden burden);

        RequestTypes getType() const;
        const QString& getMethod() const;
        const QJsonArray& getParams() const;
        int getIndex() const;
        int getCallID() const;
        RequestBurden burden() const;
        static int sCallID;
    private:
        int fCallID;
        RequestTypes fType;
        QString fMethod;
        QJsonArray fParams;
        int fIndex;
        RequestBurden fBurden;
    };

    typedef QList<RequestIPC> RequestList;

    class DbixIPC: public QObject
    {
        Q_OBJECT
        Q_PROPERTY(QString error READ getError NOTIFY error)
        Q_PROPERTY(int code READ getCode NOTIFY error)
        Q_PROPERTY(bool external READ getExternal NOTIFY externalChanged)
        Q_PROPERTY(bool busy READ getBusy NOTIFY busyChanged)
        Q_PROPERTY(bool starting READ getStarting NOTIFY startingChanged)
        Q_PROPERTY(bool syncing READ getSyncingVal NOTIFY syncingChanged)
        Q_PROPERTY(bool closing READ getClosing NOTIFY closingChanged)
        //Q_PROPERTY(bool hardForkReady READ getHardForkReady NOTIFY hardForkReadyChanged)
        Q_PROPERTY(int connectionState READ getConnectionState NOTIFY connectionStateChanged)
        Q_PROPERTY(quint64 peerCount READ peerCount NOTIFY peerCountChanged)
        Q_PROPERTY(QString clientVersion MEMBER fClientVersion NOTIFY clientVersionChanged)
        Q_PROPERTY(int netVersion MEMBER fNetVersion NOTIFY netVersionChanged)
        Q_PROPERTY(bool testnet READ getTestnet NOTIFY netVersionChanged)
        Q_PROPERTY(quint64 currentBlock READ getCurrentBlock NOTIFY syncingChanged)
        Q_PROPERTY(quint64 highestBlock READ getHighestBlock NOTIFY syncingChanged)
        Q_PROPERTY(quint64 startingBlock READ getStartingBlock NOTIFY syncingChanged)
        Q_PROPERTY(quint64 blockNumber MEMBER fBlockNumber NOTIFY getBlockNumberDone)
    public:
        DbixIPC(const QString& ipcPath, GdbixLog& gdbixLog);
        virtual ~DbixIPC();
        void setWorker(QThread* worker);
        bool getBusy() const;
        bool getExternal() const;
        bool getStarting() const;
        bool getClosing() const;
        //bool getHardForkReady() const;
        const QString& getError() const;
        int getCode() const;
        bool getTestnet() const;
        const QString getNetworkPostfix() const;
        quint64 blockNumber() const;
		int network() const;
    public slots:
        void init();
        void waitConnect();
        void connectToServer();
        void connectedToServer();
        void connectionTimeout();
        void disconnectedFromServer();
        void getAccounts();
        bool refreshAccount(const QString& hash, int index);
        bool getBalance(const QString& hash, int index);
        bool getTransactionCount(const QString& hash, int index);
        void newAccount(const QString& password, int index);
        void deleteAccount(const QString& hash, const QString& password, int index);
        void getBlockNumber();
        void getPeerCount();
        void sendTransaction(const QString& from, const QString& to, const QString& valStr, const QString& password,
                             const QString& gas = QString(), const QString& gasPrice = QString(), const QString& data = QString());
        void unlockAccount(const QString& hash, const QString& password, int duration, int index);
        void getGasPrice();
        Q_INVOKABLE void estimateGas(const QString& from, const QString& to, const QString& valStr,
                                     const QString& gas, const QString& gasPrice, const QString& data);
        void getTransactionByHash(const QString& hash);
        void getBlockByHash(const QString& hash);
        void getBlockByNumber(quint64 blockNum);
        Q_INVOKABLE void getTransactionReceipt(const QString& hash);
        void onSocketReadyRead();
        void onSocketError(QLocalSocket::LocalSocketError err);
        Q_INVOKABLE void setInterval(int interval);
        bool closeApp();
        void registerEventFilters(const QStringList& addresses, const QStringList& topics);
        void loadLogs(const QStringList& addresses, const QStringList& topics, quint64 fromBlock);
    signals:
        void connectToServerDone();
        void getAccountsDone(const AccountList& list) const;
        void newAccountDone(const QString& result, int index) const;
        void deleteAccountDone(bool result, int index) const;
        void getBlockNumberDone(quint64 num) const;
        void sendTransactionDone(const QString& hash) const;
        void unlockAccountDone(bool result, int index) const;
        void getGasPriceDone(const QString& price) const;
        void estimateGasDone(const QString& price) const;
        void newTransaction(const TransactionInfo& info) const;
        void newBlock(const QJsonObject& block) const;
        void newEvent(const QJsonObject& event, bool isNew) const;
        void getTransactionReceiptDone(const QJsonObject& receipt) const;

        void peerCountChanged(quint64 num) const;
        void accountChanged(const AccountInfo& info) const;
        void busyChanged(bool busy) const;
        void externalChanged(bool external) const;
        void startingChanged(bool starting) const;
        void syncingChanged(bool syncing) const;
        void closingChanged(bool closing) const;
        //void hardForkReadyChanged(bool hfReady) const;
        void connectionStateChanged() const;
        void clientVersionChanged(const QString& ver) const;
        void netVersionChanged(int ver) const;
        void error() const;
    private:
        QString fPath;
        QLocalSocket fSocket;
        QString fBlockFilterID;
        bool fClosingApp;
        quint64 fPeerCount;
        QString fReadBuffer;
        QString fError;
        int fCode;
        AccountList fAccountList;
        TransactionList fTransactionList;
        RequestList fRequestQueue;
        RequestIPC fActiveRequest;
        QTimer fTimer;
        int fNetVersion;
        QString fClientVersion;
        QProcess fGdbix;
        int fStarting;
        GdbixLog& fGdbixLog;
        bool fSyncing;
        quint64 fCurrentBlock;
        quint64 fHighestBlock;
        quint64 fStartingBlock;
        int fConnectAttempts;
        QTime fKillTime;
        bool fExternal;
        QString fEventFilterID;
        quint64 fBlockNumber;

        void handleNewAccount();
        void handleDeleteAccount();
        void handleGetAccounts();
        void handleAccountBalance();
        void handleAccountTransactionCount();
        void handleGetBlockNumber();
        void handleGetPeerCount();
        void handleSendTransaction();
        void handleUnlockAccount();
        void handleGetGasPrice();
        void handleEstimateGas();
        void handleNewBlockFilter();
        void handleNewEventFilter();
        void handleGetFilterChanges();
        void handleUninstallFilter();
        void handleGetTransactionByHash();
        void handleGetBlock();
        void handleGetTransactionReceipt();
        void handleGetClientVersion();
        void handleGetNetVersion();
        void handleGetSyncing();

		void ipcReady();
        void onTimer();
        bool killGdbix();
        int parseVersionNum() const;
        void getSyncing();
        void getFilterChanges(const QString& filterID);
        void getClientVersion();
        void getNetVersion();
        bool getSyncingVal() const;
        quint64 getCurrentBlock() const;
        quint64 getHighestBlock() const;
        quint64 getStartingBlock() const;
        int getConnectionState() const;
        quint64 peerCount() const;
        void bail(bool soft = false);
        void setError(const QString& error);
        void errorOut();
        void done();
        void newBlockFilter();
        void newEventFilter(const QStringList& addresses, const QStringList& topics);
        void uninstallFilter(const QString& filter);
        void getLogs(const QStringList& addresses, const QStringList& topics, quint64 fromBlock);

        QJsonObject methodToJSON(const RequestIPC& request);
        bool queueRequest(const RequestIPC& request);
        bool writeRequest(const RequestIPC& request);
        bool readData();
        bool readReply(QJsonValue& result);
        bool readVin(BigInt::Vin& result);
        bool readNumber(quint64& result);
        const QString toDecStr(const QJsonValue& jv) const;
    };

}

#endif // ETHERIPC_H

