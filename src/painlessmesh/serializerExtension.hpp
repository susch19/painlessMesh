#ifndef _PAINLESS_MESH_SERIALIZER_HPP_
#define _PAINLESS_MESH_SERIALIZER_HPP_

#include "nodeTree.hpp"
#include "typetraitsExtension.hpp"
#include "serializer.hpp"
#ifdef ESP8266
#include <GDBStub.h>
#endif


template <>
struct InternalSerializer<painlessmesh::protocol::NodeTree, void> {
  static void deserialize(painlessmesh::protocol::NodeTree* dest, const std::string& str,
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

  static void serialize(const painlessmesh::protocol::NodeTree* source, std::string& str,
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

#endif