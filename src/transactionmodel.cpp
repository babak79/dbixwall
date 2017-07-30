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
/** @file transactionmodel.cpp
 * @author Ales Katona <almindor@gmail.com>
 * @date 2015
 *
 * Transaction model implementation
 */

#include "transactionmodel.h"
#include "helpers.h"
#include "dubaicoin/tx.h"
#include <QDebug>
#include <QTimer>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonDocument>
#include <QCoreApplication>
#include <QSettings>

namespace Dbixwall {

    TransactionModel::TransactionModel(DbixIPC& ipc, const AccountModel& accountModel) :
        QAbstractListModel(0), fIpc(ipc), fAccountModel(accountModel), fBlockNumber(0), fLastBlock(0), fFirstBlock(0), fGasPrice("unknown"), fGasEstimate("unknown"), fNetManager(this),
        fLatestVersion(QCoreApplication::applicationVersion())
    {
        connect(&ipc, &DbixIPC::connectToServerDone, this, &TransactionModel::connectToServerDone);
        connect(&ipc, &DbixIPC::getAccountsDone, this, &TransactionModel::getAccountsDone);
        connect(&ipc, &DbixIPC::getBlockNumberDone, this, &TransactionModel::getBlockNumberDone);
        connect(&ipc, &DbixIPC::getGasPriceDone, this, &TransactionModel::getGasPriceDone);
        connect(&ipc, &DbixIPC::estimateGasDone, this, &TransactionModel::estimateGasDone);
        connect(&ipc, &DbixIPC::sendTransactionDone, this, &TransactionModel::sendTransactionDone);
        connect(&ipc, &DbixIPC::signTransactionDone, this, &TransactionModel::signTransactionDone);
        connect(&ipc, &DbixIPC::newTransaction, this, &TransactionModel::newTransaction);
        connect(&ipc, &DbixIPC::newBlock, this, &TransactionModel::newBlock);
        connect(&ipc, &DbixIPC::syncingChanged, this, &TransactionModel::syncingChanged);


        connect(&fNetManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(httpRequestDone(QNetworkReply*)));
        checkVersion(); // TODO: move this off at some point
    }

    quint64 TransactionModel::getBlockNumber() const {
        return fBlockNumber;
    }

    quint64 TransactionModel::getFirstBlock() const {
        return fFirstBlock;
    }

    quint64 TransactionModel::getLastBlock() const {
        return fLastBlock;
    }

    const QString& TransactionModel::getGasPrice() const {
        return fGasPrice;
    }

    const QString& TransactionModel::getGasEstimate() const {
        return fGasEstimate;
    }

    const QString& TransactionModel::getLatestVersion() const {
        return fLatestVersion;
    }

    QHash<int, QByteArray> TransactionModel::roleNames() const {
        QHash<int, QByteArray> roles;
        roles[THashRole] = "hash";
        roles[NonceRole] = "nonce";
        roles[SenderRole] = "sender";
        roles[ReceiverRole] = "receiver";
        roles[ValueRole] = "value";
        roles[BlockNumberRole] = "blocknumber";
        roles[BlockHashRole] = "blockhash";
        roles[TransactionIndexRole] = "tindex";
        roles[GasRole] = "gas";
        roles[GasPriceRole] = "gasprice";
        roles[InputRole] = "input";
        roles[DepthRole] = "depth";
        roles[SenderAliasRole] = "senderalias";
        roles[ReceiverAliasRole] = "receiveralias";

        return roles;
    }

    int TransactionModel::rowCount(const QModelIndex & parent __attribute__ ((unused))) const {
        return fTransactionList.size();
    }

    QVariant TransactionModel::data(const QModelIndex & index, int role) const {
        const int row = index.row();

        // calculate distance from current block
        if ( role == DepthRole ) {
            quint64 transBlockNum = fTransactionList.at(row).value(BlockNumberRole).toULongLong();
            if ( transBlockNum == 0 ) { // still pending
                return -1;
            }

            quint64 diff = fBlockNumber - transBlockNum;
            return diff;
        }

        return fTransactionList.at(row).value(role);
    }

    int TransactionModel::containsTransaction(const QString& hash) {
        int i = 0;
        foreach ( const TransactionInfo& t, fTransactionList ) {
            if ( t.value(THashRole).toString() == hash ) {
                return i;
            }
            i++;
        }

        return -1;
    }

    void TransactionModel::connectToServerDone() {
        fIpc.getBlockNumber();
        fIpc.getGasPrice();
    }

    void TransactionModel::getAccountsDone(const QStringList& list __attribute__((unused))) {
        refresh();
        loadHistory();
    }

