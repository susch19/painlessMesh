#ifndef _PAINLESS_MESH_VARIANT_HPP_
#define _PAINLESS_MESH_VARIANT_HPP_

#include <string>

#include "protocol.hpp"
#include "serializer.hpp"

namespace painlessmesh {


class VariantBase {
 public:
  virtual void serializeTo(std::string& str) {}
  virtual void deserializeFrom(const std::string& str) {}
};

/**
 * Can store any package variant
 *
 * Internally stores packages as a JsonObject. Main use case is to convert
 * different packages from and to Json (using ArduinoJson).
 */
template <typename T>
class TypedVariantBase : public VariantBase {
 public:
  T* package;

  TypedVariantBase(T* single, bool cleanup = false)
      : package(single), cleanup(cleanup) {}

  inline T* to() { return package.get(); }

  int type() { return package->packageType(); }

  // uint16_t routing() { return package->header.routing; }

  // uint16_t dest() { return package->header.dest; }

  virtual ~TypedVariantBase() {
    if (!cleanup) return;
    delete package;
  }
  virtual void serializeTo(std::string& str);
  virtual void deserializeFrom(const std::string& str);

 private:
  bool cleanup;
};

template <class T>
class Variant : public TypedVariantBase<T> {
 public:
  Variant(T* single, bool cleanup = false)
      : TypedVariantBase<T>(single, cleanup) {}
};

template <>
class Variant<protocol::Single> : public TypedVariantBase<protocol::Single> {
 public:
  Variant(protocol::Single* single, bool cleanup = false)
      : TypedVariantBase<protocol::Single>(single, cleanup) {}
  void serializeTo(std::string& str) override {
    int offset = 0;
    package->header.serializeTo(str, offset);
    SerializeHelper::serialize(&package->from, str, offset);
    SerializeHelper::serialize(&package->msg, str, offset);
  }
  void deserializeFrom(const std::string& str) override {
    int offset = 0;
    package->header.deserializeFrom(str, offset);
    SerializeHelper::deserialize(&package->from, str, offset);
    SerializeHelper::deserialize(&package->msg, str, offset);
  }
};

template <>
class Variant<protocol::Broadcast>
    : public TypedVariantBase<protocol::Broadcast> {
 public:
  Variant(protocol::Broadcast* broadcast, bool cleanup = false)
      : TypedVariantBase<protocol::Broadcast>(broadcast,
                                                            cleanup) {}
  void serializeTo(std::string& str) override {
    int offset = 0;
    package->header.serializeTo(str, offset);
    SerializeHelper::serialize(&package->from, str, offset);
    SerializeHelper::serialize(&package->msg, str, offset);
  }
  void deserializeFrom(const std::string& str) override {
    int offset = 0;
    package->header.deserializeFrom(str, offset);
    SerializeHelper::deserialize(&package->from, str, offset);
    SerializeHelper::deserialize(&package->msg, str, offset);
  }
};

template <>
class Variant<protocol::TimeSync>
    : public TypedVariantBase<protocol::TimeSync> {
 public:
  Variant(protocol::TimeSync* timeSync, bool cleanup = false)
      : TypedVariantBase<protocol::TimeSync>(timeSync, cleanup) {}
  void serializeTo(std::string& str) override {
    int offset = 0;
    package->header.serializeTo(str, offset);
    SerializeHelper::serialize(&package->from, str, offset);
    SerializeHelper::serialize(&package->msg, str, offset);
  }
  void deserializeFrom(const std::string& str) override {
    int offset = 0;
    package->header.deserializeFrom(str, offset);
    SerializeHelper::deserialize(&package->from, str, offset);

    static_assert(std::is_pod<protocol::time_sync_msg_t>::value, "Is not Pod");
    SerializeHelper::deserialize(&package->msg, str, offset);
  }
};

// template <class T, typename = void>
// struct get_dest {
//   uint32_t dest(T* package) { return 0; }
// };

// template <class T>
// struct get_dest<T,
//                 typename std::enable_if<std::is_same<
//                     uint32_t,
//                     decltype(std::declval<T>().dest)>::value>::type> {
//   uint32_t dest(T* package) { return package->dest; }
// };

// template <class T, typename = void>
// struct get_routing {
//   router::Type routing(T* package) {
//     auto type = package->packageType();
//     if (type == SINGLE || type == TIME_DELAY) return router::SINGLE;
//     if (type == BROADCAST) return router::BROADCAST;
//     if (type == NODE_SYNC_REQUEST || type == NODE_SYNC_REPLY ||
//         type == TIME_SYNC)
//       return router::NEIGHBOUR;
//     return router::ROUTING_ERROR;
//   }
// };

// template <class T>
// struct get_routing<
//     T, typename std::enable_if<std::is_same<
//            uint32_t, decltype(std::declval<T>().routing)>::value>::type> {
//   router::Type routing(T* package) { return package->routing; }
// };

}  // namespace painlessmesh
#endif