#ifndef DEMUXER_H
#define DEMUXER_H

#include <cstdint>
#include <string>
#include "stream.h"

// Forward declarations for FFmpeg types
struct AVFormatContext;
struct AVIOContext;
struct AVDictionary;
struct AVInputFormat;
struct AVCodecContext;
struct AVFrame;

// Simple packet structure
struct DemuxPacket {
    double pts;           // Presentation timestamp
    double dts;           // Decode timestamp  
    double duration;      // Packet duration
    int64_t pos;          // Position in source file
    
    unsigned char *buffer; // Packet data
    size_t len;           // Packet data length
    
    int stream;           // Source stream index
    bool keyframe;        // Is this a keyframe
    
    DemuxPacket();
    ~DemuxPacket();
};

class Demuxer
{
public:
    AVFormatContext *avfc;
    AVIOContext *pb;
    std::string filename;
    bool is_open;
    Stream *stream;
    
    // Demuxer options
    unsigned int buffersize;
    unsigned int probesize;
    double analyzeduration;
    
    // State
    bool seekable;
    double start_time;
    double duration;
    
    // Static callbacks for AVIOContext
    static int mp_read(void *opaque, uint8_t *buf, int size);
    static int64_t mp_seek(void *opaque, int64_t offset, int whence);
    static int64_t mp_read_seek(void *opaque, int stream_index, int64_t timestamp, int flags);

public:
    Demuxer();
    ~Demuxer();
    
    // Open stream and create AVFormatContext
    bool demux_open_filename(const std::string &filename);
    bool demux_open();
    
    // Seek using avio_seek or av_seek_frame
    bool seek(double timestamp_seconds);
    
    // Read packet using av_read_frame
    bool read_packet(DemuxPacket **packet);
    
    // Close and cleanup
    void close();
    
    // Check if demuxer is open
    bool is_open_stream() const { return is_open; }
    
    // Getters
    double get_start_time() const { return start_time; }
    double get_duration() const { return duration; }
    bool is_seekable() const { return seekable; }

    // Convenience helpers
    bool seek_to_start();
    // Minimal: seek to start and decode the first video frame (for probing)
    bool decode_first_video_frame();

    // Video decoding lifecycle
    bool init_video_decoder();
    // Demux and decode until one video frame is produced; returns true and
    // stores it in an internal frame buffer retrievable via get_last_frame().
    bool decode_next_video_frame();
    // Seek to "seconds" and decode forward until a frame at or after that
    // timestamp is produced. Stores it in the internal frame buffer.
    bool decode_to_frame_at_timestamp(double seconds);
    // Returns a pointer to the last decoded frame (owned by Demuxer).
    AVFrame *get_last_frame() const;
    // Video stream helpers
    int get_video_stream_index() const;
    AVRational get_video_time_base() const;

private:
    // Decoder state
    AVCodecContext *video_dec_ctx = nullptr;
    AVPacket *tmp_pkt = nullptr;
    AVFrame *tmp_frame = nullptr;
    int video_stream_index = -1;
};

#endif // DEMUXER_H
