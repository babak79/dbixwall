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
/** @file types.h
 * @author Ales Katona <almindor@gmail.com>
 * @date 2015
 *
 * Types header
 */

#ifndef TYPES_H
#define TYPES_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QVariant>
#include <QStandardPaths>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QDateTime>

namespace Etherwall {

    static const quint64 SYNC_DEPTH = 10;
    static const QString EtherWall_Cert = "-----BEGIN CERTIFICATE-----\n"
            "MIIDiDCCAnACCQCXJXqGOlAorjANBgkqhkiG9w0BAQsFADCBhTELMAkGA1UEBhMC\n"
            "Q0ExEDAOBgNVBAgMB0FsYmVydGExEDAOBgNVBAcMB0NhbGdhcnkxEjAQBgNVBAoM\n"
            "CUV0aGVyZHluZTEbMBkGA1UEAwwSZGF0YS5ldGhlcndhbGwuY29tMSEwHwYJKoZI\n"
            "hvcNAQkBFhJhbG1pbmRvckBnbWFpbC5jb20wHhcNMTYwODI4MjM1MjI5WhcNMjIw\n"
            "MjE4MjM1MjI5WjCBhTELMAkGA1UEBhMCQ0ExEDAOBgNVBAgMB0FsYmVydGExEDAO\n"
            "BgNVBAcMB0NhbGdhcnkxEjAQBgNVBAoMCUV0aGVyZHluZTEbMBkGA1UEAwwSZGF0\n"
            "YS5ldGhlcndhbGwuY29tMSEwHwYJKoZIhvcNAQkBFhJhbG1pbmRvckBnbWFpbC5j\n"
            "b20wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCsraaryUowJKQSQLeV\n"
            "4r833OvTrI5ylyww1s3m+aN0eGx2tQa+rVIeklu9vO0bz5qWc6CnWvOx1ZA35Ru+\n"
            "OXkYzUe63Bkt0MtBDvfhGQct3vJ5r+6dXf8TuPgSaqBCpGCG4DtIYUxuNVauJ2N7\n"
            "HX/KOOY5J2qHlWpas2TNrncbvfc55A8ezP2p3dwmbyITG7IPhwicDDez6xPz0SXH\n"
            "USsv3Y3bQlVAh+11tquGcKo14QbRtPIkmaEHuG1nN4LaFhQL7P2gH8x7g2lOmsE+\n"
            "JIDAdHV2ExewAfIf4Ep+twPo6jXf96cKLhtKCfVlUKUBEvBxsFmEFlwciW1R7d7R\n"
            "TXNBAgMBAAEwDQYJKoZIhvcNAQELBQADggEBAIMtS+szahT3ScKAsk3Y1F1Ac/E3\n"
            "t9JJZhEGS4b73vlna2Rl0wQd2rPtVMgdTarshhH/jAngWaf52xzMd0MKlYEkzRdx\n"
            "D1tSdwqDxQ+XKxMBXwiN2ffgc+r8xcFWK34BU32MC7reVL1sQtjYAiGfSuo4ZZqk\n"
            "0WThQ183ERexxwtYQ8qSn3L+kXCPJyVnazt7IJ3rylB9e6t6voaU/eNQUC7Mdwov\n"
            "Vw6Ar9fz+sQVccQQDREICKnnK1M+k8kk+g3c+rF3ISFlLPi981tWjGSTH685HH0q\n"
            "JkX2TxeYmZl+B/qvVorfPzWK7NoalCBvIxyxBeI3e67Ly0lRWAGIsWEtQP4=\n"
            "-----END CERTIFICATE-----\n";

    enum ClientType {
        ClientUnknown = 0,
        ClientGeth = 1,
        ClientParity = 2
    };

    const QString DefaultNodeArgs();
    const QString DefaultIPCPath(const QString& dataDir, bool testnet);
    const QString DefaultNodeBinaryPath();
    const QString DefaultDataDir();
    const QString SelectedNodeTypeName();
    void SetupHardforkParams(bool hardfork, QStringList& args);