    void TransactionModel::getBlockNumberDone(quint64 num) {
        if ( num <= fBlockNumber ) {
            return;
        }

        fBlockNumber = num;
        if ( fFirstBlock == 0 ) {
            fFirstBlock = num;
        }

        emit blockNumberChanged(num);

        if ( !fTransactionList.isEmpty() ) { // depth changed for all
            const QModelIndex& leftIndex = QAbstractListModel::createIndex(0, 5);
            const QModelIndex& rightIndex = QAbstractListModel::createIndex(fTransactionList.length() - 1, 5);
            emit dataChanged(leftIndex, rightIndex, QVector<int>(1, DepthRole));
        }
    }

    void TransactionModel::getGasPriceDone(const QString& num) {
        if ( num != fGasEstimate ) {
            fGasPrice = num;
            emit gasPriceChanged(num);
        }
    }

    void TransactionModel::estimateGasDone(const QString& num) {
        if ( num != fGasEstimate ) {
            fGasEstimate = num;
            emit gasEstimateChanged(num);
        }
    }

    void TransactionModel::sendTransaction(const QString& password, const QString& from, const QString& to,
                                           const QString& value, quint64 nonce, const QString& gas, const QString& gasPrice,
                                           const QString& data) {
        Dubaicoin::Tx tx(from, to, value, nonce, gas, gasPrice, data); // nonce not required here, ipc.sendTransaction doesn't fill it in as it's known to gdbix
        fQueuedTransaction.init(from, to, value, gas, gasPrice, data);

        if ( fIpc.isThinClient() ) {
            fIpc.signTransaction(tx, password);
        } else {
            fIpc.sendTransaction(tx, password);
        }
    }

    void TransactionModel::onRawTransaction(const Dubaicoin::Tx& tx)
    {
        fQueuedTransaction.init(tx.fromStr(), tx.toStr(), tx.valueStr(), tx.gasStr(), tx.gasPriceStr(), tx.dataStr());
        fIpc.sendRawTransaction(tx);
    }

    void TransactionModel::sendTransactionDone(const QString& hash) {
        fQueuedTransaction.setHash(hash);
        addTransaction(fQueuedTransaction);
        DbixLog::logMsg("Transaction sent, hash: " + hash);
    }

    void TransactionModel::signTransactionDone(const QString &hash)
    {
        fIpc.sendRawTransaction(hash);
    }

    void TransactionModel::newTransaction(const TransactionInfo &info) {
        int ai1, ai2;
        const QString& sender = info.value(SenderRole).toString().toLower();
        const QString& receiver = info.value(ReceiverRole).toString().toLower();
        if ( fAccountModel.containsAccount(sender, receiver, ai1, ai2) ) { // either our sent or someone sent to us
            const int n = containsTransaction(info.value(THashRole).toString());
            if ( n >= 0 ) { // ours
                fTransactionList[n] = info;
                const QModelIndex& leftIndex = QAbstractListModel::createIndex(n, 0);
                const QModelIndex& rightIndex = QAbstractListModel::createIndex(n, 14);
                emit dataChanged(leftIndex, rightIndex);
                storeTransaction(fTransactionList.at(n));
            } else { // external from someone to us
                addTransaction(info);
            }
        }
    }

    void TransactionModel::newBlock(const QJsonObject& block) {
        const QJsonArray transactions = block.value("transactions").toArray();
        const quint64 blockNum = Helpers::toQUInt64(block.value("number"));

        if ( blockNum == 0 ) {
            return; // not interested in pending blocks
        }

        fIpc.getGasPrice(); // let's update our gas price

        foreach ( QJsonValue t, transactions ) {
            const QJsonObject to = t.toObject();
            const QString thash = to.value("hash").toString();
            const QString sender = to.value("from").toString().toLower();
            const QString receiver = to.value("to").toString().toLower();
            int i1, i2;

            const int n = containsTransaction(thash);
            if ( n >= 0 ) {
                fTransactionList[n].init(to);
                const QModelIndex& leftIndex = QAbstractListModel::createIndex(n, 0);
                const QModelIndex& rightIndex = QAbstractListModel::createIndex(n, 14);
                QVector<int> roles(2);
                roles[0] = BlockNumberRole;
                roles[1] = DepthRole;
                emit dataChanged(leftIndex, rightIndex, roles);
                const TransactionInfo info = fTransactionList.at(n);
                storeTransaction(fTransactionList.at(n));
                emit confirmedTransaction(info.value(ReceiverRole).toString(), info.value(THashRole).toString());
            } else if ( fAccountModel.containsAccount(sender, receiver, i1, i2) ) {
                const TransactionInfo info = TransactionInfo(to);
                addTransaction(info);
                emit receivedTransaction(info.value(ReceiverRole).toString());
            }
        }
    }

