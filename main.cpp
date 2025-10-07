#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include <QString>
#include <QFileInfo>
#include <QUrl>
#include <QObject>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include <opencv2/opencv.hpp>
#include <filesystem>

QString sourceDirPath() {
    QFileInfo fi(QString::fromUtf8(__FILE__));
    return fi.absolutePath();
}

class AssetMaker : public QObject { Q_OBJECT
public:
    Q_INVOKABLE void writeBuffer() {
        static int count = 0;
        int pattern = count % 3;
        count++;

        const int width = 1080, height = 720;
        const QString dir = sourceDirPath() + "/Assets";

        cv::Mat img(height, width, CV_8UC3);
        int w3 = width / 3;
        cv::Range rows(0, height);
        cv::Range c1(0, w3), c2(w3, 2*w3), c3(2*w3, width);

        cv::Scalar R(0,0,255), G(0,255,0), B(255,0,0);
        cv::Scalar s1, s2, s3;
        if (pattern == 0) { s1 = R; s2 = G; s3 = B; }
        else if (pattern == 1) { s1 = G; s2 = B; s3 = R; }
        else { s1 = B; s2 = R; s3 = G; }

        img(rows, c1).setTo(s1);
        img(rows, c2).setTo(s2);
        img(rows, c3).setTo(s3);

        cv::imwrite((dir + "/buffer.tiff").toStdString(), img);
    }
};





int main(int argc, char *argv[]) {
    std::cout << "App dir path: " << sourceDirPath().toStdString() << std::endl;
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
    AssetMaker maker;
    maker.writeBuffer();

    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("AssetMaker", &maker);
    engine.rootContext()->setContextProperty("AssetsDir", QString::fromUtf8(sourceDirPath().toStdString()) + "/Assets");
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("QtPlayer", "Main");

    return app.exec();
}

#include "main.moc"
