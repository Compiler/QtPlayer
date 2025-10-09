#include "demuxer.h"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <algorithm>

extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/log.h>
#include <libavcodec/avcodec.h>
}

// DemuxPacket implementation
DemuxPacket::DemuxPacket() 
    : pts(-1e40)
    , dts(-1e40)
    , duration(-1)
    , pos(-1)
    , buffer(nullptr)
    , len(0)
    , stream(-1)
    , keyframe(false)
{
}

DemuxPacket::~DemuxPacket() {
    if (buffer) {
        free(buffer);
        buffer = nullptr;
    }
}

// Static callback implementations
int Demuxer::mp_read(void *opaque, uint8_t *buf, int size) {
    Demuxer *demuxer = static_cast<Demuxer*>(opaque);
    if (!demuxer->stream) {
        return AVERROR_EOF;
    }
    
    int r = demuxer->stream->fillBuffer(buf, size);
    return (r <= 0) ? AVERROR_EOF : r;
}

int64_t Demuxer::mp_seek(void *opaque, int64_t offset, int whence) {
    Demuxer *demuxer = static_cast<Demuxer*>(opaque);
    if (!demuxer->stream) {
        return -1;
    }
    
    switch (whence) {
        case SEEK_SET:
            if (!demuxer->stream->seek(offset)) {
                return -1;
            }
            return demuxer->stream->tell();
            
        case SEEK_CUR:
            if (!demuxer->stream->seek(demuxer->stream->tell() + offset)) {
                return -1;
            }
            return demuxer->stream->tell();
            
        case SEEK_END: {
            int64_t size = demuxer->stream->getSize();
            if (size < 0) {
                return -1;
            }
            if (!demuxer->stream->seek(size + offset)) {
                return -1;
            }
            return demuxer->stream->tell();
        }
        
        case AVSEEK_SIZE:
            return demuxer->stream->getSize();
            
        default:
            return -1;
    }
}

int64_t Demuxer::mp_read_seek(void *opaque, int stream_index, int64_t timestamp, int flags) {
    // Optional: implement stream-level seeking if needed
    return -1; // Not implemented for now
}

// Demuxer implementation
Demuxer::Demuxer() 
    : avfc(nullptr)
    , pb(nullptr)
    , is_open(false)
    , stream(nullptr)
    , buffersize(32768)      // Default 32KB buffer
    , probesize(0)           // 0 = use FFmpeg default
    , analyzeduration(0.0)   // 0 = use FFmpeg default
    , seekable(false)
    , start_time(0.0)
    , duration(-1.0)
{
    av_log_set_level(AV_LOG_INFO);
}

Demuxer::~Demuxer() {
    close();
}

bool Demuxer::demux_open_filename(const std::string &fname) {
    // Step 1: Create stream
    stream = Stream::create(fname);
    if (!stream) {
        std::cerr << "Failed to create stream for: " << fname << std::endl;
        return false;
    }
    
    filename = fname;
    seekable = stream->seekable;
    
    // Step 2: Call demux_open to set up AVFormatContext
    if (!demux_open()) {
        std::cerr << "demux_open failed for: " << fname << std::endl;
        if (stream) {
            delete stream;
            stream = nullptr;
        }
        return false;
    }
    
    return true;
}

