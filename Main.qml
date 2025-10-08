import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import QtQuick.Dialogs
import QtPlayer 1.0

ApplicationWindow {
    id: window
    width: 1080
    height: 720
    visible: true
    title: qsTr("QtPlayer")

    readonly property string pathToBuffer: "file:///" + AssetsDir + "/buffer.tiff"
    readonly property real fps: 30
    readonly property real timestep: 1000 / fps
    property bool play: false

    Timer {
        interval: timestep
        repeat: true
        running:true

        onTriggered: {
            //AssetMaker.writeBuffer()
            if(play) AssetMaker.readAndWriteNext();
            img2.source = ""
            img2.source = pathToBuffer
            videoView.angle += 1 % 360
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

    RowLayout {
        width: parent.width
        height: 32
        spacing: 16
        Button {
            id: butt
            text: "Open input"
            anchors.right: fileDialog.left
            onClicked: {
                fileDialog.open()
                AssetMaker.writeBuffer()
            }
        }



        TextField {
            id: ts
            width: 180
            placeholderText: "enter frame number"
            focus: true
            onAccepted:AssetMaker.seekTo(Number(ts.text) * timestep)
            EnterKey.type: Qt.EnterKeyDone
            Keys.onReturnPressed: accepted()
            Keys.onEnterPressed: accepted()
        }
        Button {
            id: playbutt
            text: play ? "Pause" : "Play"
            anchors.right: fileDialog.left
            onClicked: {
                play = !play
            }
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

        RhiTextureItem {
            id: videoView
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
        RhiTextureItem {
            id: videoView2
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }


    Component.onCompleted: Qt.application.style = "Fusion"

}
