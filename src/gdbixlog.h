#ifndef GDBIXLOG_H
#define GDBIXLOG_H

#include <QObject>
#include <QStringList>
#include <QAbstractListModel>
#include <QProcess>
#include "types.h"

namespace Dbixwall {

    class GdbixLog : public QAbstractListModel
    {
        Q_OBJECT
    public:
        GdbixLog();

        QHash<int, QByteArray> roleNames() const;
        int rowCount(const QModelIndex & parent = QModelIndex()) const;
        QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
        Q_INVOKABLE void saveToClipboard() const;
        void attach(QProcess* process);
        void append(const QString& line);
    private:
        QStringList fList;
        QProcess* fProcess;
        void readStdout();
        void readStderr();
        void readData();
        void overflowCheck();
    };

}

#endif // GDBIXLOG_H
