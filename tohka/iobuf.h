//
// Created by li on 2022/3/1.
//

#ifndef TOHKA_TOHKA_IOBUF_H
#define TOHKA_TOHKA_IOBUF_H

// prependable = readIndex
// readable = writeIndex - readIndex
// writable = size() - writeIndex
#include "platform.h"
namespace tohka {
class IoBuf {
 public:
  static constexpr size_t kPreparedSize = 2048;
  static constexpr size_t kPrependSize = 16;

  explicit IoBuf(size_t len = kPreparedSize);
  // append data to buffer
  void Append(const char* data, size_t len);
  void Append(const void* data, size_t len);

  std::string ReceiveAllAsString();

  // Get the first pointer of readable data
  const char* Peek() { return data_.data() + read_index_; }

  void Retrieve(size_t len);
  void Refresh();

  size_t GetReadableSize() const { return write_index_ - read_index_; }
  size_t GetWriteableSize() { return data_.size() - write_index_; }
  size_t GetReadIndex() const { return read_index_; }
  size_t GetWriteIndex() const { return write_index_; }
  void EnsureWritableBytes(size_t len);
  void MakeSpace(size_t len);

 private:
  size_t read_index_;
  size_t write_index_;
  std::vector<char> data_;
};
}  // namespace tohka
#endif  // TOHKA_TOHKA_IOBUF_H