bool Demuxer::demux_open() {
    if (!stream) {
        std::cerr << "demux_open: no stream available" << std::endl;
        return false;
    }
    
    AVDictionary *dopts = nullptr;
    
    // Step 1: Allocate AVFormatContext
    avfc = avformat_alloc_context();
    if (!avfc) {
        std::cerr << "Failed to allocate AVFormatContext" << std::endl;
        goto fail;
    }
    
    // Step 2: Set probesize if needed
    if (probesize > 0) {
        if (av_opt_set_int(avfc, "probesize", probesize, 0) < 0) {
            std::cerr << "Couldn't set probesize to " << probesize << std::endl;
        }
    }
    
    // Step 3: Set analyzeduration if needed
    if (analyzeduration > 0.0) {
        if (av_opt_set_int(avfc, "analyzeduration", 
                          (int64_t)(analyzeduration * AV_TIME_BASE), 0) < 0) {
            std::cerr << "Couldn't set analyzeduration to " << analyzeduration << std::endl;
        }
    }
    
    // Step 4: Create custom AVIOContext with our stream callbacks
    {
        void *buffer = av_malloc(buffersize);
        if (!buffer) {
            std::cerr << "Failed to allocate AVIO buffer" << std::endl;
            goto fail;
        }
        
        pb = avio_alloc_context(
            static_cast<unsigned char*>(buffer), 
            buffersize, 
            0,              // write_flag (0 = read-only)
            this,           // opaque pointer passed to callbacks
            mp_read,        // read callback
            nullptr,        // write callback (nullptr for read-only)
            mp_seek         // seek callback
        );
        
        if (!pb) {
            av_free(buffer);
            std::cerr << "Failed to allocate AVIOContext" << std::endl;
            goto fail;
        }
        
        pb->read_seek = mp_read_seek;
        pb->seekable = seekable ? AVIO_SEEKABLE_NORMAL : 0;
        avfc->pb = pb;
    }
    
    // Step 5: Set interrupt callback (optional, for cancellation support)
    // avfc->interrupt_callback = (AVIOInterruptCB){ .callback = interrupt_cb, .opaque = this };
    
    // Step 6: Open input
    if (avformat_open_input(&avfc, filename.c_str(), nullptr, &dopts) < 0) {
        std::cerr << "avformat_open_input failed" << std::endl;
        goto fail;
    }
    
    // Step 7: Find stream info
    if (avformat_find_stream_info(avfc, nullptr) < 0) {
        std::cerr << "avformat_find_stream_info failed" << std::endl;
        goto fail;
    }
    
    std::cout << "Opened media with " << avfc->nb_streams << " streams" << std::endl;
    
    // Step 8: Extract metadata
    if (avfc->start_time != AV_NOPTS_VALUE) {
        start_time = avfc->start_time / (double)AV_TIME_BASE;
    }
    
    // Step 9: Calculate duration (prefer per-stream duration first)
    {
        double total_duration = -1.0;
        double av_duration = -1.0;
        
        for (unsigned int n = 0; n < avfc->nb_streams; n++) {
            AVStream *st = avfc->streams[n];
            if (st->duration <= 0)
                continue;
            
            double f_duration = st->duration * av_q2d(st->time_base);
            total_duration = std::max(total_duration, f_duration);
            
            if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO ||
                st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                av_duration = std::max(av_duration, f_duration);
            }
        }
        
        duration = av_duration > 0 ? av_duration : total_duration;
        
        // Fall back to container duration if no stream duration
        if (duration <= 0 && avfc->duration > 0) {
            duration = (double)avfc->duration / AV_TIME_BASE;
        }
    }
    
    // Step 10: Check seekability
#ifdef AVFMTCTX_UNSEEKABLE
    if (avfc->ctx_flags & AVFMTCTX_UNSEEKABLE) {
        seekable = false;
    }
#endif
    
    std::cout << "Duration: " << duration << "s, Start time: " << start_time 
              << "s, Seekable: " << (seekable ? "yes" : "no") << std::endl;
    
    is_open = true;
    av_dict_free(&dopts);
    return true;

fail:
    if (avfc) {
        // avformat_open_input cleans up avfc on failure, but not if it fails before calling it
        if (avfc->pb == nullptr) {
            avformat_free_context(avfc);
        } else {
            avformat_close_input(&avfc);
        }
        avfc = nullptr;
    }
    if (pb) {
        if (pb->buffer) {
            av_free(pb->buffer);
        }
        avio_context_free(&pb);
        pb = nullptr;
    }
    av_dict_free(&dopts);
    return false;
}

bool Demuxer::seek(double timestamp_seconds) {
    if (!is_open || !avfc) {
        return false;
    }
    
    int64_t ts = static_cast<int64_t>(timestamp_seconds * AV_TIME_BASE);
    int ret = av_seek_frame(avfc, -1, ts, AVSEEK_FLAG_BACKWARD);
    
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, ret);
        std::cerr << "av_seek_frame failed: " << errbuf << std::endl;
        return false;
    }
    
    std::cout << "Seeked to " << timestamp_seconds << " seconds" << std::endl;
    return true;
}

bool Demuxer::seek_to_start() {
    if (!is_open || !avfc)
        return false;
    int ret = av_seek_frame(avfc, -1, 0, AVSEEK_FLAG_BACKWARD);
    if (ret < 0)
        return false;
    avformat_flush(avfc);
    return true;
}

