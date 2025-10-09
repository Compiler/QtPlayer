# Simplified Demuxer for QtPlayer

This is a very simple demuxer implementation that provides just the basic functionality needed for reading packets from media files.

## Structure

The demuxer consists of just two main components:

1. **Demuxer** - Main demuxer class with public API
2. **Stream** - Stream class (in parent directory)

## Key Files

- `demuxer.h/cpp` - Main demuxer class with packet reading functionality
- `example.cpp` - Simple example showing how to use the demuxer

## Usage Example

```cpp
#include "demuxer.h"
#include "../stream.h"

// Create demuxer
Demuxer *demuxer = new Demuxer();

// Open with a demuxer implementation
const DemuxerDesc &desc = SimpleDemuxer::get_desc();
if (demuxer->open(&desc, stream) != 0) {
    // Handle error
}

// Get stream info
for (int i = 0; i < demuxer->get_num_streams(); i++) {
    StreamInfo *info = demuxer->get_stream(i);
    // Use stream info
}

// Read packets
DemuxPacket *packet = demuxer->get_next_packet();
if (packet) {
    // Process packet->pts, packet->dts, packet->buffer, packet->len
    delete packet;
}
```

## Key Features

- **Simple Packet Reading** - Read packets with PTS/DTS timestamps
- **Stream Management** - Add/get stream information
- **Basic Seeking** - Seek to specific timestamps
- **Extensible** - Easy to add new demuxer implementations

## Demuxer Implementation

To create a new demuxer, implement these functions:

```cpp
class MyDemuxer {
public:
    static int open(Demuxer *demuxer, int check);
    static bool read_packet(Demuxer *demuxer, DemuxPacket **pkt);
    static void close(Demuxer *demuxer);
    static void seek(Demuxer *demuxer, double rel_seek_secs, int flags);
    static const DemuxerDesc& get_desc();
};
```

This simplified structure provides just the core functionality needed for basic media playback without the complexity of mpv's full implementation.