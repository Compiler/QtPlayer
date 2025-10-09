#include "ffvideoreader.h"
//#include "FFReaderUtils.h"
#include <iostream>
#include <mutex>

namespace videoio {
using namespace cv;
using namespace std;
bool FFVideoReader::seek(long long pts) {
    if(!_isOpen)
        return false;
    auto clampedPts = clampPts(pts);
    bool retVal = (pts == clampedPts);
    if(findIndex(pts) >= 0) {
        _currentIndex = findIndex(pts);
        _lastShown = currentPts();
        return retVal;
    }
    if(!frameInNearFuture(pts, 10)) {
        seekToKeyFrame(pts);
        long long cpts = 0, lastPts = -1;
        for(int i=1;i<=5;i++) {
            if(readNext()) {
                //qInfo() << "->->-> Was seeking" << tc2ms(pts) << "found" << tc2ms(_frames.front()->pts) << _frames.front()->key_frame;
                long long keyPts = _frames.size() == 0 ? -1 : _frames.front()->pts;
                if(_frames.size() == 0 || ((_frames.front()->pts > pts || !(_frames.front()->flags & AV_FRAME_FLAG_KEY) || _frames.front()->pts + _timestep*100 < pts) && lastPts != _frames.front()->pts )) {
                    if(_byteSeek && _frames.front()->pts + _timestep*100 < pts) {
                        if(cpts == 0)
                            cpts = pts;
                        cpts = cpts+(cpts - _frames.front()->pts)*0.9*i;
                        cpts = cpts < _duration ? cpts : _duration-_timestep*i;
                        seekToKeyFrame(cpts, i);
                    } else
                        seekToKeyFrame(pts, i);
                } else
                    break;
                lastPts = keyPts;
            } else {
                qCritical() << "Reading failed after seek";
                return false;
            }
        }
    }
    readTill(pts);

    _currentIndex = findIndex(pts);
    if(_currentIndex < 0 && !_frames.empty()) {
        _currentIndex = 0;
        retVal = false;
    }
    _lastShown = currentPts();
    return retVal;
}

void log_callback_report(void *ptr, int level, const char *fmt, va_list vl)	{
    if(level > AV_LOG_INFO)
        return;
    static mutex m;
    lock_guard<mutex> lock(m);
    char line[4096];
    vsprintf(line, fmt, vl);
    cerr << line;
}

bool FFVideoReader::open() {
    demuxer = new Demuxer();
    demuxer->demux_open_filename(_path.toStdString());
    demuxer->seek(0);
    demuxer->decode_next_video_frame();

    return true;

    qInfo() << "Trying to open the file " << _path << isOpen();
    _isOpen = false;
    auto path = _path.toStdString();
    AVDictionary* options = NULL;
    if (_startIndex >= 0) {
        char st[128];
        sprintf(st, "%lld", _startIndex);
        av_dict_set(&options, "start_number", st, 0);
    }
    qDebug() << "Open Called for file" << _path << _startIndex;
    int ret = avformat_open_input(&_pFormat, path.c_str(), nullptr, &options);
    if(ret) {
        qCritical() << "Unable to open file" << _path;
        return false;
    }
    ret = avformat_find_stream_info(_pFormat, nullptr);
    if(ret) {
        qCritical() << "Unable to find stream information in file" << _path;
        return false;
    }
    _videoStreamIndex = av_find_best_stream(_pFormat, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if(_videoStreamIndex < 0) {
        qCritical() << "Unable to find video stream information in file" << _path;
        return false;
    }
    const AVStream *pStream = _pFormat->streams[_videoStreamIndex];
    qDebug() << "CodecID:" << pStream->codecpar->codec_id;
    const AVCodec* pCodec = avcodec_find_decoder(pStream->codecpar->codec_id);
    if(pCodec == nullptr) {
        qCritical() << "Unable to decode video stream information in file" << _path ;
        return false;
    }
    _pCodecContext = avcodec_alloc_context3(pCodec);
    ret = avcodec_parameters_to_context(_pCodecContext, pStream->codecpar);
    if(ret < 0) {
        qCritical() << "Unable to copy decoded video stream info with index" << _videoStreamIndex << "from file" << _path ;
        avcodec_free_context(&_pCodecContext);
        return false;
    }
    ret = avcodec_open2(_pCodecContext, pCodec, NULL);
    if (ret < 0) {
        qCritical() << "Failed to open codec through avcodec_open2" << _videoStreamIndex << "from file" << _path ;
        return false;
    }
    _startTC = pStream->start_time == AV_NOPTS_VALUE ? 0 : pStream->start_time;
    _byteSeek = (!(_pFormat->iformat->flags & AVFMT_NO_BYTE_SEEK) && !!(_pFormat->iformat->flags & AVFMT_TS_DISCONT) && strcmp("ogg", _pFormat->iformat->name));

    //            if(_pFormat->start_time != AV_NOPTS_VALUE)
    //                _startTC += _pFormat->start_time;
    if(_startTC < 0)
        _startTC = 0;
    _duration = pStream->duration;
    _timebase = pStream->time_base;
    _framerate = pStream->r_frame_rate;
    _sar = pStream->sample_aspect_ratio;
    if(_sar.num == 0) { _sar = _pCodecContext->sample_aspect_ratio; }
    if(_sar.num == 0) { _sar.den = _sar.num = 1; }
    _size = avio_size(_pFormat->pb);
    if(_duration <= 0) {
        _duration = _pFormat->duration*1.0/AV_TIME_BASE/av_q2d(_timebase);
    }
    _timestep = av_q2d(av_inv_q(av_mul_q(_timebase, _framerate)));
    qDebug() << "DURATION" << _duration << _pFormat->duration << _pFormat->duration*1.0/AV_TIME_BASE << av_q2d(_timebase)*_duration << _pFormat->duration*1.0/AV_TIME_BASE/av_q2d(_timebase) << _byteSeek;
    qDebug() << "TIMESTEP" << _timestep << av_q2d(_framerate) << _framerate.num << _framerate.den << av_q2d(_timebase) << _timebase.num << _timebase.den;
    _width = _pCodecContext->width*av_q2d(_sar);
    _height = _pCodecContext->height;
    if(_pCodecContext->pix_fmt == AV_PIX_FMT_NONE)
        _pCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    _rotate = detectOrientation(pStream);
    qInfo() << "===--- Rotate on open: " << _rotate;
    _info["rotation"] = _rotate;
    _info["originalRotation"] = _rotate;
    _info["path"] = _path;
    _info["length"] = _duration;
    _info["step"] = _timestep;

    _info["originalColorPrimaries"] = _pCodecContext->color_primaries;
    _info["originalColorSpace"] = _pCodecContext->colorspace;
    _info["originalColorTrc"] = _pCodecContext->color_trc;
    _info["originalColorRange"] = _pCodecContext->color_range;

    _info["framerate"] = av_q2d(pStream->r_frame_rate);

    float tbr = av_q2d(pStream->r_frame_rate);
    float fps = av_q2d(pStream->avg_frame_rate);
    float codecFps = av_q2d(pStream->codecpar->framerate);
    // cv::VideoCapture video(_path.toStdString());
    // double opencvFPS = video.get(cv::CAP_PROP_FPS);
    if(isnan(tbr)) return false; // couldn't open reliably.

    // priority for fps read: avstream->avg_fps > codecpar->framerate > opencvFPS
    _info["fps"] = isnan(fps) ? (codecFps == 0.0 ?  tbr : codecFps) : fps;
    if(pStream->codecpar->codec_id == 24) {
        _info["fps"] = tbr;
    }

    _info["sar"] = av_q2d(_sar);
    _info["video"] = streamInfo(AVMEDIA_TYPE_VIDEO);
    _info["audio"] = streamInfo(AVMEDIA_TYPE_AUDIO);
    _info["subtitle"] = streamInfo(AVMEDIA_TYPE_SUBTITLE);
    _info["originalSize"] =  QSize(_width, _height);
    // We should call transpose here?
    if(_rotate == ROTATE_90_CLOCKWISE || _rotate == ROTATE_90_COUNTERCLOCKWISE) {
        _info["width"] = _height;
        _info["height"] =  _width;
        _info["size"] =  QSize(_height, _width);
        _info["resolution"] =  QSize(_pCodecContext->height, _pCodecContext->width);
    } else {
        _info["width"] = _width;
        _info["height"] =  _height;
        _info["size"] =  QSize(_width, _height);
        _info["resolution"] =  QSize(_pCodecContext->width, _pCodecContext->height);
    }
    _info["timebase"] = av_q2d(_timebase);
    _info["timestep"] = _timestep*av_q2d(_timebase)*1000;
    _info["pixelFormat"] = av_get_pix_fmt_name(_pCodecContext->pix_fmt);
    _info["isBlackAndWhite"] = _pCodecContext->pix_fmt == AV_PIX_FMT_GRAY8;
    _info["isTelecined"] = false; // can we detect this (or a separate isDeinterlaced) later?
    _pSwsContext = sws_getContext(_pCodecContext->width, _pCodecContext->height,_pCodecContext->pix_fmt, _width, _height, AV_PIX_FMT_RGBA64LE, SWS_BICUBIC, NULL,NULL,NULL);
    if(_pSwsContext == nullptr) {
        qCritical() << "Unable to setup conversion context";
        avcodec_free_context(&_pCodecContext);
        return false;
    }
    av_log_set_callback(log_callback_report);
    av_dump_format(_pFormat, 0, path.c_str(), 0);
    _isOpen = true;
    _isEOF = false;
    if(_startIndex < 0) {
        _duration += _startTC;
        qInfo() << "Determining start and end" << _startTC << _duration;
        if (_duration < 0) {
            estimateDuration();
        }
        _startTC = readFirst();
        qInfo() << "Determined start and end" << _startTC << _duration;
    }
    _info["adjustmentFactor"] = computeAdjustedFrameSize(_info["resolution"].toSize(), _info["sar"].toDouble());
    _info["adjustedSize"] = _adjustedSize;
    _info["start"] = _startTC;  // Presentation timestamp (PTS) of first frame in stream in stream time base.
    _info["startTimeMs"] = tc2ms(_startTC); // PTS of first frame in ms --- I think this is always 0? becase tc2ms subtracts _startTC.
    _info["duration"] = tc2ms(_duration);   // Duration in ms.
    qInfo() << "Successfully opened " << _path;
    return true;
}

void FFVideoReader::estimateDuration() {
    _duration = 1e10;
    _duration = readLast();
    _info["length"] = _duration;
    _info["duration"] = tc2ms(_duration);
}

QVariantList FFVideoReader::streamInfo(AVMediaType avType) {
    QVariantList streamList;
    unsigned i = 0, n = _pFormat->nb_streams, count = 0;
    for (i = 0; i < n; i++) {
        AVStream *pStream = _pFormat->streams[i];

        if(pStream->codecpar->codec_type != avType)
            continue;

        QVariantMap map;
        map["id"] = pStream->id;
        map["startTime"] = static_cast<long long>(pStream->start_time);
        map["duration"] = static_cast<long long>(pStream->duration);
        map["numFrames"] = static_cast<long long>(pStream->nb_frames);

        // There is a lot more info we could include here, attempted to limit map to what may be relevant.
        // See: https://ffmpeg.org/doxygen/3.1/structAVStream.html

        // codec info...
        map["codecId"] = pStream->codecpar->codec_id;
        map["bitrate"] = static_cast<long long>(pStream->codecpar->bit_rate);
        map["profile"] = pStream->codecpar->profile;

        if (avType == AVMEDIA_TYPE_AUDIO) {
            map["channels"] = pStream->codecpar->ch_layout.nb_channels;
        }

        streamList.append(map);
    }
    return streamList;
}

unsigned FFVideoReader::detectOrientation(const AVStream *pStream) {
    const AVCodecParameters *par = pStream->codecpar;
    const AVPacketSideData *sd = av_packet_side_data_get(par->coded_side_data, par->nb_coded_side_data, AV_PKT_DATA_DISPLAYMATRIX);
    // This side data contains a 3x3 transformation matrix describing an affine transformation that needs to be applied to
    // the decoded video frames for correct presentation.
    if (sd && sd->size >= 9 * sizeof(int32_t)) {
        auto displaymatrix = reinterpret_cast<int32_t *>(sd->data);
        double theta = 0;
        if (displaymatrix)
            theta = -round(av_display_rotation_get((int32_t*) displaymatrix));
        theta -= 360*floor(theta/360 + 0.9/360);
        if (fabs(theta - 90) < 1.0)
            return cv::ROTATE_90_CLOCKWISE;
        if (fabs(theta - 180) < 1.0)
            return cv::ROTATE_180;
        if (fabs(theta - 270) < 1.0)
            return cv::ROTATE_90_COUNTERCLOCKWISE;
    }
    return 3;
}

void FFVideoReader::close() {

    delete demuxer;

    return;
    qInfo() << "Trying to close the file" << isReadingNext << _path << isOpen();
    if (isOpen() && !isReadingNext) {
        qInfo() << "Closing the file" << _path;
        clearFrames();
        avformat_close_input(&_pFormat);
        avformat_free_context(_pFormat);
        avcodec_free_context(&_pCodecContext);
        sws_freeContext(_pSwsContext);
        _pCodecContext = nullptr;
        _pFormat = nullptr;
        Reader::close();
    }
}

bool FFVideoReader::seekToKeyFrame(long long pts, int attempt) {
    clearFrames();
    if (_byteSeek) {
        long long location = (pts-_timestep*attempt*5-_startTC)*_size/_duration;
        if(avformat_seek_file(_pFormat, -1, INT64_MIN, location, INT64_MAX, AVSEEK_FLAG_BYTE) < 0) {
            qCritical() << "Seek failed to location" << pts << "on stream" << _videoStreamIndex;
            return false;
        }
    } else {
        if(av_seek_frame(_pFormat, _videoStreamIndex, pts - (_timestep*attempt*2), AVSEEK_FLAG_BACKWARD) < 0) {
            qCritical() << "Seek failed to location" << pts << "on stream" << _videoStreamIndex;
            return false;
        }
    }
    avcodec_flush_buffers(_pCodecContext);
    return true;
}

Mat FFVideoReader::convertFrame(std::shared_ptr<AVFrame> pFrame) {
    if (pFrame != nullptr && pFrame->height > 0 && pFrame->width > 0) {
        Mat frame(pFrame->height, pFrame->width, CV_16UC4);
        int step = frame.step;
        if (!_pSwsContext) {
            _pSwsContext = sws_getContext(pFrame->width, pFrame->height, (AVPixelFormat)pFrame->format, pFrame->width, pFrame->height, AV_PIX_FMT_RGBA64LE, SWS_BICUBIC, NULL,NULL,NULL);
        }
        sws_scale(_pSwsContext, pFrame->data, pFrame->linesize, 0, pFrame->height, &frame.data, &step);

        if (_rotate < 3) {
            Mat rframe;
            cv::rotate(frame, rframe, _rotate);
            return rframe;
        }
        return frame;
    }
    return Mat();
}

Mat FFVideoReader::convertFrameRGB(std::shared_ptr<AVFrame> pFrame) {
    Mat frame = convertFrame(pFrame), temp;
    frame.convertTo(temp, CV_8UC4, 1/256.0f);
    cvtColor(temp, frame, COLOR_RGBA2BGR);
    return frame;
}

void FFVideoReader::readTill(long long pts) {
    while (_frames.empty() || (!containsFrame(pts) && _frames.back()->pts < pts && _frames.front()->pts < pts)) {
        if (!readNext())
            break;
    }
}

int FFVideoReader::decodeAndAdd(AVPacket* pPacket) {
    int count = 0;
    int response = avcodec_send_packet(_pCodecContext, pPacket);
    if (response < 0 && pPacket != nullptr) {
        char errorMsg[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(errorMsg, AV_ERROR_MAX_STRING_SIZE, response);
        qCritical() << "Error sending packet to decode" << errorMsg;
        return response;
    }
    while(response >= 0) {
        AVFrame* pFrame = av_frame_alloc();
        response = avcodec_receive_frame(_pCodecContext, pFrame);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            av_frame_free(&pFrame);
            return count;
        } else if(response < 0) {
            char errorMsg[AV_ERROR_MAX_STRING_SIZE];
            av_make_error_string(errorMsg, AV_ERROR_MAX_STRING_SIZE, response);
            qCritical() << "Error decoding packet" << errorMsg;
            av_frame_free(&pFrame);
            return response;
        }
        pFrame->pts = pFrame->best_effort_timestamp;
        // qCritical() << "FF pFrame: w: " << pFrame->width << pFrame->pts << pFrame->time_base.num;
        if (addFrame(std::shared_ptr<AVFrame>(pFrame, [](AVFrame* ptr) { av_frame_free(&ptr); }))) {
            count++;
        }
    }
    return count;
}

void FFVideoReader::eraseFramesTo(int size) {
    while (_frames.size() > size) {
        _frames.erase(_frames.begin());
    }
}

bool FFVideoReader::addFrame(std::shared_ptr<AVFrame> pFrame) {
    if (_frames.empty() || pFrame->pts != _frames.back()->pts) {
        _frames.push_back(pFrame);
        eraseFramesTo(_maxSize);
        return true;
    }
    return false;
}

int FFVideoReader::findIndex(long long pts) {
    for (int i = 0; i<_frames.size(); i++) {
        if (pts <= (_frames[i]->pts + _timestep/2.0f) && pts > (_frames[i]->pts - _timestep/2.0f)) {
            return i;
        }
    }
    return -1;
}

std::shared_ptr<AVFrame> FFVideoReader::find(long long pts) {
    auto index = findIndex(pts);
    return index >= 0 && index < _frames.size() ? _frames[index] : nullptr;
}

bool FFVideoReader::readNext() {
    demuxer->decode_next_video_frame();

    return true;
    // std::string mySeekTo = std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())).substr(0, 5) + " FFVR::readNext(" + ")";
    // auto now = std::chrono::high_resolution_clock::now();
    // long long now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    // __TL_threadLogger.push({now_ms, mySeekTo});
    // console.debug("^_()_^", __TL_threadLogger);
    isReadingNext = true;
    AVPacket *pPacket = av_packet_alloc();
    while (av_read_frame(_pFormat, pPacket) >= 0) {
        if (pPacket->stream_index == _videoStreamIndex) {
            int count = decodeAndAdd(pPacket);
            if (count != 0) {
                av_packet_free(&pPacket);
                _isEOF = false;
                isReadingNext = false;
                return count > 0;
            }
        }
        av_packet_unref(pPacket);
    }
    av_packet_free(&pPacket);
    int count = decodeAndAdd(nullptr);
    if(count <= 0)
        _isEOF = true;

    // mySeekTo = std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())).substr(0, 5) + " FFVR::readNext(" + ") END";
    // now = std::chrono::high_resolution_clock::now();
    // now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    // __TL_threadLogger.push({now_ms, mySeekTo});
    // console.debug("^_()_^", __TL_threadLogger);
    isReadingNext = false;
    return count > 0;
}

long long FFVideoReader::readFirst() {
    long long step = _startTC;
    int n = 10;
    while((!seekToKeyFrame(step, 0) || !readNext()) && n > 0) {
        step+=_timestep;
        n--;
    }
    _currentIndex = 0;
    return currentPts();
}

long long FFVideoReader::readLast() {
    long long ts = _duration;
    seek(ts);
    int n = 10;
    while(_frames.empty() && n > 0) {
        ts -= _timestep;
        seek(ts);
        n--;
    }
    if(_frames.empty()) {
        seekToKeyFrame(ts - _startTC,0);
        readNext();
    }
    n = 0;
    while (readNext() && n < 100) n++;
    _currentIndex = _frames.size() - 1;
    return currentPts();
}

bool FFVideoReader::isAllBlack(std::shared_ptr<AVFrame> pFrame, int threshold) {
    Mat gframe;
    cvtColor(convertFrameRGB(pFrame), gframe, cv::COLOR_BGR2GRAY);
    cv::Scalar tempVal = cv::mean( gframe );
    return tempVal.val[0] <= threshold;
}

Mat FFVideoReader::getThumbnail(float width, int maxRead, int startFrame) {
    if(!isOpen()) return Mat();

    auto startFrameTC = ms2tc(startFrame);
    if (startFrameTC <= 0 || _duration <= startFrameTC) {
        readFirst();
        int i = 0;
        while(i < maxRead && (_frames.empty() || isAllBlack(_frames.back()))) {
            readNext();
            i++;
        }
    } else {
        seekTo(startFrame);
        readNext();
    }

    if(_frames.empty()) return Mat();
    Mat thumbnail;
    Mat frame = convertFrameRGB(_frames.back());
    double height = (width / frame.cols) * frame.rows;
    resize(frame, thumbnail, Size(width, height), INTER_LINEAR);
    return thumbnail;
}
}
