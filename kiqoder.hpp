#pragma once

#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <functional>
#include <cstring>
#include <cassert>
#include <cmath>

namespace kiqoder {
static const uint32_t MAX_PACKET_SIZE = 4096;
static const uint8_t  HEADER_SIZE     = 4;

class FileHandler {
public:
struct File
{
  uint8_t*  byte_ptr;
  uint32_t  size;
  bool      complete;
};

//----------------------------------------------
//------------------Decoder---------------------
//----------------------------------------------
using ReceiveFn      = std::function<void(uint32_t, uint8_t*, size_t)>;
using FileCallbackFn = std::function<void(int32_t, uint8_t*, size_t)>;
class Decoder
{
public:
  Decoder(ReceiveFn file_callback_fn_ptr,
          bool      keep_header)
  : file_buffer         (nullptr),
    packet_buffer       (nullptr),
    index               (0),
    packet_buffer_offset(0),
    total_packets       (0),
    file_buffer_offset  (0),
    file_size           (0),
    m_file_cb_ptr       (file_callback_fn_ptr),
    m_keep_header       (keep_header),
    m_header_size       (HEADER_SIZE)
    {}
  //----------------------------------------------
  ~Decoder()
  {
    if (m_buffers.size())
      for (auto&& buffer : m_buffers) delete[] buffer;

    if (file_buffer != nullptr)
    {
      delete[] file_buffer;
      delete[] packet_buffer;
      file_buffer   = nullptr;
      packet_buffer = nullptr;
    }
  }
  //----------------------------------------------
  void setID(uint32_t id)
  {
    m_id = id;
  }
  //----------------------------------------------
  void clearPacketBuffer()
  {
    if (packet_buffer)
      memset(packet_buffer, 0, MAX_PACKET_SIZE);
    packet_buffer_offset = 0;
  }
  //----------------------------------------------
  void reset()
  {
    index              = 0;
    total_packets      = 0;
    file_buffer_offset = 0;
    file_size          = 0;
    clearPacketBuffer();
  }
  //----------------------------------------------
  uint8_t* PrepareBuffer(uint8_t* ptr, uint32_t new_size)
  {
    if (ptr)
      m_buffers.push_back(ptr);
    return new uint8_t[new_size];
  }
  //----------------------------------------------
  void processPacketBuffer(uint8_t* data, uint32_t size)
  {
          uint32_t bytes_to_finish{}; // current packet
          int32_t  remaining      {}; // after
          uint32_t bytes_to_copy  {}; // packet buffer
          uint32_t packet_size    {};
          bool     packet_received{};
    const bool     is_first_packet = (!index && !packet_buffer_offset && file_size > (MAX_PACKET_SIZE - m_header_size));
    const bool     is_last_packet  = index == (total_packets);

    if (is_first_packet)
      bytes_to_finish = packet_size = (m_keep_header) ? MAX_PACKET_SIZE : MAX_PACKET_SIZE - m_header_size;
    else
    if (is_last_packet)
    {
      packet_size     = file_size   - file_buffer_offset;
      bytes_to_finish = packet_size - packet_buffer_offset;
    }
    else
    { // Other chunks
      packet_size     = MAX_PACKET_SIZE;
      bytes_to_finish = MAX_PACKET_SIZE - packet_buffer_offset;
    }

    remaining               = (size - bytes_to_finish);
    packet_received         = (size >= bytes_to_finish);
    bytes_to_copy           = (packet_received) ? bytes_to_finish : size;
    std::memcpy(packet_buffer + packet_buffer_offset, data, bytes_to_copy);
    packet_buffer_offset    += bytes_to_copy;

    assert((packet_buffer_offset <= MAX_PACKET_SIZE));

    if (packet_received)
    {
      std::memcpy((file_buffer + file_buffer_offset), packet_buffer, packet_size);
      file_buffer_offset = (file_buffer_offset + packet_size);
      clearPacketBuffer();
      index++;

      if (is_last_packet)
      {
        m_file_cb_ptr(m_id, file_buffer, file_size);
        reset();
      }

      if (remaining > HEADER_SIZE)
      {
        const bool last_packet_complete = ((index == total_packets) && static_cast<uint32_t>(remaining) >= (file_size - file_buffer_offset));
        if (is_last_packet || last_packet_complete)
          processPacket((data + bytes_to_copy), remaining);
        else
        {
          std::memcpy(packet_buffer, (data + bytes_to_copy), remaining);
          packet_buffer_offset = packet_buffer_offset + remaining;
        }
      }
    }
  }
  //----------------------------------------------
  void processPacket(uint8_t* data, uint32_t size)
  {
    uint32_t process_index{0};

    while (size)
    {
      bool     is_first_chunk  = (!index) && !packet_buffer_offset && !file_buffer_offset;
      uint32_t size_to_read    = size <= MAX_PACKET_SIZE ? size : MAX_PACKET_SIZE;

      if (is_first_chunk)
      {
        file_size     = (m_keep_header) ?
          int(data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]) + HEADER_SIZE + 1 :
          int(data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]) - HEADER_SIZE;
        total_packets = static_cast<uint32_t>(ceil(static_cast<double>(file_size / MAX_PACKET_SIZE)));
        file_buffer   = PrepareBuffer(file_buffer, file_size);

        if (nullptr == packet_buffer)
          packet_buffer = new uint8_t[MAX_PACKET_SIZE];
        file_buffer_offset = 0;

        (m_keep_header) ?
          processPacketBuffer(data, size_to_read) :
          processPacketBuffer(data + HEADER_SIZE, size_to_read - HEADER_SIZE);
      }
      else
        processPacketBuffer((data + process_index), size_to_read);

