#include "stream.h"
#include <cerrno>
#include <cstring>

Stream::Stream() {}

bool Stream::open(const std::string &url)
{
    close();
    url_ = url;

    AVDictionary *dict = nullptr;
    AVIOInterruptCB cb{}; // optional interrupt callback
    int flags = AVIO_FLAG_READ;

    int err = avio_open2(&avio_, url.c_str(), flags, &cb, &dict);
    if (err < 0) {
        av_dict_free(&dict);
        avio_ = nullptr;
        return false;
    }
    av_dict_free(&dict);

    // Determine size if available
    int64_t cur = avio_tell(avio_);
    if (avio_seek(avio_, 0, SEEK_END) >= 0) {
        size_ = avio_tell(avio_);
        if (size_ >= 0 && avio_seek(avio_, cur, SEEK_SET) >= 0)
            seekable = true;
        else
            seekable = false;
    } else {
        size_ = -1;
        seekable = false;
    }

    pos = avio_tell(avio_);
    eof = 0;
    return true;
}

Stream *Stream::create(const std::string &url)
{
    Stream *s = new Stream();
    if (!s->open(url)) {
        delete s;
        return nullptr;
    }
    return s;
}

int Stream::readPartial(void *buf, int max_len)
{
    if (!avio_ || max_len <= 0)
        return -1;
    int n = avio_read(avio_, (unsigned char*)buf, max_len);
    if (n <= 0) {
        if (n == AVERROR_EOF)
            eof = 1;
        return 0;
    }
    pos = avio_tell(avio_);
    return n;
}

int Stream::fillBuffer(void *buf, int max_len)
{
    if (!avio_ || max_len <= 0)
        return -1;
    int r = avio_read_partial(avio_, (unsigned char*)buf, max_len);
    if (r <= 0)
        return -1;
    pos = avio_tell(avio_);
    return r;
}

int Stream::writeBuffer(const void *buf, int len)
{
    if (!avio_ || len <= 0)
        return -1;
    avio_write(avio_, (const unsigned char*)buf, len);
    avio_flush(avio_);
    if (avio_->error)
        return -1;
    pos = avio_tell(avio_);
    return len;
}

bool Stream::seek(int64_t new_pos)
{
    if (!avio_)
        return false;
    if (new_pos < 0)
        return false;
    if (avio_seek(avio_, new_pos, SEEK_SET) >= 0) {
        pos = avio_tell(avio_);
        eof = 0;
        return true;
    }
    return false;
}

int64_t Stream::tell() const
{
    return pos;
}

int64_t Stream::getSize() const
{
    return size_;
}

void Stream::dropBuffers()
{
    // Ring buffer drop logic would go here
    // For now, just reset internal ring buffer state if present
    // Since we're using AVIOContext directly, we can call avio_flush to drop internal buffers
    if (avio_) {
        avio_flush(avio_);
    }
}

void Stream::close()
{
    if (avio_) {
        avio_closep(&avio_);
        avio_ = nullptr;
    }
    pos = 0;
    eof = 0;
    size_ = -1;
    url_.clear();
}
