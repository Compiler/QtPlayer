#pragma once

extern "C" {
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/display.h"
#include "libavcodec/packet.h"
}

#include <opencv2/opencv.hpp>
#include "Reader.h"
#include "demux/demuxer.h"
#include <QFile>
#include <QDebug>
#include <atomic>

namespace videoio {
using namespace std;
using namespace cv;

class FFVideoReader: public Reader {

    Demuxer *demuxer;

    AVFormatContext *_pFormat;
    AVCodecContext *_pCodecContext;
    SwsContext *_pSwsContext;
    int _videoStreamIndex;
    vector<std::shared_ptr<AVFrame>> _frames;
    long long _lastShown, _startTC, _timestep, _duration, _size;
    AVRational _timebase, _framerate, _sar;
    int _maxSize, _currentIndex, _width, _height;
    bool _byteSeek, _isBlackAndWhite, _isTelecined, _overrideInputColorspace;
    int _colorPrimaries, _colorSpace, _colorTrc, _colorRange;
    unsigned int _rotate;
    long long _startIndex;
    std::atomic<bool> isReadingNext{false};

    bool readNext();
    bool seek(long long pts);
    void readTill(long long pts);
    int decodeAndAdd(AVPacket* pPacket);
    void eraseFramesTo(int size);
    bool addFrame(std::shared_ptr<AVFrame> pFrame);
    int findIndex(long long pts);
    std::shared_ptr<AVFrame> find(long long pts);
    bool isAllBlack(std::shared_ptr<AVFrame> pFrame, int threshold = 30);
    bool seekToKeyFrame(long long pts, int attempt = 0);
    bool containsFrame(long long pts) { return !_frames.empty() && pts >= _frames.front()->pts && pts <= _frames.back()->pts; }

    long long ms2tc(long long ms) { return ms/av_q2d(_timebase)/1000 + _startTC; }
    long long tc2ms(long long tc) { return (tc - _startTC)*av_q2d(_timebase)*1000; }
    void clearFrames() { eraseFramesTo(0); }
    long long clampPts(long long pts) { return pts < _startTC ? _startTC : (pts > _duration ? _duration : pts); }
    bool frameInNearFuture(long long pts, int frames = 5) { return !_frames.empty() && _frames.back()->pts < pts && (_frames.back()->pts + frames*_timestep) > pts; }
    bool isIndexValid() { return _currentIndex >= 0 && _currentIndex < _frames.size(); }

    Mat convertFrame(std::shared_ptr<AVFrame> pFrame);
    Mat convertFrameRGB(std::shared_ptr<AVFrame> pFrame);
    unsigned detectOrientation(const AVStream *pStream);
    QVariantList streamInfo(AVMediaType avType);

    void transpose()  {
        return;
        if(_rotate == ROTATE_90_CLOCKWISE || _rotate == ROTATE_90_COUNTERCLOCKWISE) {
            _info["width"] = _height;
            _info["height"] =  _width;
            _info["size"] =  QSize(_height, _width);
            if(_pCodecContext != nullptr)
                _info["resolution"] =  QSize(_pCodecContext->height, _pCodecContext->width);
            else
                _info["resolution"] =  QSize(_height, _width);
        } else {
            _info["width"] = _width;
            _info["height"] =  _height;
            _info["size"] =  QSize(_width, _height);
            if(_pCodecContext != nullptr)
                _info["resolution"] =  QSize(_pCodecContext->width, _pCodecContext->height);
            else{
                _info["resolution"] =  QSize(_height, _width);
            }
        }
        _info["adjustmentFactor"] = computeAdjustedFrameSize(_info["resolution"].toSize(), _info["sar"].toDouble());
        _info["adjustedSize"] = _adjustedSize;
    }

    void estimateDuration();

public:
    FFVideoReader(const QString path, int maxSize = 10, long long startIndex = -1)
        : Reader(path), _pFormat(nullptr), _videoStreamIndex(-1), _startIndex(startIndex), _maxSize(maxSize), _lastShown(0), _startTC(0), _timestep(0), _duration(0), _width(0), _height(0), _currentIndex(-1), _byteSeek(false), _rotate(3) {}

    virtual ~FFVideoReader() {
        close();
    }