    enum LogRoles {
        MsgRole = Qt::UserRole + 1,
        DateRole,
        SeverityRole
    };

    enum LogSeverity {
        LS_Debug,
        LS_Info,
        LS_Warning,
        LS_Error
    };

    class LogInfo
    {
    public:
        LogInfo(const QString& info, LogSeverity sev);
        const QVariant value(int role) const;
    private:
        QString fMsg;
        QDateTime fDate;
        LogSeverity fSeverity;

        const QString getSeverityString() const;
    };

    typedef QList<LogInfo> LogList;

    enum CurrencyRoles {
        NameRole = Qt::UserRole + 1,
        PriceRole
    };

    class CurrencyInfo
    {
    public:
        CurrencyInfo( const QString name, const float price );
        const QVariant value(const int role) const;
        double recalculate(const float ether) const;
    private:
        QString fName;
        float fPrice;
    };

    typedef QList<CurrencyInfo> CurrencyInfos;

    enum RequestTypes {
        NoRequest,
        NewAccount,
        DeleteAccount,
        GetBlockNumber,
        GetAccountRefs,
        GetBalance,
        GetTransactionCount,
        GetPeerCount,
        SendTransaction,
        UnlockAccount,
        GetGasPrice,
        EstimateGas,
        NewBlockFilter,
        NewEventFilter,
        GetFilterChanges,
        UninstallFilter,
        GetTransactionByHash,
        GetBlock,
        GetClientVersion,
        GetNetVersion,
        GetSyncing,
        GetLogs,
        GetTransactionReceipt
    };

    enum AccountRoles {
        HashRole = Qt::UserRole + 1,
        BalanceRole,
        TransCountRole,
        SummaryRole,
        AliasRole,
        IndexRole
    };

    class AccountInfo
    {
    public:
        AccountInfo(const QString& hash, const QString& balance, quint64 transCount);

        const QVariant value(const int role) const;
        void setBalance(const QString& balance);
        void setTransactionCount(quint64 count);
        void lock();
        void unlock();
        bool isLocked() const;
        void alias(const QString& name);
    private:
        int fIndex;
        QString fHash;
        QString fBalance; // in ether
        quint64 fTransCount;
        QString fAlias;
        bool fLocked;
    };

    typedef QList<AccountInfo> AccountList;

    enum TransactionRoles {
        THashRole = Qt::UserRole + 1,
        NonceRole,
        SenderRole,
        ReceiverRole,
        ValueRole,
        BlockNumberRole,
        BlockHashRole,
        TransactionIndexRole,
        GasRole,
        GasPriceRole,
        InputRole,
        DepthRole,
        SenderAliasRole,
        ReceiverAliasRole
    };

    class TransactionInfo
    {
    public:
        TransactionInfo();
        TransactionInfo(const QJsonObject& source);
        TransactionInfo(const QString& hash, quint64 blockNum); // for storing from server reply

        const QVariant value(const int role) const;
        quint64 getBlockNumber() const;
        void setBlockNumber(quint64 num);
        const QString getHash() const;
        void setHash(const QString& hash);
        void init(const QString& from, const QString& to, const QString& value, const QString& gas = QString(),
                  const QString& gasPrice = QString(), const QString& data = QString());
        void init(const QJsonObject source);
        const QJsonObject toJson(bool decimal = false) const;
        const QString toJsonString(bool decimal = false) const;
        void lookupAccountAliases();
    private:
        QString fHash;
        quint64 fNonce;
        QString fSender;
        QString fReceiver;
        QString fValue; // in ether
        quint64 fBlockNumber;
        QString fBlockHash;
        quint64 fTransactionIndex;
        QString fGas;
        QString fGasPrice;
        QString fInput;
        QString fSenderAlias;
        QString fReceiverAlias;
    };

    typedef QList<TransactionInfo> TransactionList;

}

#endif // TYPES_H
