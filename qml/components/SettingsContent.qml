import QtQuick 2.0
import QtQuick.Controls 1.4
import QtQuick.Dialogs 1.2
import QtQuick.Extras 1.4

TabView {
    Tab {
        title: qsTr("Basic")

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
                    onValueChanged: {
                        settings.setValue("ipc/interval", intervalSpinBox.value)
                        ipc.setInterval(intervalSpinBox.value * 1000)
                    }
                }
            }

            ErrorDialog {
                id: hfConfirmDialog
                title: qsTr("Warning")
                msg: qsTr("Changing hard fork decision requires a restart of Dbixwall (and gdbix if running externally).")
            }

            Row {
                width: parent.width

                Label {
                    text: qsTr("Support DAO hard fork: ")
                }

                ToggleButton {
                    id: hfButton

                    checked: settings.valueBool("gdbix/hardfork", true)
                    text: checked ? qsTr("support") : qsTr("oppose")
                    onClicked: {
                        settings.setValue("gdbix/hardfork", checked)
                        if ( settings.contains("program/firstrun") ) {
                            hfConfirmDialog.show()
                        }
                    }
                }
            }
        }
    }

    Tab {
        title: qsTr("Advanced")

        Column {
            anchors.margins: 0.2 * dpi
            anchors.fill: parent
            spacing: 0.1 * dpi

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
                id: confirmDialog
                title: qsTr("Warning")
                msg: qsTr("Changing the chain requires a restart of Dbixwall (and gdbix if running externally).")
            }

            Row {
                id: rowGdbixTestnet
                width: parent.width

                Label {
                    id: gdbixTestnetLabel
                    text: "Testnet: "
                }

                CheckBox {
                    id: gdbixTestnetCheck
                    checked: settings.valueBool("gdbix/testnet", false)
                    onClicked: {
                        settings.setValue("gdbix/testnet", gdbixTestnetCheck.checked)
                        if ( settings.contains("program/firstrun") ) {
                            confirmDialog.show()
                        }
                    }
                }
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
                    text: settings.value("gdbix/args", "--fast --cache 512")
                    onTextChanged: {
                        settings.setValue("gdbix/args", gdbixArgsField.text)
                    }
                }
            }

            Row {
                id: rowLogBlocks
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

}
