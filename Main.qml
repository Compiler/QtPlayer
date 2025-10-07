import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import QtQuick.Dialogs

Window {
    id: window
    width: 1080
    height: 720
    visible: true
    title: qsTr("QtPlayer")

    readonly property string pathToBuffer: "file:///" + AssetsDir + "/buffer.tiff"
    readonly property real fps: 30
    readonly property real timestep: 1000 / fps

    Timer {
        interval: timestep
        repeat: true
        running:true

        onTriggered: {
            //AssetMaker.writeBuffer()
            AssetMaker.readAndWriteNext();
            img2.source = ""
            img2.source = pathToBuffer
        }
    }

    FileDialog {
        id: fileDialog
        title: "Select a file"
        nameFilters: [ "All files (*)" ]
        onAccepted: {
            console.log("Selected file: " + fileDialog.selectedFile)
            AssetMaker.openAndWrite(fileDialog.selectedFile)
        }
        onRejected: {
            console.log("File selection cancelled")
        }
    }

    Button {
        id: butt
        text: "Cycle & Write"
        anchors.right: fileDialog.left
        onClicked: {
            fileDialog.open()
            AssetMaker.writeBuffer()
        }
    }



    TextField {
        id: ts
        width: 180
        anchors.top: butt.bottom
        placeholderText: "0"
        focus: true
        onAccepted: if (acceptableInput) AssetMaker.seekTo(ts.text)
        EnterKey.type: Qt.EnterKeyDone
        Keys.onReturnPressed: accepted()
        Keys.onEnterPressed: accepted()
    }
    Button {
        id: randomSeek
        text: "haha"
        anchors.right: fileDialog.left
        anchors.top: ts.bottom
        onClicked: {
            AssetMaker.seekTo(Math.floor(Math.random() * 10000))
        }
    }


    RowLayout {
        id: splitPanes

        anchors.margins: 64
        anchors.fill: parent


        Image {
            id: img2
            Layout.fillWidth: true
            Layout.fillHeight: true
            source: "file:///" + AssetsDir + "/256x256_test.png"
            cache: false
            asynchronous: false
        }
    }



}
