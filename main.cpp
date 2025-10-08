#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSGRendererInterface>

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

#include "ffvideoreader.h"

QString sourceDirPath() {
    QFileInfo fi(QString::fromUtf8(__FILE__));
    return fi.absolutePath();
}

class AssetMaker : public QObject { Q_OBJECT
public:

    std::unique_ptr<videoio::FFVideoReader> _reader;
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

    Q_INVOKABLE void openAndWrite(QString file) {
        _reader = std::make_unique<videoio::FFVideoReader>(file);
        _reader->open();
        auto mat = _reader->getFrame();
        const QString dir = sourceDirPath() + "/Assets";
        cv::imwrite((dir + "/buffer.tiff").toStdString(), mat);
    }

    Q_INVOKABLE void readAndWriteNext() {
        if(!_reader) return;
        std::cout << __func__ << std::endl;
        //_reader->seekTo(5000);
        auto now = std::chrono::high_resolution_clock::now();
        _reader->nextFrame();
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration<double, std::milli>(end - now).count();
        qInfo().nospace() << "main: nextframe() took " << ms << " ms";
        std::cout << _reader->getCurrentFrame()->best_effort_timestamp << std::endl;
        auto mat = _reader->getFrame();
        const QString dir = sourceDirPath() + "/Assets";
        cv::imwrite((dir + "/buffer.tiff").toStdString(), mat);
    }

    Q_INVOKABLE void seekTo(float seekToMs) {
        if(!_reader) return;
        std::cout << __func__ << std::endl;
        //_reader->seekTo(5000);
        auto now = std::chrono::high_resolution_clock::now();
        _reader->seekTo(seekToMs);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration<double, std::milli>(end - now).count();
        qInfo().nospace() << "main: seekTo() took " << ms << " ms";
        std::cout << _reader->getCurrentFrame()->best_effort_timestamp << std::endl;
        auto mat = _reader->getFrame();
        const QString dir = sourceDirPath() + "/Assets";
        cv::imwrite((dir + "/buffer.tiff").toStdString(), mat);
    }
};



#include "rhitextureitem.h"
int main(int argc, char *argv[]) {
    std::cout << "App dir path: " << sourceDirPath().toStdString() << std::endl;
    AssetMaker maker;
    maker.writeBuffer();

    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("AssetMaker", &maker);
    engine.rootContext()->setContextProperty("AssetsDir", QString::fromUtf8(sourceDirPath().toStdString()) + "/Assets");
    auto *root = engine.rootObjects().isEmpty() ? nullptr : engine.rootObjects().first();
    auto *rhiItem = root ? root->findChild<ExampleRhiItem*>("videoView") : nullptr;
    Q_ASSERT(rhiItem);
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