    void TransactionModel::syncingChanged(bool syncing)
    {
        if ( !syncing ) {
            refresh();
        }
    }

    int TransactionModel::getInsertIndex(const TransactionInfo& info) const {
        const quint64 block = info.value(BlockNumberRole).toULongLong();

        if ( block == 0 ) {
            return 0; // new/pending
        }

        for ( int i = 0; i < fTransactionList.length(); i++ ) {
            const quint64 oldBlock = fTransactionList.at(i).value(BlockNumberRole).toULongLong();
            if ( oldBlock <= block ) {
                return i;
            }
        }

        return fTransactionList.length();
    }

    void TransactionModel::addTransaction(const TransactionInfo& info) {
        const int index = getInsertIndex(info);
        beginInsertRows(QModelIndex(), index, index);
        fTransactionList.insert(index, info);
        endInsertRows();

        storeTransaction(info);
    }

    void TransactionModel::storeTransaction(const TransactionInfo& info) {
        // save to persistent memory for re-run
        const quint64 blockNum = info.value(BlockNumberRole).toULongLong();
        QSettings settings;
        settings.beginGroup("transactions");
        settings.setValue(Helpers::toDecStr(blockNum) + "_" + info.value(TransactionIndexRole).toString(), info.toJsonString());
        settings.endGroup();
    }

    bool transCompare(const TransactionInfo& a, const TransactionInfo& b) {
        return a.getBlockNumber() > b.getBlockNumber();
    }

    void TransactionModel::refresh() {
        QSettings settings;
        settings.beginGroup("transactions");
        QStringList list = settings.allKeys();

        foreach ( const QString bns, list ) {
            const QString val = settings.value(bns, "bogus").toString();
            if ( val.contains("{") ) { // new format, get data and reload only recent transactions
                QJsonParseError parseError;
                const QJsonDocument jsonDoc = QJsonDocument::fromJson(val.toUtf8(), &parseError);

                if ( parseError.error != QJsonParseError::NoError ) {
                    DbixLog::logMsg("Error parsing stored transaction: " + parseError.errorString(), LS_Error);
                } else {
                    const TransactionInfo info(jsonDoc.object());
                    newTransaction(info);
                    // if transaction is newer than 1 day restore it from gdbix anyhow to ensure correctness in case of reorg
                    if ( info.getBlockNumber() == 0 || fBlockNumber - info.getBlockNumber() < 5400 ) {
                        fIpc.getTransactionByHash(info.getHash());
                    }
                }
            } else if ( val != "bogus" ) { // old format, re-get and store full data
                fIpc.getTransactionByHash(val);
                settings.remove(bns);
            }
        }
        settings.endGroup();

        qSort(fTransactionList.begin(), fTransactionList.end(), transCompare);
    }

    const QString TransactionModel::estimateTotal(const QString& value, const QString& gas, const QString& gasPrice) const {
        BigInt::Rossi valRossi = Helpers::dbixStrToRossi(value);
        BigInt::Rossi valGas = Helpers::decStrToRossi(gas);
        BigInt::Rossi valGasPrice = Helpers::dbixStrToRossi(gasPrice);

        const QString wei = QString((valRossi + valGas * valGasPrice).toStrDec().data());

        return Helpers::weiStrToDbixStr(wei);
    }

    const QString TransactionModel::getHash(int index) const {
        if ( index >= 0 && index < fTransactionList.length() ) {
            return fTransactionList.at(index).value(THashRole).toString();
        }

        return QString();
    }

    const QString TransactionModel::getSender(int index) const {
        if ( index >= 0 && index < fTransactionList.length() ) {
            return fTransactionList.at(index).value(SenderRole).toString();
        }

        return QString();
    }

    const QString TransactionModel::getReceiver(int index) const {
        if ( index >= 0 && index < fTransactionList.length() ) {
            return fTransactionList.at(index).value(ReceiverRole).toString();
        }

        return QString();
    }

    double TransactionModel::getValue(int index) const {
        if ( index >= 0 && index < fTransactionList.length() ) {
            return fTransactionList.at(index).value(ValueRole).toFloat();
        }

        return 0;
    }

    const QJsonObject TransactionModel::getJson(int index, bool decimal) const {
        if ( index < 0 || index >= fTransactionList.length() ) {
            return QJsonObject();
        }

        return fTransactionList.at(index).toJson(decimal);
    }