    virtual bool updateInfo(const QVariantMap& info) override {
        bool ret = false;
        if(info.contains("rotation")) {
            _rotate = info["rotation"].toInt();
            _info["rotation"] = _rotate;
            transpose();
            ret = true;
        }
        if (info.contains("isBlackAndWhite")) {
            _isBlackAndWhite = info["isBlackAndWhite"].toBool();
            _info["isBlackAndWhite"] = _isBlackAndWhite;
            ret = true;
        }
        if (info.contains("isTelecined")) {
            _isTelecined = info["isTelecined"].toBool();
            _info["isTelecined"] = _isTelecined;
            ret = true;
        }
        if (info.contains("overrideInputColorspace")) {
            _overrideInputColorspace = info["overrideInputColorspace"].toBool();
            _info["overrideInputColorspace"] = _overrideInputColorspace;
            ret = true;
        }
        if (info.contains("colorPrimaries")) {
            _colorPrimaries = info["colorPrimaries"].toInt();
            _info["colorPrimaries"] = _colorPrimaries;
            ret = true;
        }
        if (info.contains("colorSpace")) {
            _colorSpace = info["colorSpace"].toInt();
            _info["colorSpace"] = _colorSpace;
            ret = true;
        }
        if (info.contains("colorTrc")) {
            _colorTrc = info["colorTrc"].toInt();
            _info["colorTrc"] = _colorTrc;
            ret = true;
        }
        if (info.contains("colorRange")) {
            _colorRange = info["colorRange"].toInt();
            _info["colorRange"] = _colorRange;
            ret = true;
        }

        return ret;
    }

    std::shared_ptr<AVFrame> getCurrentFrame() { return isIndexValid() ? _frames[_currentIndex] : nullptr; }
    unsigned int getCurrentFrameIndex() { return _currentIndex; }

    virtual bool open() override;

    virtual bool isEOF() override {
        return _lastShown >= _duration || Reader::isEOF();
    }

    long long currentPts() override {
        std::shared_ptr<AVFrame> pFrame = getCurrentFrame();
        return pFrame == nullptr || pFrame->pts < 0 ? -1 : pFrame->pts;
    }

    long long currentTimestamp() override { return tc2ms(currentPts()); }

    virtual long long readFirst() override;

    virtual long long readLast() override;

    virtual bool seekTo(long long timestamp, bool onFilterGraphReady = false) override {
        // std::string mySeekTo = std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())).substr(0, 5) + " FFVR::seekTo(" + std::to_string(timestamp) + ")";
        // auto now = std::chrono::high_resolution_clock::now();
        // long long now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        // __TL_threadLogger.push({now_ms, mySeekTo});
        auto result = seek(ms2tc(timestamp));
        // mySeekTo = std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())).substr(0, 5) + " FFVR::seekTo(" + ") END";
        // now = std::chrono::high_resolution_clock::now();
        // now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        // __TL_threadLogger.push({now_ms, mySeekTo});
        return result;
    }

    virtual void nextFrame() override {
        if (isIndexValid() && !isEOF()) {
            if (_currentIndex == _frames.size() - 1) {
                readNext();
                _currentIndex = _frames.size() - 1;
            } else
                _currentIndex++;
        } else {
            if (_looping)
                _currentIndex = 0;
        }
    }

    virtual void prevFrame() override {
        if (isIndexValid()) {
            if (_currentIndex == 0)
                seek(currentPts() - _timestep);
            else
                _currentIndex--;
        } else {
            if (_looping)
                _currentIndex = _frames.size() - 1;
        }
    }

    virtual Mat getFrame() override {
        Mat frame;
        std::shared_ptr<AVFrame> pFrame = getCurrentFrame();
        if (pFrame != nullptr) {
            _lastShown = pFrame->pts;
            frame = convertFrame(pFrame);
        }
        return frame;
    }

    virtual bool canReload() override { return false; }

    void close() override;

    Mat getThumbnail(float maxWidth = 640.0f, int maxRead = 30, int startFrame = 0) override;

    virtual bool clearBuffers() override {
        clearFrames();
        if(_pCodecContext != nullptr)
            avcodec_flush_buffers(_pCodecContext);
        return true;
    }
};
}

