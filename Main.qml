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
    property real fps: 60
    readonly property real timestep: 1000 / fps
    property bool play: false

    // Scene Graph FPS
    property int sgFramesThisSecond: 0
    property int sgFps: 0

    Connections {
        target: window
        function onFrameSwapped() {
            sgFramesThisSecond++
        }
    }
    Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: {
            sgFps = sgFramesThisSecond
            sgFramesThisSecond = 0
        }
    }

    Timer {
        interval: timestep
        repeat: true
        running:true

        onTriggered: {
            //AssetMaker._writeBuffer()
            if(play) AssetMaker._readAndWriteNext();
            videoView2.angle += 1 % 360
        }
    }

    FileDialog {
        id: fileDialog
        title: "Select a file"
        nameFilters: [ "All files (*)" ]
        onAccepted: {
            console.log("Selected file: " + fileDialog.selectedFile)
            AssetMaker._openAndWrite(fileDialog.selectedFile)
        }
        onRejected: {
            console.log("File selection cancelled")
        }
    }

    RowLayout {
        width: parent.width
        height: 32
        spacing: 16
        Label {
            text: "SG FPS: " + sgFps
            font.pixelSize: 16
            color: "lime"
            background: Rectangle {
                radius: 6
                color: "#66000000"
            }
            padding: 6
        }
        Button {
            id: butt
            text: "Open input"
            anchors.right: fileDialog.left
            onClicked: {
                fileDialog.open()
                AssetMaker._writeBuffer()
            }
        }



        TextField {
            id: ts
            width: 180
            placeholderText: "enter frame number"
            focus: true
            onAccepted:AssetMaker._seekTo(Number(ts.text) * timestep)
            EnterKey.type: Qt.EnterKeyDone
            Keys.onReturnPressed: accepted()
            Keys.onEnterPressed: accepted()
        }

        Button {
            id: minF
            text: "-"
            anchors.right: fileDialog.left
            onClicked: {
                val.value -= 1
            }
        }

        Slider {
            id: val
            from: 1
            to: 350
            value: 60
            onValueChanged: {
                fps =  value
            }
        }
        Button {
            id: maxF
            text: "+"
            anchors.right: fileDialog.left
            onClicked: {
                val.value += 1
            }
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


        RhiTextureItem {
            id: videoView
            Layout.preferredWidth: 2
            Layout.fillWidth: true
            Layout.fillHeight: true
            Component.onCompleted: AssetMaker.setVideoView(videoView)

        }
        RhiTextureItem {
            id: videoView2
            Layout.preferredWidth: 1
            Layout.fillWidth: true
            Layout.fillHeight: true
            Rectangle {
                anchors.centerIn: parent
                width: 50
                height: 50
                color: "red"
            }
        }
    }

    Slider {
        anchors.top: splitPanes.bottom
        from: 0
        to: 10000
        onValueChanged: {
            pause = true
            AssetMaker._seekTo(value)
        }

    }


    Component.onCompleted: Qt.application.style = "Fusion"

}
