import QtQuick
import QtQuick.Layouts

Window {
    id: window
    width: 1080
    height: 720
    visible: true
    title: qsTr("QtPlayer")


    RowLayout {
        id: splitPanes

        anchors.margins: 64
        anchors.fill: parent


        Image {
            id: img2
            Layout.fillWidth: true
            Layout.fillHeight: true
            source: "qrc:/qt/qml/QtPlayer/Assets/256x256_test.png"
            Component.onCompleted: {
                console.log("Resolved:", Qt.resolvedUrl("Assets/256x256_test.png"))
            }
            onStatusChanged: {
                console.log("img1 status:", status, "error:", errorString)
            }
        }

        Image {
            id: img1
            Layout.fillWidth: true
            Layout.fillHeight: true
            source: "qrc:/qt/qml/QtPlayer/Assets/256x256_test.png"
        }


    }



}
