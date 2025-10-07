#pragma once

#include <QVariantMap>
#include <opencv2/opencv.hpp>
#include <QObject>
#include <QSize>
#include <QPoint>

#define TVAI_MAX_WIDTH 7680.0
#define TVAI_MAX_HEIGHT 4320.0

namespace videoio {
    using namespace cv;
    using namespace std;

    class Reader : public QObject {
        Q_OBJECT
    protected:
        QVariantMap _info;
        bool _isOpen, _isEOF, _looping, _deleteOnClose, _isInstant, _isImgSeq;
        QString _path;
        QSize _adjustedSize;
        double _adjustmentFactor;
        long long _rangeStartPts;
        long long _rangeEndPts;
        std::vector<QPoint> _playRanges{}; // in timestamp

        double computeAdjustedFrameSize(QSize resolution, double sar) {
            double w = resolution.width() * sar / TVAI_MAX_WIDTH;
            double h = resolution.height() / TVAI_MAX_HEIGHT;
            _adjustmentFactor = (w > 1 || h > 1) ? (w > h ? w : h) : 1;
            _adjustedSize = QSize(resolution.width() / _adjustmentFactor * sar,
                                  resolution.height() / _adjustmentFactor);
            return _adjustmentFactor;
        }

    public:
        int* _playheadTS;

        Reader(const QString path):_path(path), _isOpen(false), _isEOF(false), _looping(false), _deleteOnClose(false), _rangeStartPts(0), _rangeEndPts(0), _isInstant(false) {}
        virtual QVariantMap& getInfo() { return _info; }
        virtual bool updateInfo(const QVariantMap& info) = 0;
        virtual bool open() = 0;
        virtual bool isEOF() { return _isEOF; }
        virtual bool isOpen() { return _isOpen; }
        virtual long long currentPts() = 0;
        virtual long long currentTimestamp() = 0;
        virtual long long readFirst() = 0;
        virtual long long readLast() = 0;
        virtual bool seekTo(long long ms, bool onFilterGraphReady = false) = 0;
        virtual void nextFrame() = 0;
        virtual void prevFrame() = 0;
        virtual long long getLast(){return -1;}
        virtual Mat getFrame() = 0;
        virtual void close() {
            _isOpen = false;
            if(_deleteOnClose) {
                qInfo() << "Delete after close" << _path;
            }
        }
        virtual void setLooping(bool loop) { _looping = loop; }
        virtual bool getLooping() { return _looping; }
        virtual Mat getThumbnail(float maxWidth = 640.0f, int maxRead = 30, int startFrame = 0) = 0;
        virtual bool clearBuffers() = 0;
        virtual vector<int64_t> getFrameBufferRange() {return vector<int64_t>();}
        virtual bool canReload() = 0;
        virtual ~Reader() {}
        virtual void cacheAndClear(bool ignorePlayHead = false) {};

        virtual void setDeleteOnClose(bool value) { _deleteOnClose = value; }
        virtual void setRangeStartTimeStamp(long long startPts, long long endPts) { _rangeStartPts = startPts; _rangeEndPts = endPts; }

        QString getPath() const { return _path; }
        QString getFile() const { return _path.mid(_path.lastIndexOf("/")); }


        static bool IsImageSequence(const QString& path) {
            static const std::vector<QString> imgSeqPathEndings = {".tif", ".tiff", ".png", ".jpg", ".jpeg", ".exr", ".dpx"};
            return std::any_of(imgSeqPathEndings.begin(), imgSeqPathEndings.end(), [&path](const QString& ending) { return path.endsWith(ending, Qt::CaseInsensitive);});
        }

        virtual bool getIsInstant() { return _isInstant; }
        void clearPlayRanges(){ _playRanges.clear(); }

        void deletePlayRange(QPoint range){
            sortPlayRanges();
            auto intersects = [](QPoint a, QPoint b){return !(a.x() > b.y() || b.x() > a.y());};
            auto first = std::find_if(_playRanges.begin(), _playRanges.end(), [&](QPoint& other){
                return intersects(other, range);
            });
            std::vector<decltype(_playRanges)::iterator> removes;
            while(first != _playRanges.end() && intersects(range, *first)){
                removes.push_back(first);
                first++;
            }
            _debug_print();
            if(!removes.empty()){
                // qInfo() << "#$%@ Attempting remove";
                // if(removes.front() != _playRanges.end() && removes.back() != _playRanges.end())
                    // qInfo() << "#$%@ Removing " << (*removes.front()).x() << " to " << (*removes.back()).y();
                auto last = removes.back() == _playRanges.end() ? removes.back() : std::next(removes.back());
                _playRanges.erase(removes.front(), last);
            }
            _debug_print();
        }
        void addToPlayRanges(QPoint range) {
            qDebug() << "#$%@ Adding range: ["<< range.x() << ", " << range.y() << "]";
            _playRanges.push_back(range);
            _debug_print();
            qDebug() << "Instant Reader$ " << this << "has playranges " << _playRanges;
        }

        void _debug_print(){
            sortPlayRanges();
            // qDebug() << "Begin print all instant reader ranges #$%@";
            for(auto range : _playRanges){
                // qDebug() << "#$%@ [" << range.x() << ", " << range.y() << "] " << "[" << range.x() / 40 << ", " << range.y() / 40 << "] ";
            }
            // qDebug() << "#$%@ end";

        }

        void sortPlayRanges() {
            std::sort(_playRanges.begin(), _playRanges.end(), [](const QPoint& a, const QPoint& b) {
                return a.x() < b.x();
            });
        }

        virtual void justread(bool isPaused=  false) {};

        virtual void updatePlayhead(int* playheadTS){
            // qDebug() << "where is playhead" << *playheadTS;
            _playheadTS = playheadTS;
        }
    // public slots:
    //     void setFilter(QVariantMap filterInfo);
    signals:
        void frameBufferRangeChangedoo(vector<long long> ptsList);
        // void previewReady(const QString& filename);
        // void beginDiskCacheChanged();
        // void previewReady(const char* filename);
    };
}