    const QString TransactionModel::getMaxValue(int row, const QString& gas, const QString& gasPrice) const {
        const QModelIndex index = QAbstractListModel::createIndex(row, 2);

        BigInt::Rossi balanceWeiRossi = Helpers::dbixStrToRossi( fAccountModel.data(index, BalanceRole).toString() );
        const BigInt::Rossi gasRossi = Helpers::decStrToRossi(gas);
        const BigInt::Rossi gasPriceRossi = Helpers::dbixStrToRossi(gasPrice);
        const BigInt::Rossi gasTotalRossi = gasRossi * gasPriceRossi;

        if ( balanceWeiRossi < gasTotalRossi ) {
            return "0";
        }
        const BigInt::Rossi resultWeiRossi = (balanceWeiRossi - gasTotalRossi);

        const QString resultWei = QString(resultWeiRossi.toStrDec().data());

        return Helpers::weiStrToDbixStr(resultWei);
    }

    void TransactionModel::lookupAccountsAliases() {
        for ( int n = 0; n < fTransactionList.size(); n++ ) {
            fTransactionList[n].lookupAccountAliases();
        }

        QVector<int> roles(2);
        roles[0] = SenderRole;
        roles[1] = ReceiverRole;
        const QModelIndex& leftIndex = QAbstractListModel::createIndex(0, 0);
        const QModelIndex& rightIndex = QAbstractListModel::createIndex(fTransactionList.size(), 0);

        emit dataChanged(leftIndex, rightIndex, roles);
    }

    void TransactionModel::httpRequestDone(QNetworkReply *reply) {
        const QString uri = reply->url().fileName();

        if ( uri == "version" ) {
            checkVersionDone(reply);
        } else if ( uri == "transactions" ) {
            loadHistoryDone(reply);
        } else {
            DbixLog::logMsg("Unknown uri from reply: " + uri, LS_Error);
        }
    }

    void TransactionModel::checkVersion() {
        // get latest app version
        QNetworkRequest request(QUrl("http://dbixscan.io/api/version"));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QJsonObject objectJson;
        const QByteArray data = QJsonDocument(objectJson).toJson();

        DbixLog::logMsg("HTTP Post request: " + data, LS_Debug);

        fNetManager.post(request, data);
    }

    void TransactionModel::checkVersionDone(QNetworkReply *reply) {
        QJsonObject resObj = Helpers::parseHTTPReply(reply);
        const bool success = resObj.value("success").toBool();

        if ( !success ) {
            const QString error = resObj.value("error").toString("unknown error");
            return DbixLog::logMsg("Response error: " + error, LS_Error);
        }
        const QJsonValue rv = resObj.value("result");
        fLatestVersion = rv.toString("0.0.0");
        int latestIntVer = Helpers::parseAppVersion(fLatestVersion);
        int intVer = Helpers::parseAppVersion(QCoreApplication::applicationVersion());

        if ( intVer < latestIntVer ) {
            emit latestVersionChanged(fLatestVersion);
        } else {
            emit latestVersionSame(fLatestVersion);
        }
    }

    void TransactionModel::loadHistory() {
        QSettings settings;
        if ( fAccountModel.rowCount() == 0 || settings.value("gdbix/testnet", false).toBool() ) {
            return; // don't try with empty request or on test net/ETC
        }

        // get historical transactions from dbixdata
        QNetworkRequest request(QUrl("https://dbixscan.io/api/transactions"));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QJsonObject objectJson;
        objectJson["accounts"] = fAccountModel.getAccountsJsonArray();
        const QByteArray data = QJsonDocument(objectJson).toJson();

        DbixLog::logMsg("HTTP Post request: " + data, LS_Debug);

        fNetManager.post(request, data);
    }

    void TransactionModel::loadHistoryDone(QNetworkReply *reply) {
        QJsonObject resObj = Helpers::parseHTTPReply(reply);
        const bool success = resObj.value("success").toBool();

        if ( !success ) {
            const QString error = resObj.value("error").toString("unknown error");
            return DbixLog::logMsg("Response error: " + error, LS_Error);
        }
        const QJsonValue rv = resObj.value("result");
        const QJsonArray result = rv.toArray();

        int stored = 0;
        foreach ( const QJsonValue jv, result ) {
            const QJsonObject jo = jv.toObject();
            const QString hash = jo.value("hash").toString("bogus");

            if ( hash == "bogus" ) {
                return DbixLog::logMsg("Response hash missing", LS_Error);
            }

            if ( containsTransaction(hash) < 0 ) {
                fIpc.getTransactionByHash(hash);
                stored++;
            }
        }

        if ( stored > 0 ) {
            DbixLog::logMsg("Restored " + QString::number(stored) + " transactions from dbixdata server", LS_Info);
        }

        reply->close();
    }    

}
