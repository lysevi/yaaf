#include <libnmq/serialization/serialization.h>

namespace nmq {
namespace serialization {

// template <> size_t get_size_of<std::string>(const std::string &s) {
//  return sizeof(uint32_t) + s.length();
//}

// template <typename Iterator>
// void write_value<Iterator, std::string>(Iterator it,
//                              const std::string &s) {
//  auto len = static_cast<uint32_t>(s.size());
//  std::memcpy(buffer.data() + offset, &len, sizeof(uint32_t));
//  std::memcpy(buffer.data() + offset + sizeof(uint32_t), s.data(), s.size());
//}

// template <>
// void read_value<std::string>(std::vector<uint8_t> &buffer, size_t &offset,
//	std::string &s) {
//	uint32_t len = 0;
//	std::memcpy(&len, buffer.data() + offset, sizeof(uint32_t));
//	s.resize(len);
//	std::memcpy(&s[0], buffer.data() + offset + sizeof(uint32_t), size_t(len));
//}
} // namespace serialization
} // namespace nmq
