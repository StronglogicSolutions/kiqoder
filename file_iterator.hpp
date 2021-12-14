#pragma once

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace Kiqoder
{

static std::vector<uint8_t> ReadFileAsBytes(const std::string& file_path)
{
  std::ifstream        stream(file_path, std::ios::binary);
  std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(stream), {}};
  stream.close();
  return bytes;
}

static const uint32_t PACKET_SIZE{4096};
template <typename T>
class FileIterator
{
static const uint32_t HEADER_SIZE{4};
public:
FileIterator(const std::string& path)
: m_buffer(PrepareBuffer(std::move(ReadFileAsBytes(path)))),
  data_ptr(nullptr),
  m_bytes_read(0)
{
  if (m_buffer.size())
  {
    data_ptr = m_buffer.data();
    m_size   = m_buffer.size();
  }
}

FileIterator(const T* bytes, const size_t size)
: m_buffer(PrepareBuffer(std::move(std::vector<T>(bytes, bytes + size)))),
  data_ptr(nullptr),
  m_bytes_read(0)
{
  if (!m_buffer.empty())
  {
    data_ptr = m_buffer.data();
    m_size   = m_buffer.size();
  }
}


FileIterator(const FileIterator& i)
: m_buffer(std::move(i.m_buffer)),
  data_ptr(std::move(i.data_ptr)),
  m_bytes_read(std::move(i.m_bytes_read)),
  m_size(std::move(i.m_size))
{}

FileIterator(FileIterator&& i)
: m_buffer(std::move(i.m_buffer)),
  data_ptr(std::move(i.data_ptr)),
  m_bytes_read(std::move(i.m_bytes_read)),
  m_size(std::move(i.m_size))
{}

static std::vector<T> PrepareBuffer (std::vector<T>&& data)
{
  const uint32_t bytes = (data.size() + HEADER_SIZE);
  std::vector<T> buffer{};
  buffer.reserve(bytes);
  buffer.emplace_back((bytes >> 24) & 0xFF);
  buffer.emplace_back((bytes >> 16) & 0xFF);
  buffer.emplace_back((bytes >> 8 ) & 0xFF);
  buffer.emplace_back((bytes      ) & 0xFF);
  buffer.insert(buffer.end(), std::make_move_iterator(data.begin()), std::make_move_iterator(data.end()));
  return buffer;
}

uint32_t GetBytesRead() const
{
  return m_bytes_read;
}

struct PacketWrapper
{
PacketWrapper(T* ptr_, uint32_t size_)
: ptr(ptr_),
  size(size_) {}

T* data() { return ptr; }

T*       ptr;
uint32_t size;
};

bool has_data()
{
  return (data_ptr != nullptr);
}

std::string to_string()
{
  std::string data_s{};
  for (uint32_t i = 0; i < m_buffer.size(); i++) data_s += std::to_string(+(*(m_buffer.data() + i)));
  return data_s;
}

PacketWrapper next() {
  uint32_t size;
  uint32_t bytes_remaining = m_size - m_bytes_read;
  T*       ptr             = data_ptr;

  if (bytes_remaining < PACKET_SIZE)
  {
    size     = bytes_remaining;
    data_ptr = nullptr;
  }
  else
  {
    size     =  PACKET_SIZE;
    data_ptr += PACKET_SIZE;
  }

  m_bytes_read += size;

  return PacketWrapper{ptr, size};
}

private:

std::vector<T> m_buffer;
T*             data_ptr;
uint32_t       m_bytes_read;
uint32_t       m_size;
};
} // ns kiqoder
