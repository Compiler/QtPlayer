# QtPlayer Demuxer

A simplified demuxer implementation for QtPlayer, inspired by mpv's demux architecture but adapted for C++ and Qt.

## Architecture Overview

The demuxer consists of two main components:

### 1. Stream (`stream.h` / `stream.cpp`)
Provides an abstraction layer over various media sources (files, URLs, network streams) using FFmpeg's AVIO.

**Key Features:**
- Uses `AVIOContext` for I/O operations
- Supports reading from files and URLs via `avio_open2`
- Ring-buffer style methods for efficient streaming
- Seekable detection and size determination

**Methods:**
- `create(url)` - Factory method to create and open a stream
- `open(url)` - Opens a file/URL using FFmpeg AVIO
- `readPartial(buf, len)` - Blocking read (uses `avio_read`)
- `fillBuffer(buf, len)` - Partial read for ring buffer (uses `avio_read_partial`)
- `writeBuffer(buf, len)` - Write support (uses `avio_write`)
- `seek(pos)` - Absolute byte seek
- `tell()` - Current position
- `getSize()` - Total size (if known)
- `dropBuffers()` - Flush internal buffers
- `close()` - Close the stream

### 2. Demuxer (`demuxer.h` / `demuxer.cpp`)
Main demuxing component that wraps FFmpeg's `AVFormatContext` and provides packet reading.

**Key Features:**
- Custom AVIO callbacks that redirect to the `Stream` layer
- Automatic stream info detection
- Duration and timestamp extraction
- Seeking support (both byte-level and time-based)

**Flow (matching mpv's `demux_open_lavf`):**

1. **Stream Creation** (`demux_open_filename`)
   - Creates a `Stream` object via `Stream::create()`
   - Stores filename and initial seekability

2. **Demuxer Opening** (`demux_open`)
   - Allocates `AVFormatContext`
   - Sets options (probesize, analyzeduration)
   - Creates custom `AVIOContext` with callbacks:
     - `mp_read` → calls `Stream::fillBuffer()`
     - `mp_seek` → calls `Stream::seek()`
     - `mp_read_seek` → optional stream-level seeking
   - Calls `avformat_open_input()` to probe format
   - Calls `avformat_find_stream_info()` to detect streams
   - Extracts metadata (duration, start_time, seekability)

3. **Packet Reading** (`read_packet`)
   - Calls `av_read_frame()` to get next packet
   - Converts `AVPacket` to `DemuxPacket`
   - Converts timestamps using stream timebase

4. **Seeking** (`seek`)
   - Uses `av_seek_frame()` for time-based seeking
   - Supports backward seeking for accurate positioning

5. **Cleanup** (`close`)
   - Closes `AVFormatContext`
   - Frees custom `AVIOContext`
   - Deletes `Stream` object

## Usage Example

```cpp
#include "demuxer.h"
#include <iostream>

int main() {
    Demuxer *demuxer = new Demuxer();
    
    // Open media file
    if (!demuxer->demux_open_filename("video.mp4")) {
        std::cerr << "Failed to open file" << std::endl;
        return -1;
    }
    
    std::cout << "Duration: " << demuxer->get_duration() << "s" << std::endl;
    std::cout << "Seekable: " << demuxer->is_seekable() << std::endl;
    
    // Read packets
    DemuxPacket *packet = nullptr;
    while (demuxer->read_packet(&packet)) {
        std::cout << "Stream: " << packet->stream 
                  << ", PTS: " << packet->pts 
                  << ", Size: " << packet->len << std::endl;
        delete packet;
    }
    
    // Seek to 10 seconds
    demuxer->seek(10.0);
    
    // Cleanup
    demuxer->close();
    delete demuxer;
    
    return 0;
}
```

## Key Differences from mpv

1. **No talloc**: Uses standard C++ `new`/`delete` instead of talloc
2. **No mp_* helpers**: Direct FFmpeg API usage where possible
3. **Simplified structure**: No intermediate `lavf_priv_t` struct
4. **Class-based**: Object-oriented design instead of function pointers
5. **No threading (yet)**: Single-threaded for simplicity

## Comparison with mpv's demux_lavf

### Similar to mpv:
- ✓ Custom `AVIOContext` with read/seek callbacks
- ✓ Stream abstraction layer
- ✓ `avformat_open_input()` → `avformat_find_stream_info()` flow
- ✓ Duration calculation from stream/container
- ✓ Timestamp conversion using stream timebase
- ✓ Seekability detection

### Not implemented (yet):
- ✗ Format detection and format_hacks
- ✗ Cancellation support via `mp_cancel`
- ✗ Chapter handling
- ✗ Multi-stream management (`sh_stream`)
- ✗ Packet queuing and buffering
- ✗ Threading and async packet prefetch
- ✗ Network protocol options
- ✗ DVD/BD special handling

## Build

The demuxer is integrated into QtPlayer's CMake build:

```cmake
SOURCES demux/demuxer.h demux/demuxer.cpp
SOURCES demux/stream.h demux/stream.cpp
```

## Dependencies

- FFmpeg libraries (libavformat, libavcodec, libavutil)
- C++11 or later
- Qt6 (for the main application)

## Future Enhancements

1. **Threading**: Add a demuxer thread with packet queue (like mpv's `demux_internal.c`)
2. **Format Hacks**: Add format-specific workarounds (like mpv's `format_hacks`)
3. **Multiple Streams**: Support for selecting specific audio/video/subtitle tracks
4. **Caching**: Implement packet caching for better seeking
5. **Network Options**: Add support for network protocols (RTSP, HLS, etc.)
6. **Chapter Support**: Parse and expose chapter information
7. **Metadata**: Full metadata extraction (tags, cover art, etc.)

## References

- mpv source: `/demux/demux_lavf.c`
- mpv stream: `/stream/stream_lavf.c`
- FFmpeg docs: https://ffmpeg.org/doxygen/trunk/
