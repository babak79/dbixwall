/*
    This file is part of etherwall.
    etherwall is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    etherwall is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with etherwall. If not, see <http://www.gnu.org/licenses/>.
*/
/** @file accountmodel.cpp
 * @author Ales Katona <almindor@gmail.com>
 * @date 2015
 *
 * Account model implementation
 */

#include "accountmodel.h"
#include "types.h"
#include "helpers.h"
#include <QDebug>
#include <QSettings>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>

namespace Etherwall {

    AccountModel::AccountModel(EtherIPC& ipc, const CurrencyModel& currencyModel) :
        QAbstractListModel(0), fIpc(ipc), fAccountList(), fSelectedAccountRow(-1), fCurrencyModel(currencyModel), fBusy(false)
    {
        connect(&ipc, &EtherIPC::connectToServerDone, this, &AccountModel::connectToServerDone);
        connect(&ipc, &EtherIPC::getAccountsDone, this, &AccountModel::getAccountsDone);
        connect(&ipc, &EtherIPC::newAccountDone, this, &AccountModel::newAccountDone);
        connect(&ipc, &EtherIPC::deleteAccountDone, this, &AccountModel::deleteAccountDone);
        connect(&ipc, &EtherIPC::accountChanged, this, &AccountModel::accountChanged);
        connect(&ipc, &EtherIPC::newBlock, this, &AccountModel::newBlock);
        connect(&ipc, &EtherIPC::syncingChanged, this, &AccountModel::syncingChanged);

        connect(&currencyModel, &CurrencyModel::currencyChanged, this, &AccountModel::currencyChanged);
    }

    QHash<int, QByteArray> AccountModel::roleNames() const {
        QHash<int, QByteArray> roles;
        roles[HashRole] = "hash";
        roles[BalanceRole] = "balance";
        roles[TransCountRole] = "transactions";
        roles[SummaryRole] = "summary";
        roles[AliasRole] = "alias";
        roles[IndexRole] = "index";

        return roles;
    }

    int AccountModel::rowCount(const QModelIndex & parent __attribute__ ((unused))) const {
        return fAccountList.size();
    }

    QVariant AccountModel::data(const QModelIndex & index, int role) const {
        const int row = index.row();

        QVariant result = fAccountList.at(row).value(role);
        if ( role == BalanceRole ) {
            return fCurrencyModel.recalculate(result);
        }

        return result;
    }

    // TODO: optimize with hashmap
    bool AccountModel::containsAccount(const QString& from, const QString& to, int& i1, int& i2) const {
        i1 = -1;
        i2 = -1;
        int i = 0;
        foreach ( const AccountInfo& a, fAccountList ) {
            const QString addr = a.value(HashRole).toString().toLower();
            if ( addr == from ) {
                i1 = i;
            }

            if ( addr == to ) {
                i2 = i;
            }
            i++;
        }

        return (i1 >= 0 || i2 >= 0);
    }

    const QString AccountModel::getTotal() const {
        BigInt::Rossi total;

        foreach ( const AccountInfo& info, fAccountList ) {
            const QVariant balance = info.value(BalanceRole);
            double dVal = fCurrencyModel.recalculate(balance).toDouble();
            const QString strVal = QString::number(dVal, 'f', 18);
            total += Helpers::etherStrToRossi(strVal);
        }

        const QString weiStr = QString(total.toStrDec().data());
        return Helpers::weiStrToEtherStr(weiStr);
    }

    void AccountModel::newAccount(const QString& pw) {
        const int index = fAccountList.size();
        fIpc.newAccount(pw, index);
    }

    void AccountModel::renameAccount(const QString& name, int index) {
        if ( index >= 0 && index < fAccountList.size() ) {
            fAccountList[index].alias(name);

            QVector<int> roles(2);
            roles[0] = AliasRole;
            roles[1] = SummaryRole;
            const QModelIndex& modelIndex = QAbstractListModel::createIndex(index, 0);

            emit dataChanged(modelIndex, modelIndex, roles);
        } else {
            EtherLog::logMsg("Invalid account selection for rename", LS_Error);
        }
    }

    void AccountModel::deleteAccount(const QString& pw, int index) {
        if ( index >= 0 && index < fAccountList.size() ) {            
            const QString hash = fAccountList.at(index).value(HashRole).toString();
            fIpc.deleteAccount(hash, pw, index);
        } else {
            EtherLog::logMsg("Invalid account selection for delete", LS_Error);
        }
    }

    const QString AccountModel::getAccountHash(int index) const {
        if ( index >= 0 && fAccountList.length() > index ) {
            return fAccountList.at(index).value(HashRole).toString();
        }

        return QString();
    }

    bool AccountModel::exportAccount(const QUrl& dir, int index) {
        if ( index < 0 || index >= fAccountList.length() ) {
            return false;
        }

        const QDir keystore = Helpers::getKeystoreDir(false);
        QString address = fAccountList.at(index).value(HashRole).toString();
        const QString data = Helpers::exportAddress(keystore, address);
        const QString fileName = Helpers::getAddressFilename(keystore, address);
        QDir directory(dir.toLocalFile());
        QFile file(directory.absoluteFilePath(fileName));
        if ( !file.open(QFile::WriteOnly) ) {
            return false;
        }
        QTextStream stream( &file );
        stream << data;
        file.close();

        return true;
    }

