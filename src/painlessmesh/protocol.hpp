#ifndef _PAINLESS_MESH_PROTOCOL_HPP_
#define _PAINLESS_MESH_PROTOCOL_HPP_

#include <cmath>
#include <functional>
#include <list>
#include <unordered_map>

#include "Arduino.h"
#include "painlessmesh/configuration.hpp"

namespace painlessmesh {

namespace router {

/** Different ways to route packages
 *
 * NEIGHBOUR packages are send to the neighbour and will be immediately handled
 * there. The TIME_SYNC and NODE_SYNC packages are NEIGHBOUR. SINGLE messages
 * are meant for a specific node. When another node receives this message, it
 * will look in its routing information and send it on to the correct node,
 * withouth processing the message in any other way. Only the targetted node
 * will actually parse/handle this message (without sending it on). Finally,
 * BROADCAST message are send to every node and processed/handled by every node.
 * */
enum Type { ROUTING_ERROR = -1, NEIGHBOUR, SINGLE, BROADCAST };
}  // namespace router

namespace protocol {

enum Type : uint16_t {
  TIME_DELAY = 3,
  TIME_SYNC = 4,
  NODE_SYNC_REQUEST = 5,
  NODE_SYNC_REPLY = 6,
  BROADCAST = 8,  // application data for everyone
  SINGLE = 9      // application data for a single node,
};

enum TimeType {
  TIME_SYNC_ERROR = -1,
  TIME_SYNC_REQUEST,
  TIME_REQUEST,
  TIME_REPLY
};

class PackageInterface {
 protected:
  uint16_t type;
  // uint32_t dest = 0;
  PackageInterface(Type type) : type(type) {}
  // PackageInterface(Type type, uint32_t dest) : type(type), dest(dest) {}

 public:
  virtual uint32_t size() = 0;
  uint16_t packageType() { return type; }
};

/**
 * Single package
 *
 * Message send to a specific node
 */
class Single : public PackageInterface {
 public:
  uint32_t dest;
  uint32_t from;
  std::string msg = "";

  Single() : PackageInterface(SINGLE) {}

  Single(uint32_t fromID, uint32_t destID, std::string& message)
      : PackageInterface(SINGLE) {
    from = fromID;
    msg = message;
  }

  uint32_t size() override {
    return sizeof(type) + sizeof(from) + sizeof(dest) + msg.length();
  }
};

/**
 * Broadcast package
 */
class Broadcast : public Single {
 public:
  using Single::Single;

  Broadcast() { type = BROADCAST; }

  Broadcast(uint32_t fromID, std::string& message) : Broadcast() {
    from = fromID;
    dest = 0;
    msg = message;
  }
};

class NodeTree : public PackageInterface {
 public:
  uint32_t nodeId = 0;
  bool root = false;
  std::list<NodeTree> subs;

  NodeTree(Type type) : PackageInterface(type) {}

  NodeTree(uint32_t nodeID, bool iAmRoot, std::list<NodeTree> subsList,
           Type type)
      : PackageInterface(type) {
    nodeId = nodeID;
    root = iAmRoot;
    subs = subsList;
  }

  bool operator==(const NodeTree& b) const {
    if (!(this->nodeId == b.nodeId && this->root == b.root &&
          this->subs.size() == b.subs.size()))
      return false;
    auto itA = this->subs.begin();
    auto itB = b.subs.begin();
    for (size_t i = 0; i < this->subs.size(); ++i) {
      if ((*itA) != (*itB)) {
        return false;
      }
      ++itA;
      ++itB;
    }
    return true;
  }

  bool operator!=(const NodeTree& b) const { return !this->operator==(b); }

  TSTRING toString(bool pretty = false);

  uint32_t size() override {
    return sizeof(nodeId) + sizeof(root) + subs.size();
  }

  void clear() {
    nodeId = 0;
    subs.clear();
    root = false;
  }
};

class NodeSync : public NodeTree {
 public:
  uint32_t from;
  uint32_t dest;

  NodeSync(Type type) : NodeTree(type) {}
  NodeSync(uint32_t fromID, uint32_t destID, std::list<NodeTree> subTree,
           bool iAmRoot, Type type)
      : NodeSync(type) {
    from = fromID;
    dest = destID;
    subs = subTree;
    nodeId = fromID;
    root = iAmRoot;
  }

  bool operator==(const NodeSync& b) const {
    if (!(this->from == b.from && this->dest == b.dest)) return false;
    return NodeTree::operator==(b);
  }

  bool operator!=(const NodeSync& b) const { return !this->operator==(b); }

  uint32_t size() override {
    return NodeTree::size() + sizeof(from) + sizeof(dest);
  }
};

/**
 * NodeSyncRequest package
 */
class NodeSyncRequest : public NodeSync {
 public:
  NodeSyncRequest() : NodeSync(NODE_SYNC_REQUEST) {}
  NodeSyncRequest(uint32_t fromID, uint32_t destID, std::list<NodeTree> subTree,
                  bool iAmRoot = false)
      : NodeSync(fromID, destID, subTree, iAmRoot, NODE_SYNC_REQUEST) {}
};

/**
 * NodeSyncReply package
 */
class NodeSyncReply : public NodeSync {
 public:
  NodeSyncReply() : NodeSync(NODE_SYNC_REPLY) {}
  NodeSyncReply(uint32_t fromID, uint32_t destID, std::list<NodeTree> subTree,
                bool iAmRoot = false)
      : NodeSync(fromID, destID, subTree, iAmRoot, NODE_SYNC_REPLY) {}
};

struct time_sync_msg_t {
  int type = TIME_SYNC_ERROR;
  uint32_t t0 = 0;
  uint32_t t1 = 0;
  uint32_t t2 = 0;
} __attribute__((packed));

/**
 * TimeSync package
 */
class TimeSync : public PackageInterface {
 public:
  uint32_t dest;
  uint32_t from;
  time_sync_msg_t msg;

