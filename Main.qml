import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Window {
    id: window
    width: 1080
    height: 720
    visible: true
    title: qsTr("QtPlayer")

    readonly property string pathToBuffer: "file:///" + AssetsDir + "/buffer.tiff"
    readonly property real fps: 10
    readonly property real timestep: 1000 / fps

    Timer {
        interval: timestep
        repeat: true
        running:true

        onTriggered: {
            AssetMaker.writeBuffer()
            img2.source = ""
            img2.source = pathToBuffer
        }
    }
    Button {
        text: "Cycle & Write"
        onClicked: {
            AssetMaker.writeBuffer()
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
            Component.onCompleted: {
                console.log("Resolved:", Qt.resolvedUrl("Assets/256x256_test.png"))
            }
        }
    }



}
