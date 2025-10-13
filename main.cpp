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

#include <thread>

class AssetMaker : public QObject { Q_OBJECT
private:

    std::thread _runner;
    std::queue<int> reqs;
    std::mutex _lock;
    std::condition_variable _cv;
    std::atomic_bool _stop{false};

    long long ts;
    std::string file;

public:

    AssetMaker() {
        _runner = std::thread(&AssetMaker::handleReq, this);
    }

    ~AssetMaker() {
        {
            std::lock_guard<std::mutex> g(_lock);
            _stop = true;
        }
        _cv.notify_all();
        if (_runner.joinable()) _runner.join();
    }

    Q_INVOKABLE void _writeBuffer() {
        {
            std::lock_guard<std::mutex> g(_lock);
            reqs.push(1);
        }
        _cv.notify_one();
    }

    Q_INVOKABLE void _openAndWrite(QString f) {
        {
            std::lock_guard<std::mutex> g(_lock);
            file = f.toStdString();
            reqs.push(2);
        }
        _cv.notify_one();
    }

    Q_INVOKABLE void _seekTo(long long t) {
        {
            std::lock_guard<std::mutex> g(_lock);
            ts = t;
            reqs.push(3);
        }
        _cv.notify_one();
    }

    Q_INVOKABLE void _readAndWriteNext() {
        {
            std::lock_guard<std::mutex> g(_lock);
            reqs.push(4);
        }
        _cv.notify_one();
    }
    void handleReq() {
        std::unique_lock<std::mutex> l(_lock);
        for (;;) {
            _cv.wait(l, [this]{ return _stop || !reqs.empty(); });
            if (_stop && reqs.empty()) break;

            int code = reqs.front();
            reqs.pop();

            l.unlock();
            switch(code) {
            case 1: writeBuffer(); break;
            case 2: openAndWrite(QString::fromStdString(file)); break;
            case 3: seekTo(static_cast<float>(ts)); break;
            case 4: readAndWriteNext(); break;
            default: break;
            }
            l.lock();
        }
    }

    std::unique_ptr<videoio::FFVideoReader> _reader;
    Q_INVOKABLE void writeBuffer() {
        std::unique_lock<std::mutex> l(_lock);
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
        std::unique_lock<std::mutex> l(_lock);
        if(file.contains("file:///"))
            file = file.replace("file:///", "");
        _reader = std::make_unique<videoio::FFVideoReader>(file);
        _reader->open();
        auto mat = _reader->getFrame();
        const QString dir = sourceDirPath() + "/Assets";
        //cv::imwrite((dir + "/buffer.tiff").toStdString(), mat);
        pushMat(mat);
    }

    Q_INVOKABLE void readAndWriteNext() {
        std::unique_lock<std::mutex> l(_lock);
        if(!_reader) return;
        std::cout << __func__ << std::endl;
        //_reader->seekTo(5000);
        auto now = std::chrono::high_resolution_clock::now();
        _reader->nextFrame();
        if(_reader->isEOF()) _reader->seekTo(0);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration<double, std::milli>(end - now).count();
        qInfo().nospace() << "main: nextframe() took " << ms << " ms";
        std::cout << _reader->getCurrentFrame()->best_effort_timestamp << std::endl;
        auto mat = _reader->getFrame();
        const QString dir = sourceDirPath() + "/Assets";
        pushMat(mat);
        //cv::imwrite((dir + "/buffer.tiff").toStdString(), mat);
    }

    Q_INVOKABLE void seekTo(float seekToMs) {
        std::unique_lock<std::mutex> l(_lock);
        if(!_reader) return;
        if (!_view) { qWarning() << "AssetMaker: no videoView set"; return; }
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
        pushMat(mat);
        //cv::imwrite((dir + "/buffer.tiff").toStdString(), mat);
    }


    Q_INVOKABLE void setVideoView(QObject* obj) {
        _view = obj;
    }
    QObject* _view;


    Q_INVOKABLE void pushMat(const cv::Mat& mat) {
        if (!_view) { qWarning() << "AssetMaker: no videoView set"; return; }
        cv::Mat rgba;
        switch (mat.type()) {
        case CV_8UC3:
            cv::cvtColor(mat, rgba, cv::COLOR_BGR2RGBA);
            break;
        case CV_8UC4:
            cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
            break;
        case CV_16UC4: {
            cv::Mat tmp8;
            mat.convertTo(tmp8, CV_8UC4, 1.0/256.0);
            rgba = std::move(tmp8);
            break;
        }
        default:
            // best effort fallback: try downconvert with scale if depth>8
            double alpha = mat.depth() > CV_8U ? 1.0 / ((1 << (CV_MAT_DEPTH(mat.type())==CV_16U?16:8)) / 256.0) : 1.0;
            mat.convertTo(rgba, CV_8UC4, alpha);
            break;
        }
        QByteArray bytes(reinterpret_cast<const char*>(rgba.data),
                         int(rgba.total() * rgba.elemSize()));

        const bool ok = QMetaObject::invokeMethod(
            _view, "setFrameRGBA8",
            Qt::QueuedConnection,
            Q_ARG(QByteArray, bytes),
            Q_ARG(int, rgba.cols),
            Q_ARG(int, rgba.rows)
            );
        if (!ok) qWarning() << "AssetMaker: invoke setFrameRGBA8 failed (method missing?)";

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
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated,
        &app,
        [&](QObject *obj, const QUrl &) {
            auto *win = qobject_cast<QQuickWindow*>(obj);
            if (!win) return;
            if (QObject *rhiItem = win->findChild<QObject*>("videoView")) {
                maker.setVideoView(rhiItem);
                QObject::connect(rhiItem, &QObject::destroyed, &maker, [&]{
                    maker.setVideoView(nullptr);
                });
            } else {
                qWarning() << "videoView not found";
            }
        },
    Qt::QueuedConnection);
    engine.loadFromModule("QtPlayer", "Main");
    if (engine.rootObjects().isEmpty()) return -1;

    return app.exec();
}

#include "main.moc"
