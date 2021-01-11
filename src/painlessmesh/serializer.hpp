#ifndef _PAINLESS_MESH_SERIALIZER_HPP_
#define _PAINLESS_MESH_SERIALIZER_HPP_

#include <string>
#include <type_traits>

namespace painlessmesh {
struct SerializeHelper {
  template <class T>
  static void deserialize(T* dest, const std::string& str, int& offset);
  template <class T>
  static void serialize(T* source, std::string& str, int& offset);
};

template <class T, typename = void>
struct InternalSerializer {
  static void deserialize(T* dest, const std::string& str, int& offset) {
    static_assert(!std::is_same<T, T>::value, "Type can't be deserialized!");
  }
  static void serialize(T* source, std::string& str, int& offset) {
    static_assert(!std::is_same<T, T>::value, "Type can't be serialized!");
  }
};

template <class T>
struct InternalSerializer<
    T, typename std::enable_if<std::is_pod<T>::value>::type> {
  static void deserialize(T* dest, const std::string& str, int& offset) {
    memcpy(dest, &str[offset], sizeof(T));
    offset += sizeof(T);
  }
  static void serialize(T* source, std::string& str, int& offset) {
    str.reserve(offset + sizeof(T));
    memcpy(&str[offset], source, sizeof(T));
    offset += sizeof(T);
  }
};
template <>
struct InternalSerializer<std::string, void> {
  static void deserialize(std::string* dest, const std::string& str, int& offset) {
    uint16_t length;
    SerializeHelper::deserialize(&length, str, offset);
    dest->assign(&str[offset], static_cast<size_t>(length));
    offset += length;
  }
  static void serialize(std::string* source, std::string& str, int& offset) {
    uint16_t length = source->size();
    str.reserve(length + sizeof(int));
    SerializeHelper::serialize(&length, str, offset);
    memcpy(&str[offset], &(*source)[0], static_cast<size_t>(length));
    offset += length;
  }
};

  template <class T>
  void SerializeHelper::deserialize(T* dest, const std::string& str, int& offset) {
    InternalSerializer<T>::deserialize(dest, str, offset);
  }
  template <class T>
  void SerializeHelper::serialize(T* source, std::string& str, int& offset) {
    InternalSerializer<T>::serialize(source, str, offset);
  }

// struct SerializeHelper {
//   template <class T>
//   static void deserialize(T* dest, std::string& str, int& offset) {
//     InternalSerializer<T>::deserialize(dest, str, offset);
//   }
//   template <class T>
//   static void serialize(T* source, std::string& str, int& offset) {
//     InternalSerializer<T>::serialize(source, str, offset);
//   }
// };
}  // namespace painlessmesh
#endif