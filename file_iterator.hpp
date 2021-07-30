#pragma once

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static std::vector<uint8_t> readFileAsBytes(const std::string& file_path)
{
  std::ifstream        stream(file_path, std::ios::binary);
  std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(stream), {}};
  stream.close();
  return bytes;
}


static const uint32_t PACKET_SIZE{4096};
class FileIterator
{
public:
FileIterator(const std::string& path)
: m_buffer(readFileAsBytes(path)),
  data_ptr(nullptr)
{
  if (!m_buffer.empty())
  {
    data_ptr = m_buffer.data();
    m_size   = m_buffer.size();
  }
}

struct PacketWrapper
{
  PacketWrapper(uint8_t* ptr_, uint32_t size_)
  : ptr(ptr_),
    size(size_) {}
  uint8_t* ptr;
  uint32_t size;

  uint8_t* data() { return ptr; }
};

bool has_data()
{
  return data_ptr != nullptr;
}

PacketWrapper next() {
  uint8_t* ptr = data_ptr;
  uint32_t bytes_remaining = m_size - m_bytes_read;
  uint32_t size{};
  if (bytes_remaining < PACKET_SIZE)
  {
    size = bytes_remaining;
    data_ptr = nullptr;
  }
  else
  {
    size      = PACKET_SIZE;
    data_ptr += PACKET_SIZE;
  }
    return PacketWrapper{ptr, size};
}

private:

std::vector<uint8_t> m_buffer;
uint8_t*             data_ptr;
uint32_t             m_bytes_read;
uint32_t             m_size;
};
