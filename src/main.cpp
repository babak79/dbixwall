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
/** @file main.cpp
 * @author Ales Katona <almindor@gmail.com>
 * @date 2017
 *
 * Main entry point
 */

#include <QApplication>
#include <QTranslator>
#include <QQmlApplicationEngine>
#include <QDir>
#include <QQmlContext>
#include <QtQml/qqml.h>
#include <QIcon>
#include <QPixmap>
#include <QFile>
#include <QDebug>
#include "dbixlog.h"
#include "settings.h"
#include "clipboard.h"
#include "accountmodel.h"
#include "accountproxymodel.h"
#include "transactionmodel.h"
#include "contractmodel.h"
#include "eventmodel.h"
#include "currencymodel.h"
#include "filtermodel.h"
#include "gdbixlog.h"
#include "helpers.h"
//#include "remoteipc.h"

//#include "trezor/trezor.h"
//#include "platform/devicemanager.h"

using namespace Dbixwall;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    qmlRegisterType<AccountProxyModel>("AccountProxyModel", 0, 1, "AccountProxyModel");

    QCoreApplication::setOrganizationName("Arabianchain");
    QCoreApplication::setOrganizationDomain("arabianchain.org");
    QCoreApplication::setApplicationName("Dbixwall");
    QCoreApplication::setApplicationVersion("2.0.0");
    app.setWindowIcon(QIcon(QPixmap(":/images/dbix-256")));

    QTranslator translator;
    translator.load("i18n/dbixwall_" + QLocale::system().name());
    app.installTranslator(&translator);

    Settings settings;

	bool testnet = settings.value("gdbix/testnet", false).toBool();
    const QString gdbixPath = settings.value("gdbix/path", DefaultGdbixPath()).toString();
    const QString dataPath = settings.value("gdbix/datadir", DefaultDataDir).toString();
	const QString ipcPath = DefaultIPCPath(dataPath, testnet);

    // set defaults
    if ( !settings.contains("gdbix/path") ) {
        settings.setValue("gdbix/path", gdbixPath);
    }
    if ( !settings.contains("gdbix/datadir") ) {
        settings.setValue("gdbix/datadir", dataPath);
    }

    ClipboardAdapter clipboard;
    DbixLog log;
    GdbixLog gdbixLog;

    /*// get SSL cert for https://data.dbixwall.com
    const QSslCertificate certificate(DbixWall_Cert.toUtf8());
    QSslSocket::addDefaultCaCertificate(certificate);*/

    //Trezor::TrezorDevice trezor;
    //DeviceManager deviceManager(app);
    //RemoteIPC ipc(gdbixLog, "");
	DbixIPC ipc(ipcPath, gdbixLog);
    CurrencyModel currencyModel;
    AccountModel accountModel(ipc, currencyModel);
    TransactionModel transactionModel(ipc, accountModel);
    ContractModel contractModel(ipc);
    FilterModel filterModel(ipc);
    EventModel eventModel(contractModel, filterModel);

    // main connections
    //QObject::connect(&accountModel, &AccountModel::accountsReady);
    //QObject::connect(&deviceManager, &DeviceManager::deviceInserted, &trezor, &Trezor::TrezorDevice::onDeviceInserted);
    //QObject::connect(&deviceManager, &DeviceManager::deviceRemoved, &trezor, &Trezor::TrezorDevice::onDeviceRemoved);
    //QObject::connect(&transactionModel, &TransactionModel::onRawTransaction);

    // for QML only
    QmlHelpers qmlHelpers;

    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty("settings", &settings);
    engine.rootContext()->setContextProperty("ipc", &ipc);
    //engine.rootContext()->setContextProperty("trezor", &trezor);
    engine.rootContext()->setContextProperty("accountModel", &accountModel);
    engine.rootContext()->setContextProperty("transactionModel", &transactionModel);
    engine.rootContext()->setContextProperty("contractModel", &contractModel);
    engine.rootContext()->setContextProperty("filterModel", &filterModel);
    engine.rootContext()->setContextProperty("eventModel", &eventModel);
    engine.rootContext()->setContextProperty("currencyModel", &currencyModel);
    engine.rootContext()->setContextProperty("clipboard", &clipboard);
    engine.rootContext()->setContextProperty("log", &log);
    engine.rootContext()->setContextProperty("gdbix", &gdbixLog);
    engine.rootContext()->setContextProperty("helpers", &qmlHelpers);

    engine.load(QUrl(QStringLiteral("qrc:///main.qml")));

    /*if ( settings.contains("program/v2firstrun") ) {
        ipc.init();
    } else {
        settings.remove("gdbix/testnet"); // cannot be set from before in the firstrun, not compatible with thin client
    }*/
	ipc.init();

    return app.exec();
}