      size          -= size_to_read;
      process_index += size_to_read;
    }
  }

   private:
    using buffers = std::vector<uint8_t*>;

    uint8_t*    file_buffer;
    uint8_t*    packet_buffer;
    uint32_t    index;
    uint32_t    packet_buffer_offset;
    uint32_t    total_packets;
    uint32_t    file_buffer_offset;
    uint32_t    file_size;
    ReceiveFn   m_file_cb_ptr;
    bool        m_keep_header;
    uint8_t     m_header_size;
    uint32_t    m_id;
    buffers     m_buffers;
  };

  //----------------------------------------------
  //-----------------FileHandler------------------
  //----------------------------------------------
  FileHandler(FileCallbackFn callback_fn,
              bool           keep_header = false)
  : m_decoder(new Decoder(
      [callback_fn](uint32_t id, uint8_t* data, size_t size)
      {
        if (size)
          callback_fn(id, data, size);
      },
    keep_header))
  {}
  //----------------------------------------------
  FileHandler(FileHandler&& f)
  : m_decoder(f.m_decoder)
  {
    f.m_decoder = nullptr;
  }
  //----------------------------------------------
  FileHandler(const FileHandler& f)
  : m_decoder(new Decoder{*(f.m_decoder)})
  {}
  //----------------------------------------------
  FileHandler &operator=(const FileHandler& f)
  {
    if (&f != this)
    {
      delete m_decoder;
      m_decoder = nullptr;
      m_decoder = new Decoder{*(f.m_decoder)};
    }

    return *this;
  }
  //----------------------------------------------
  FileHandler &operator=(FileHandler &&f)
  {
    if (&f != this)
    {
      delete m_decoder;
      m_decoder   = f.m_decoder;
      f.m_decoder = nullptr;
    }

    return *this;
  }
  //----------------------------------------------
  ~FileHandler()
  {
    delete m_decoder;
  }
  //----------------------------------------------
  void setID(uint32_t id)
  {
    m_decoder->setID(id);
  }
  //----------------------------------------------
  void reset()
  {
    m_decoder->reset();
  }
  //----------------------------------------------
  void processPacket(uint8_t *data, uint32_t size)
  {
    m_decoder->processPacket(data, size);
  }

 private:
  Decoder* m_decoder;
};
} // namespace kiqoder
