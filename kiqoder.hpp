#pragma once

#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <functional>
#include <cstring>
#include <cassert>
#include <cmath>
#include <QDebug>

namespace Kiqoder {
static const uint32_t MAX_PACKET_SIZE = 4096;
static const uint8_t  HEADER_SIZE     = 4;

class FileHandler {
public:

/**
* File
*
* @struct
*/
struct File
{
  uint8_t*  byte_ptr;
  uint32_t  size;
  bool      complete;
};

/**
 * Decoder class
 *
 * @class
 *
 * Does the heavy lifting
 *
 */
using ReceiveFn      = std::function<void(uint32_t, uint8_t*, int32_t)>;
using FileCallbackFn = std::function<void(int32_t, uint8_t*, size_t)>;
class Decoder
{
public:
 /**
  * Default constructor
  *
  * @constructor
  */
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

/**
 * @destructor
 */
  ~Decoder()
  {
    if (file_buffer != nullptr)
    {
      delete[] file_buffer;
      delete[] packet_buffer;
      file_buffer   = nullptr;
      packet_buffer = nullptr;
    }
  }

  void setID(uint32_t id)
  {
    m_id = id;
  }

  /**
   * clearPacketBuffer
   *
   * Clear buffer before writing a new packet
   */

  void clearPacketBuffer()
  {
    memset(packet_buffer, 0, MAX_PACKET_SIZE);
    packet_buffer_offset = 0;
  }

  /**
   * reset
   *
   * Reset the decoder's state so it's ready to decode a new file
   */
  void reset()
  {
    index              = 0;
    total_packets      = 0;
    file_buffer_offset = 0;
//    file_size          = 0;
    clearPacketBuffer();
  }

  /**
   * processPacketBuffer
   *
   * @param[in] {uint8_t*} `data` A pointer to the beginning of a byte sequence
   * @param[in] {uint32_t} `size` The number of bytes to process
   */

  void processPacketBuffer(uint8_t* data, uint32_t size)
  {
    uint32_t bytes_to_finish        {}; // current packet
    uint32_t remaining              {}; // after this call
    uint32_t bytes_to_copy          {}; // to packet buffer
    uint32_t packet_size            {};
    bool     packet_received        {};
    bool     is_last_packet = index == (total_packets);

    if (!index && !packet_buffer_offset && file_size > (MAX_PACKET_SIZE - m_header_size))
      bytes_to_finish = packet_size = (m_keep_header) ?        // 1st chunk
                                        MAX_PACKET_SIZE : MAX_PACKET_SIZE - m_header_size;
    else
    if (is_last_packet)
    {
      packet_size     = file_size   - file_buffer_offset;
      bytes_to_finish = packet_size - packet_buffer_offset;
    }
    else
    {                                                          // All other chunks
      packet_size     = MAX_PACKET_SIZE;
      bytes_to_finish = MAX_PACKET_SIZE - packet_buffer_offset;
    }

    remaining               = (size < bytes_to_finish) ? (bytes_to_finish - size) : (size - bytes_to_finish); // this doesn't work if size is less
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

      if (remaining)
      {
        std::memcpy(packet_buffer, (data + bytes_to_copy), remaining);
        packet_buffer_offset = packet_buffer_offset + remaining;
      }
      else
      if (is_last_packet)
      {
        m_file_cb_ptr(m_id, std::move(file_buffer), file_size);
        reset();

        if (remaining)
          qDebug() << "We have some remaining";
//          processPacket(data + bytes_to_copy, size - bytes_to_copy);
      }
    }
  }
  /**
   * processPacket
   *
   * @param[in] {uint8_t*} `data` A pointer to the beginning of a byte sequence
   * @param[in] {uint32_t} `size` The number of bytes to process
   */
  void processPacket(uint8_t* data, uint32_t size)
  {
    uint32_t process_index{0};
    int32_t  i{};

    while (size)
    {
      bool     is_first_packet = (index == 0);
      uint32_t size_to_read    = size <= MAX_PACKET_SIZE ? size : MAX_PACKET_SIZE;

      if (is_first_packet && !packet_buffer_offset && !file_buffer_offset)   // First iteration
//      if (is_first_packet)   // First iteration
      {
        uint32_t prev_size = file_size;
                 file_size = (m_keep_header) ?
          int(data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]) + HEADER_SIZE + 1 :
          int(data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]) - HEADER_SIZE;
        total_packets = static_cast<uint32_t>(ceil(static_cast<double>(file_size / MAX_PACKET_SIZE)));

        if (prev_size >= file_size)
          memset(file_buffer, 0, file_size);
        else
        {
          if (file_buffer)
            delete[] file_buffer;
          file_buffer = new uint8_t[file_size];
        }

        if (file_size < size_to_read) // We need to account for header. Sometimes it's being disregarded
          size_to_read = (file_size + HEADER_SIZE);

        if (nullptr == packet_buffer)
          packet_buffer = new uint8_t[MAX_PACKET_SIZE];

        file_buffer_offset = 0;

        if (size_to_read < size)
          qDebug() << "Spillover";

        if (m_keep_header)
          processPacketBuffer(data, size_to_read);
        else
          processPacketBuffer((data + HEADER_SIZE), size_to_read - HEADER_SIZE);
      }
      else
      {
        size_to_read = (file_size < size_to_read) ? file_size : size_to_read;
        processPacketBuffer((data + process_index), size_to_read); // TODO: why is this larger than file_size?
      }

//      if ((size_to_read != size) && is_first_packet)
//      {
//        size_to_read = (m_keep_header) ? size_to_read : (size_to_read + HEADER_SIZE);
//        is_first_packet = false;
//      }
      size          -= size_to_read;
      process_index += size_to_read;
      i++;
//      data          += size_to_read;
    }
  }

   private:
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
  };

  /**
   * Default constructor
   * @constructor
   */
  FileHandler(FileCallbackFn callback_fn,
              bool           keep_header = false)
  : m_decoder(new Decoder(
      [callback_fn](uint32_t id, uint8_t*&& data, int size)
      {
        if (size)
          callback_fn(id, std::move(data), size);
      },
    keep_header))
  {}

  /**
   * Move&& constructor
   * @constructor
   */
  FileHandler(FileHandler&& f)
  : m_decoder(f.m_decoder)
  {
    f.m_decoder = nullptr;
  }

  /**
   * Copy& constructor
   * @constructor
   */

  FileHandler(const FileHandler& f)
  : m_decoder(new Decoder{*(f.m_decoder)})
  {}

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

  /**
   * Assignment operator
   * @operator
   */
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

  /**
   * @destructor
   */
  ~FileHandler()
  {
    delete m_decoder;
  }

  /**
   * setID
   *
   * @param id
   */
  void setID(uint32_t id)
  {
    m_decoder->setID(id);
  }

  /**
   * processPacket
   *
   * @param[in] {uint8_t*} `data`
   * @param[in] {uint32_t} `size`
   */
  void processPacket(uint8_t *data, uint32_t size)
  {
    m_decoder->processPacket(data, size);
  }

 private:
  Decoder* m_decoder;
};
} // namespace Decoder
