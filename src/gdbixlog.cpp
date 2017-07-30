#include "gdbixlog.h"
#include <QDebug>
#include <QSettings>
#include <QApplication>
#include <QClipboard>

namespace Dbixwall {

    GdbixLog::GdbixLog() :
        QAbstractListModel(0), fList(), fProcess(0)
    {
    }

    QHash<int, QByteArray> GdbixLog::roleNames() const {
        QHash<int, QByteArray> roles;
        roles[MsgRole] = "msg";

        return roles;
    }

    int GdbixLog::rowCount(const QModelIndex & parent __attribute__ ((unused))) const {
        return fList.length();
    }

    QVariant GdbixLog::data(const QModelIndex & index, int role __attribute__ ((unused))) const {
        return fList.at(index.row());
    }

    void GdbixLog::saveToClipboard() const {
        QString text;

        foreach ( const QString& info, fList ) {
            text += (info + QString("\n"));
        }

        QApplication::clipboard()->setText(text);
    }

    void GdbixLog::attach(QProcess* process) {
        fProcess = process;
        connect(fProcess, &QProcess::readyReadStandardOutput, this, &GdbixLog::readStdout);
        connect(fProcess, &QProcess::readyReadStandardError, this, &GdbixLog::readStderr);
    }

    void GdbixLog::append(const QString& line) {
        fList.append(line);
    }

    void GdbixLog::readStdout() {
        const QByteArray ba = fProcess->readAllStandardOutput();
        const QString str = QString::fromUtf8(ba);
        beginInsertRows(QModelIndex(), 0, 0);
        fList.insert(0, str);
        endInsertRows();
        overflowCheck();
    }

    void GdbixLog::readStderr() {
        const QByteArray ba = fProcess->readAllStandardError();
        const QString str = QString::fromUtf8(ba);
        beginInsertRows(QModelIndex(), 0, 0);
        fList.insert(0, str);
        endInsertRows();
        overflowCheck();
    }

    void GdbixLog::overflowCheck() {
        if ( fList.length() > 100 ) {
            beginRemoveRows(QModelIndex(), fList.length() - 1, fList.length() - 1);
            fList.removeAt(fList.length() - 1);
            endRemoveRows();
        }
    }

}
