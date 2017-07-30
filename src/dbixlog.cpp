#include "dbixlog.h"
#include <QDebug>
#include <QSettings>
#include <QApplication>
#include <QClipboard>

namespace Dbixwall {

    static DbixLog* sLog = NULL;

    DbixLog::DbixLog() :
        QAbstractListModel(0), fList()
    {
        sLog = this;
        const QSettings settings;
        fLogLevel = (LogSeverity)settings.value("program/loglevel", LS_Info).toInt();
    }

    QHash<int, QByteArray> DbixLog::roleNames() const {
        QHash<int, QByteArray> roles;
        roles[MsgRole] = "msg";
        roles[DateRole] = "date";
        roles[SeverityRole] = "severity";

        return roles;
    }

    int DbixLog::rowCount(const QModelIndex & parent __attribute__ ((unused))) const {
        return fList.length();
    }

    QVariant DbixLog::data(const QModelIndex & index, int role) const {
        return fList.at(index.row()).value(role);
    }

    void DbixLog::saveToClipboard() const {
        QString text;

        foreach ( const LogInfo info, fList ) {
            text += (info.value(MsgRole).toString() + QString("\n"));
        }

        QApplication::clipboard()->setText(text);
    }

    void DbixLog::logMsg(const QString &msg, LogSeverity sev) {
        sLog->log(msg, sev);
    }

    void DbixLog::log(QString msg, LogSeverity sev) {
        if ( sev < fLogLevel ) {
            return; // skip due to severity setting
        }

        if ( msg.contains("personal_unlockAccount") ) {
            msg = "account content *REDACTED*";
        }

        beginInsertRows(QModelIndex(), 0, 0);
        fList.insert(0, LogInfo(msg, sev));
        endInsertRows();
    }

    int DbixLog::getLogLevel() const {
        return fLogLevel;
    }

    void DbixLog::setLogLevel(int ll) {
        fLogLevel = (LogSeverity)ll;
        QSettings settings;
        settings.setValue("program/loglevel", ll);
    }

}
