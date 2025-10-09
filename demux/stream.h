#ifndef STREAM_H
#define STREAM_H

#include <cstdint>
#include <string>
#include <cstdio>
extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
}
class Stream
{
public:
    const char *name;
    int64_t pos;
    int eof; // valid only after read calls that returned a short result
    int mode; //STREAM_READ or STREAM_WRITE
    int stream_origin; // any STREAM_ORIGIN_*
    void *priv; // used for DVD, TV, RTSP etc
    char *path; // filename (url without protocol prefix)
    char *demuxer; // request demuxer to be used
    char *lavf_type; // name of expected demuxer type for lavf
    bool seekable : 1; // presence of general byte seeking support
    bool fast_skip : 1; // consider stream fast enough to fw-seek by skipping
    bool allow_partial_read : 1; // allows partial read with stream_read_file()

    // Read statistic for fill_buffer calls. All bytes read by fill_buffer() are
    // added to this. The user can reset this as needed.
    uint64_t total_unbuffered_read_bytes;
    // Seek statistics. The user can reset this as needed.
    uint64_t total_stream_seeks;

    // Buffer size requested by user; s->buffer may have a different size
    int requested_buffer_size;

    // This is a ring buffer. It is reset only on seeks (or when buffers are
    // dropped). Otherwise old contents always stay valid.
    // The valid buffer is from buf_start to buf_end; buf_end can be larger
    // than the buffer size (requires wrap around). buf_cur is a value in the
    // range [buf_start, buf_end].
    // When reading more data from the stream, buf_start is advanced as old
    // data is overwritten with new data.
    // Example:
    //    0  1  2  3    4  5  6  7    8  9  10 11   12 13 14 15
    //  +===========================+---------------------------+
    //  + 05 06 07 08 | 01 02 03 04 + 05 06 07 08 | 01 02 03 04 +
    //  +===========================+---------------------------+
    //                  ^ buf_start (4)  |          |
    //                                   |          ^ buf_end (12 % 8 => 4)
    //                                   ^ buf_cur (9 % 8 => 1)
    // Here, the entire 8 byte buffer is filled, i.e. buf_end - buf_start = 8.
    // buffer_mask == 7, so (x & buffer_mask) == (x % buffer_size)
    unsigned int buf_start; // index of oldest byte in buffer (is <= buffer_mask)
    unsigned int buf_cur;   // current read pos (can be > buffer_mask)
    unsigned int buf_end;   // end position (can be > buffer_mask)

    unsigned int buffer_mask; // buffer_size-1, where buffer_size == 2**n
    uint8_t *buffer;

    // Minimal FFmpeg AVIO-backed implementation details
    AVIOContext *avio_ = nullptr;
    int64_t size_ = -1;
    std::string url_;
public:
    Stream();

    // Open a file/URL using simple POSIX FILE* (read-only)
    bool open(const std::string &url);
    // Factory similar to stream_create()
    static Stream *create(const std::string &url);
    // Read up to max_len bytes, return bytes read (0 on EOF), <0 on error
    int readPartial(void *buf, int max_len);
    // Fill caller buffer from source (low-level), return -1 on error/EOF
    int fillBuffer(void *buf, int max_len);
    // Write to source if supported, return bytes written or -1 on error
    int writeBuffer(const void *buf, int len);
    // Seek to absolute byte position, return true on success
    bool seek(int64_t new_pos);
    // Current byte position
    int64_t tell() const;
    // Total size in bytes, or -1 if unknown
    int64_t getSize() const;
    // EOF flag after short read
    bool isEof() const { return eof != 0; }
    // Drop internal ring buffer contents
    void dropBuffers();
    // Close underlying handle
    void close();
};

#endif // STREAM_H