  TimeSync() : PackageInterface(TIME_SYNC) {}

  TimeSync(uint32_t fromID, uint32_t destID) : TimeSync() {
    from = fromID;
    dest = destID;
    msg.type = TIME_SYNC_REQUEST;
  }

  TimeSync(uint32_t fromID, uint32_t destID, uint32_t t0) : TimeSync() {
    from = fromID;
    dest = destID;
    msg.type = TIME_REQUEST;
    msg.t0 = t0;
  }

  TimeSync(uint32_t fromID, uint32_t destID, uint32_t t0, uint32_t t1)
      : TimeSync() {
    from = fromID;
    dest = destID;
    msg.type = TIME_REPLY;
    msg.t0 = t0;
    msg.t1 = t1;
  }

  TimeSync(uint32_t fromID, uint32_t destID, uint32_t t0, uint32_t t1,
           uint32_t t2)
      : TimeSync() {
    from = fromID;
    dest = destID;
    msg.type = TIME_REPLY;
    msg.t0 = t0;
    msg.t1 = t1;
    msg.t2 = t2;
  }

  /**
   * Create a reply to the current message with the new time set
   */
  void reply(uint32_t newT0) {
    msg.t0 = newT0;
    ++msg.type;
    std::swap(from, dest);
  }

  /**
   * Create a reply to the current message with the new time set
   */
  void reply(uint32_t newT1, uint32_t newT2) {
    msg.t1 = newT1;
    msg.t2 = newT2;
    ++msg.type;
    std::swap(from, dest);
  }

  uint32_t size() override { return sizeof(dest) + sizeof(from) + sizeof(msg); }
};

/**
 * TimeDelay package
 */
class TimeDelay : public TimeSync {
 public:
  int type = TIME_DELAY;
  using TimeSync::TimeSync;
};

template <class T, typename = void>
struct get_dest {
  uint32_t dest(T* package) { return 0; }
};

template <class T>
struct get_dest<T,
                typename std::enable_if<std::is_same<
                    uint32_t, decltype(std::declval<T>().dest)>::value>::type> {
  uint32_t dest(T* package) { return package->dest; }
};

template <class T, typename = void>
struct get_routing {
  router::Type routing(T* package) {
    auto type = package->packageType();
    if (type == SINGLE || type == TIME_DELAY) return router::SINGLE;
    if (type == BROADCAST) return router::BROADCAST;
    if (type == NODE_SYNC_REQUEST || type == NODE_SYNC_REPLY ||
        type == TIME_SYNC)
      return router::NEIGHBOUR;
    return router::ROUTING_ERROR;
  }
};

template <class T>
struct get_routing<
    T, typename std::enable_if<std::is_same<
           uint32_t, decltype(std::declval<T>().routing)>::value>::type> {
  router::Type routing(T* package) { return package->routing; }
};

class VariantBase {};

/**
 * Can store any package variant
 *
 * Internally stores packages as a JsonObject. Main use case is to convert
 * different packages from and to Json (using ArduinoJson).
 */
template <typename T>
class Variant : public VariantBase {
 public:
  T* package;
  /**
   * Create Variant object from any package implementing PackageInterface
   */
  // Variant(const PackageInterface* pkg) { package =
  // std::unique_ptr<PackageInterface>(pkg); }

  Variant(T& single) { package = &single; }

  /**
   * Whether this package is of the given type
   */
  inline bool is() { return false; }

  /**
   * Convert Variant to the given type
   */
  inline T* to() { return package; }

  /**
   * Return package type
   */
  int type() { return package->packageType(); }

  /**
   * Package routing method
   */
  router::Type routing() { return get_routing<T>::routing(package); }

  /**
   * Destination node of the package
   */
  uint32_t dest() { return get_dest<T>::dest(package); }

  /**
   * Print a variant to a string
   *
   * @return A json representation of the string
   */
  void serializeTo(std::string& str) {}
  void deserializeFrom(const std::string& str) {}
};

template <>
class Variant<Single> {
  void serializeTo(std::string& str) {}
  void deserializeFrom(const std::string& str) {}
};

// template <>
// inline bool Variant::is<Single>() {
//   return package->packageType() == SINGLE;
// }

// template <>
// inline bool Variant::is<Broadcast>() {
//   return package->packageType() == BROADCAST;
// }

// template <>
// inline bool Variant::is<NodeSyncReply>() {
//   return package->packageType() == NODE_SYNC_REPLY;
// }

// template <>
// inline bool Variant::is<NodeSyncRequest>() {
//   return package->packageType() == NODE_SYNC_REQUEST;
// }

// template <>
// inline bool Variant::is<TimeSync>() {
//   return package->packageType() == TIME_SYNC;
// }

// template <>
// inline bool Variant::is<TimeDelay>() {
//   return package->packageType() == TIME_DELAY;
// }

// inline TSTRING NodeTree::toString(bool pretty) {
//   TSTRING str;
//   auto variant = Variant(*this);
//   variant.printTo(str, pretty);
//   return str;
// }

// typedef PackageInterface* (*create_ptr)();
class PackageTypeProvider {
 public:
  template <class T>
  static void add(int typeId) {
    map.emplace(typeId, []() { return new T(); });
  };

  static PackageInterface* get(int typeId) { return map[typeId](); }

 protected:
  static std::unordered_map<int, std::function<PackageInterface*()>> map;
};

void test() { PackageTypeProvider::add<Single>(10); }

}  // namespace protocol
}  // namespace painlessmesh
#endif