    void AccountModel::exportWallet(const QUrl& fileName) const {
        const QDir keystore = Helpers::getKeystoreDir(false);

        try {
            QByteArray backupData = Helpers::createBackup(keystore);
            QString strName = fileName.toLocalFile();
            const QFileInfo fileInfo(strName);
            if ( fileInfo.completeSuffix() != "etherwall" ) {
                strName += ".etherwall"; // force suffix
            }

            QFile file(strName);
            if ( !file.open(QFile::WriteOnly) ) {
                throw file.errorString();
            }
            if ( file.write(backupData) < backupData.size() ) {
                throw file.errorString();
            }
            file.close();
        } catch ( QString err ) {
            emit walletErrorEvent(err);
            return;
        }

        emit walletExportedEvent();
    }

    void AccountModel::importWallet(const QUrl& fileName) {
        try {
            const QDir keystore = Helpers::getKeystoreDir(false);
            QFile file(fileName.toLocalFile());
            if ( !file.exists() ) {
                throw QString("Wallet backup file not found");
            }
            if ( !file.open(QFile::ReadOnly) ) {
                throw file.errorString();
            }
            QByteArray backupData = file.readAll();
            Helpers::restoreBackup(backupData, keystore);
            file.close();
        } catch ( QString err ) {
            emit walletErrorEvent(err);
            return;
        }

        fBusy = true;
        emit busyChanged(true);
        QTimer::singleShot(2000, this, SLOT(importWalletDone()));
    }

    void AccountModel::importWalletDone() {
        fBusy = false;
        emit busyChanged(false);
        fIpc.getAccounts();
        emit walletImportedEvent();
    }

    void AccountModel::connectToServerDone() {
        fIpc.getAccounts();
    }

    void AccountModel::newAccountDone(const QString& hash, int index) {
        if ( !hash.isEmpty() ) {
            beginInsertRows(QModelIndex(), index, index);
            fAccountList.append(AccountInfo(hash, "0.00000000000000000", 0));
            endInsertRows();
            EtherLog::logMsg("New account created");
        } else {
            EtherLog::logMsg("Account create failure");
        }
    }

    void AccountModel::deleteAccountDone(bool result, int index) {
        if ( result ) {
            beginRemoveRows(QModelIndex(), index, index);
            fAccountList.removeAt(index);
            endRemoveRows();
            EtherLog::logMsg("Account deleted");
        } else {
            EtherLog::logMsg("Account delete failure");
        }
    }

    void AccountModel::currencyChanged() {
        QVector<int> roles(1);
        roles[0] = BalanceRole;

        const QModelIndex& leftIndex = QAbstractListModel::createIndex(0, 0);
        const QModelIndex& rightIndex = QAbstractListModel::createIndex(fAccountList.size() - 1, 4);
        emit dataChanged(leftIndex, rightIndex, roles);
        emit totalChanged();
    }

    void AccountModel::syncingChanged(bool syncing) {
        if ( !syncing ) {
            refreshAccounts();
        }
    }

    void AccountModel::getAccountsDone(const AccountList& list) {
        beginResetModel();
        fAccountList = list;
        endResetModel();

        refreshAccounts();
    }

    void AccountModel::refreshAccounts() {
        //qDebug() << "Refreshing accounts\n";
        int i = 0;
        foreach ( const AccountInfo& info, fAccountList ) {
            const QString& hash = info.value(HashRole).toString();
            fIpc.refreshAccount(hash, i++);
        }

        emit totalChanged();
    }

    void AccountModel::accountChanged(const AccountInfo& info) {
        int index = 0;
        const QString infoHash = info.value(HashRole).toString();
        foreach ( const AccountInfo& a, fAccountList ) {
            if ( a.value(HashRole).toString() == infoHash ) {
                fAccountList[index] = info;
                const QModelIndex& leftIndex = QAbstractListModel::createIndex(index, 0);
                const QModelIndex& rightIndex = QAbstractListModel::createIndex(index, 4);
                emit dataChanged(leftIndex, rightIndex);
                emit totalChanged();
                return;
            }
            index++;
        }

        const int len = fAccountList.length();
        beginInsertRows(QModelIndex(), len, len);
        fAccountList.append(info);
        endInsertRows();

        emit totalChanged();
    }

    void AccountModel::newBlock(const QJsonObject& block) {
        const QJsonArray transactions = block.value("transactions").toArray();
        const QString miner = block.value("miner").toString("bogus").toLower();
        int i1, i2;
        if ( containsAccount(miner, "bogus", i1, i2) ) {
            fIpc.refreshAccount(miner, i1);
        }

        foreach ( QJsonValue t, transactions ) {
            const QJsonObject to = t.toObject();
            const TransactionInfo info(to);
            const QString& sender = info.value(SenderRole).toString().toLower();
            const QString& receiver = info.value(ReceiverRole).toString().toLower();

            if ( containsAccount(sender, receiver, i1, i2) ) {
                if ( i1 >= 0 ) {
                    fIpc.refreshAccount(sender, i1);
                }

                if ( i2 >= 0 ) {
                    fIpc.refreshAccount(receiver, i2);
                }
            }
        }

        emit totalChanged();
    }

    int AccountModel::getSelectedAccountRow() const {
        return fSelectedAccountRow;
    }

    void AccountModel::setSelectedAccountRow(int row) {
        fSelectedAccountRow = row;
        emit accountSelectionChanged(row);
    }

    const QString AccountModel::getSelectedAccount() const {
        return getAccountHash(fSelectedAccountRow);
    }

    const QJsonArray AccountModel::getAccountsJsonArray() const {
        QJsonArray result;

        foreach ( const AccountInfo ai, fAccountList ) {
            const QString hash = ai.value(HashRole).toString();
            result.append(hash);
        }

        return result;
    }

}
