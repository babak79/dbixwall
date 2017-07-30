import QtQuick 2.0
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.2
import QtQuick.Extras 1.4

TabView {
    property bool hideTrezor: false
    property bool thinClient: ipc.thinClient

    Tab {
        title: qsTr("Basic")

        Row {
            spacing: 0.5 * dpi
            anchors.margins: 0.2 * dpi
            anchors {
                top: parent.top
                bottom: parent.bottom
                left: parent.left
            }

            // TODO: rename to infodialog
            ErrorDialog {
                id: confirmThinClientDialog
                title: qsTr("Warning")
                msg: qsTr("Changing node type requires a restart of Dbixwall.")
            }

            ToggleButton {
                id: clientModeButton
                width: 1 * dpi
                text: qsTr("Thin client")
                checked: thinClient

                onClicked: {
                    thinClient = clientModeButton.checked
                    settings.setValue("gdbix/thinclient", clientModeButton.checked)

                    if ( clientModeButton.checked ) {
                        settings.setValue("gdbix/testnet", false)
                    }

                    if ( settings.contains("program/v2firstrun") ) {
                        confirmThinClientDialog.show()
                    }
                }
            }

            Column {
                spacing: 0.1 * dpi
                width: 5 * dpi
                height: 3 * dpi


                Row {
                    width: parent.width

                    Label {
                        text: qsTr("Helper currency: ")
                    }

                    ComboBox {
                        id: defaultFiatCombo
                        width: 1 * dpi
                        model: currencyModel
                        textRole: "name"
                        currentIndex: currencyModel.helperIndex

                        onActivated: currencyModel.setHelperIndex(index)
                    }
                }
            }
        }
    }

    Tab {
        title: qsTr("Gdbix")

        Column {
            anchors.margins: 0.2 * dpi
            anchors.fill: parent
            spacing: 0.1 * dpi

            Row {
                id: rowGdbixDatadir
                width: parent.width

                Label {
                    id: gdbixDDLabel
                    text: "Gdbix Data Directory: "
                }

                TextField {
                    id: gdbixDDField
                    width: parent.width - gdbixDDButton.width - gdbixDDLabel.width
                    text: settings.value("gdbix/datadir", "")
                    onTextChanged: {
                        settings.setValue("gdbix/datadir", gdbixDDField.text)
                    }
                }

                Button {
                    id: gdbixDDButton
                    text: qsTr("Choose")

                    onClicked: {
                        ddFileDialog.open()
                    }
                }

                FileDialog {
                    id: ddFileDialog
                    title: qsTr("Gdbix data directory")
                    selectFolder: true
                    selectExisting: true
                    selectMultiple: false

                    onAccepted: {
                        gdbixDDField.text = helpers.localURLToString(ddFileDialog.fileUrl)
                    }
                }
            }

            Row {
                id: rowGdbixPath
                width: parent.width

                Label {
                    id: gdbixPathLabel
                    text: "Gdbix path: "
                }

                TextField {
                    id: gdbixPathField
                    width: parent.width - gdbixPathLabel.width - gdbixPathButton.width
                    text: settings.value("gdbix/path", "")
                    onTextChanged: {
                        settings.setValue("gdbix/path", gdbixPathField.text)
                    }
                }

                Button {
                    id: gdbixPathButton
                    text: qsTr("Choose")

                    onClicked: {
                        gdbixFileDialog.open()
                    }
                }

                FileDialog {
                    id: gdbixFileDialog
                    title: qsTr("Gdbix executable")
                    selectFolder: false
                    selectExisting: true
                    selectMultiple: false

                    onAccepted: {
                        gdbixPathField.text = helpers.localURLToString(gdbixFileDialog.fileUrl)
                    }
                }
            }

            // TODO: rename to infodialog
            ErrorDialog {
                id: confirmDialogTestnet
                title: qsTr("Warning")
                msg: qsTr("Changing the chain requires a restart of Dbixwall (and gdbix if running externally).")
            }

            Row {
                id: rowGdbixArgs
                width: parent.width

                Label {
                    id: gdbixArgsLabel
                    text: "Additional Gdbix args: "
                }

                TextField {
                    id: gdbixArgsField
                    width: parent.width - gdbixArgsLabel.width
                    text: settings.value("gdbix/args", "--syncmode=fast --cache 512")
                    onTextChanged: {
                        settings.setValue("gdbix/args", gdbixArgsField.text)
                    }
                }
            }

            Row {
                enabled: !thinClient
                id: rowGdbixTestnet
                width: parent.width

                Label {
                    id: gdbixTestnetLabel
                    text: "Testnet (testnet): "
                }

                CheckBox {
                    id: gdbixTestnetCheck
                    checked: settings.valueBool("gdbix/testnet", false)
                    onClicked: {
                        settings.setValue("gdbix/testnet", gdbixTestnetCheck.checked)
                        if ( settings.contains("program/v2firstrun") ) {
                            confirmDialogTestnet.show()
                        }
                    }
                }
            }
        }
    }

    Tab {
        title: qsTr("Advanced")
        enabled: !ipc.thinClient

        Column {
            anchors.margins: 0.2 * dpi
            anchors.fill: parent
            spacing: 0.1 * dpi


            Row {
                enabled: !thinClient
                width: parent.width

                Label {
                    text: qsTr("Update interval (s): ")
                }

                SpinBox {
                    id: intervalSpinBox
                    width: 1 * dpi
                    minimumValue: 5
                    maximumValue: 60

                    value: settings.value("ipc/interval", 10)
                    onValueChanged: ipc.setInterval(intervalSpinBox.value * 1000)
                }
            }

            Row {
                id: rowLogBlocks
                enabled: !thinClient
                width: parent.width

                Label {
                    id: logBlocksLabel
                    text: qsTr("Event history in blocks: ")
                }

                SpinBox {
                    id: logBlocksField
                    width: 1 * dpi
                    minimumValue: 0
                    maximumValue: 100000
                    value: settings.value("gdbix/logsize", 7200)
                    onValueChanged: {
                        settings.setValue("gdbix/logsize", logBlocksField.value)
                        filterModel.loadLogs()
                    }
                }
            }
        }
    }


    Tab {
        enabled: !hideTrezor
        title: "TREZOR"

        Column {
            anchors.margins: 0.2 * dpi
            anchors.fill: parent
            spacing: 0.1 * dpi

            /*Button {
                enabled: trezor.initialized
                text: qsTr("Import accounts")
                onClicked: trezorImportDialog.open('<a href="http://www.dbixwall.com/faq/#importaccount">' + qsTr("Import addresses from TREZOR?") + '</a>')
            }*/

            Row {
                spacing: 0.05 * dpi

                ConfirmDialog {
                    id: accountRemoveDialog
                    title: qsTr("Confirm removal of all TREZOR accounts")
                    msg: qsTr("All your TREZOR accounts will be removed from Dbixwall") + ' <a href="http://www.dbixwall.com/faq/#removeaccount">?</a>'
                    onYes: accountModel.removeAccounts()
                }

                Label {
                    text: qsTr("Clear TREZOR accounts")
                }

                Button {
                    text: qsTr("Clear")
                    onClicked: accountRemoveDialog.open()
                }
            }
        }
    }


}
