#ifndef _PAINLESS_MESH_SERIALIZER_HPP_
#define _PAINLESS_MESH_SERIALIZER_HPP_

#include <string>
#include <type_traits>
#include "plugin/typetraitsExtension.hpp"
#include "nodeTree.hpp"

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
    T, typename std::enable_if<std::is_trivially_copyable<T>::value>::type> {
  static void deserialize(T* dest, const std::string& str, int& offset) {
    Serial.println(offset);
    memcpy(dest, &str[offset], sizeof(T));
    offset += sizeof(T);
    Serial.println(offset);
  }
  static void serialize(T* source, std::string& str, int& offset) {
    str.reserve(offset + sizeof(T));
    memcpy(&str[offset], source, sizeof(T));
    offset += sizeof(T);
  }
};
template <>
struct InternalSerializer<std::string, void> {
  static void deserialize(std::string* dest, const std::string& str,
                          int& offset) {
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

template <>
struct InternalSerializer<protocol::NodeTree, void> {
  static void deserialize(protocol::NodeTree* dest, const std::string& str,
                          int& offset) {
    SerializeHelper::deserialize(&dest->nodeId, str, offset);
    SerializeHelper::deserialize(&dest->root, str, offset);

    uint16_t length;
    SerializeHelper::deserialize(&length, str, offset);
    dest->subs.resize(length);

    for (auto&& i : dest->subs) {
      SerializeHelper::deserialize(&i, str, offset);
    }
  }

  static void serialize(protocol::NodeTree* source, std::string& str,
                        int& offset) {
    SerializeHelper::serialize(&source->nodeId, str, offset);
    SerializeHelper::serialize(&source->root, str, offset);

    uint16_t length = source->subs.size();
    SerializeHelper::serialize(&length, str, offset);

    for (auto&& i : source->subs) {
      SerializeHelper::serialize(&i, str, offset);
    }
  }
};

template <class T>
void SerializeHelper::deserialize(T* dest, const std::string& str,
                                  int& offset) {
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