bool Demuxer::decode_first_video_frame() {
    if (!is_open || !avfc)
        return false;

    // Find first video stream
    int vindex = -1;
    AVCodecParameters *vpar = nullptr;
    for (unsigned i = 0; i < avfc->nb_streams; ++i) {
        if (avfc->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            vindex = (int)i;
            vpar = avfc->streams[i]->codecpar;
            break;
        }
    }
    if (vindex < 0 || !vpar)
        return false;

    const AVCodec *codec = avcodec_find_decoder(vpar->codec_id);
    if (!codec)
        return false;

    AVCodecContext *cc = avcodec_alloc_context3(codec);
    if (!cc)
        return false;
    int ret = avcodec_parameters_to_context(cc, vpar);
    if (ret < 0) {
        avcodec_free_context(&cc);
        return false;
    }
    ret = avcodec_open2(cc, codec, nullptr);
    if (ret < 0) {
        avcodec_free_context(&cc);
        return false;
    }

    // Seek to start
    if (!seek_to_start()) {
        avcodec_free_context(&cc);
        return false;
    }

    AVPacket *pkt = av_packet_alloc();
    AVFrame *frm = av_frame_alloc();
    bool got = false;
    if (!pkt || !frm) {
        av_frame_free(&frm);
        av_packet_free(&pkt);
        avcodec_free_context(&cc);
        return false;
    }

    // Read a few packets until we decode 1 frame
    for (int tries = 0; tries < 200 && !got; ++tries) {
        ret = av_read_frame(avfc, pkt);
        if (ret < 0) {
            // try flushing decoder if EOF
            avcodec_send_packet(cc, nullptr);
        } else {
            if (pkt->stream_index == vindex)
                avcodec_send_packet(cc, pkt);
        }

        while (true) {
            ret = avcodec_receive_frame(cc, frm);
            if (ret == AVERROR(EAGAIN))
                break;
            if (ret == AVERROR_EOF)
                break;
            if (ret < 0)
                break;
            got = true;
            break;
        }
        av_packet_unref(pkt);
    }

    av_frame_free(&frm);
    av_packet_free(&pkt);
    avcodec_free_context(&cc);
    return got;
}

bool Demuxer::read_packet(DemuxPacket **packet) {
    if (!is_open || !avfc || !packet) {
        return false;
    }
    
    AVPacket *av_pkt = av_packet_alloc();
    if (!av_pkt) {
        std::cerr << "Failed to allocate AVPacket" << std::endl;
        return false;
    }
    
    int ret = av_read_frame(avfc, av_pkt);
    if (ret < 0) {
        av_packet_free(&av_pkt);
        if (ret == AVERROR_EOF) {
            std::cout << "End of file reached" << std::endl;
        } else {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_make_error_string(errbuf, AV_ERROR_MAX_STRING_SIZE, ret);
            std::cerr << "av_read_frame failed: " << errbuf << std::endl;
        }
        return false;
    }
    
    // Convert AVPacket to DemuxPacket
    DemuxPacket *pkt = new DemuxPacket();
    pkt->stream = av_pkt->stream_index;
    pkt->pos = av_pkt->pos;
    pkt->keyframe = (av_pkt->flags & AV_PKT_FLAG_KEY) != 0;
    
    // Convert timestamps
    AVStream *st = avfc->streams[av_pkt->stream_index];
    if (av_pkt->pts != AV_NOPTS_VALUE) {
        pkt->pts = av_pkt->pts * av_q2d(st->time_base);
    }
    if (av_pkt->dts != AV_NOPTS_VALUE) {
        pkt->dts = av_pkt->dts * av_q2d(st->time_base);
    }
    if (av_pkt->duration > 0) {
        pkt->duration = av_pkt->duration * av_q2d(st->time_base);
    }
    
    // Copy packet data
    pkt->len = av_pkt->size;
    pkt->buffer = static_cast<unsigned char*>(malloc(av_pkt->size));
    if (pkt->buffer) {
        memcpy(pkt->buffer, av_pkt->data, av_pkt->size);
    }
    
    *packet = pkt;
    av_packet_free(&av_pkt);
    
    return true;
}

void Demuxer::close() {
    if (avfc) {
        avformat_close_input(&avfc);
        avfc = nullptr;
    }
    
    if (pb) {
        if (pb->buffer) {
            av_free(pb->buffer);
        }
        avio_context_free(&pb);
        pb = nullptr;
    }
    
    if (stream) {
        delete stream;
        stream = nullptr;
    }
    
    is_open = false;
    seekable = false;
    start_time = 0.0;
    duration = -1.0;
    
    std::cout << "Demuxer closed" << std::endl;
}
