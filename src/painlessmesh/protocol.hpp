#ifndef _PAINLESS_MESH_PROTOCOL_HPP_
#define _PAINLESS_MESH_PROTOCOL_HPP_

#include <cmath>
#include <functional>
#include <list>
#include <unordered_map>

#include "Arduino.h"
#include "nodeTree.hpp"
#include "painlessmesh/configuration.hpp"
#include "serializer.hpp"
// #include "variant.hpp"

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

enum Type {
  NONE = 0,
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

struct ProtocolHeader {
  uint16_t type;
  uint16_t routing;
  uint32_t dest;

  // ProtocolHeader() {}
  // ProtocolHeader(uint16_t type) : type(type) {}
  // ProtocolHeader(uint16_t type, uint16_t routing, uint32_t dest)
  //     : type(type), routing(routing), dest(dest) {}

  void deserializeFrom(const std::string& str, int& offset) {
    SerializeHelper::deserialize(this, str, offset);
  }

  void serializeTo(std::string& str, int& offset) {
    SerializeHelper::serialize(this, str, offset);
  }

} __attribute__((packed));

class PackageInterface {
 protected:
  // uint32_t dest = 0;

  PackageInterface(ProtocolHeader header) : header(header) {}

  // PackageInterface(uint16_t type) { header.type = type; }
  PackageInterface(uint16_t type, uint16_t routing) {
    header.type = type;
    header.routing = routing;
  }

  // PackageInterface(Type type, uint32_t dest) : type(type), dest(dest) {}

 public:
  ProtocolHeader header;
  virtual uint32_t size() { return sizeof(header); };
  uint16_t packageType() { return header.type; }
  virtual ~PackageInterface() {}
};

/**
 * Single package
 *
 * Message send to a specific node
 */
class Single : public PackageInterface {
 public:
  uint32_t from;
  std::string msg = "";

  Single(ProtocolHeader header) : PackageInterface(header) {}

  Single(uint32_t fromID, uint32_t destID, std::string& message)
      : PackageInterface(SINGLE, router::SINGLE) {
    from = fromID;
    msg = message;
  }

  uint32_t size() override {
    return PackageInterface::size() + sizeof(from) + msg.length();
  }

 protected:
  Single(int type) : PackageInterface(type, router::SINGLE) {}
};

/**
 * Broadcast package
 */
class Broadcast : public Single {
 public:
  using Single::Single;

  Broadcast(ProtocolHeader header) : Single(header) {}

  Broadcast(uint32_t fromID, std::string& message) : Single(from, 0, message) {
    header.type = BROADCAST;
    header.type = router::BROADCAST;
  }
};

class NodeSync : public NodeTree, public PackageInterface {
 public:
  uint32_t from;

  NodeSync(Type type = NONE) : PackageInterface(type, router::NEIGHBOUR) {}
  NodeSync(ProtocolHeader header) : PackageInterface(header) {}
  NodeSync(uint32_t fromID, uint32_t destID, std::list<NodeTree> subTree,
           bool iAmRoot = false, Type type = NONE)
      : NodeSync(type) {
    from = fromID;
    header.dest = destID;
    subs = subTree;
    nodeId = fromID;
    root = iAmRoot;
  }

  bool operator==(const NodeSync& b) const {
    if (!(this->from == b.from && this->header.dest == b.header.dest))
      return false;
    return NodeTree::operator==(b);
  }

  bool operator!=(const NodeSync& b) const { return !this->operator==(b); }

  uint32_t size() override { return NodeTree::size() + sizeof(from); }
};

/**
 * NodeSyncRequest package
 */
class NodeSyncRequest : public NodeSync {
 public:
  // NodeSyncRequest() : NodeSync(NODE_SYNC_REQUEST) {}
  NodeSyncRequest(ProtocolHeader header) : NodeSync(header) {}
  NodeSyncRequest(uint32_t fromID, uint32_t destID, std::list<NodeTree> subTree,
                  bool iAmRoot = false)
      : NodeSync(fromID, destID, subTree, iAmRoot, NODE_SYNC_REQUEST) {}
};

/**
 * NodeSyncReply package
 */
class NodeSyncReply : public NodeSync {
 public:
  // NodeSyncReply() : NodeSync(NODE_SYNC_REPLY) {}
  NodeSyncReply(ProtocolHeader header) : NodeSync(header) {}
  NodeSyncReply(uint32_t fromID, uint32_t destID, std::list<NodeTree> subTree,
                bool iAmRoot = false)
      : NodeSync(fromID, destID, subTree, iAmRoot, NODE_SYNC_REPLY) {}
};

struct time_sync_msg_t {
  int16_t type = TIME_SYNC_ERROR;
  uint32_t t0 = 0;
  uint32_t t1 = 0;
  uint32_t t2 = 0;
} __attribute__((packed));

/**
 * TimeSync package
 */
class TimeSync : public PackageInterface {
 public:
  uint32_t from;
  time_sync_msg_t msg;

  TimeSync(ProtocolHeader header) : PackageInterface(header) {}

  TimeSync() : PackageInterface(TIME_SYNC, router::NEIGHBOUR) {}
  TimeSync(int type) : PackageInterface(type, router::NEIGHBOUR) {}

  TimeSync(uint32_t fromID, uint32_t destID) : TimeSync() {
    from = fromID;
    header.dest = destID;
    msg.type = TIME_SYNC_REQUEST;
  }

  TimeSync(uint32_t fromID, uint32_t destID, uint32_t t0) : TimeSync() {
    from = fromID;
    header.dest = destID;
    msg.type = TIME_REQUEST;
    msg.t0 = t0;
  }

  TimeSync(uint32_t fromID, uint32_t destID, uint32_t t0, uint32_t t1)
      : TimeSync() {
    from = fromID;
    header.dest = destID;
    msg.type = TIME_REPLY;
    msg.t0 = t0;
    msg.t1 = t1;
  }

  TimeSync(uint32_t fromID, uint32_t destID, uint32_t t0, uint32_t t1,
           uint32_t t2)
      : TimeSync() {
    from = fromID;
    header.dest = destID;
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

    swapFromAndDest();
  }

  /**
   * Create a reply to the current message with the new time set
   */
  void reply(uint32_t newT1, uint32_t newT2) {
    msg.t1 = newT1;
    msg.t2 = newT2;
    ++msg.type;
    swapFromAndDest();
  }

  uint32_t size() override {
    return PackageInterface::size() + sizeof(from) + sizeof(msg);
  }

 private:
  void swapFromAndDest() {
    auto dest = header.dest;
    header.dest = from;
    from = dest;
  }
};

/**
 * TimeDelay package
 */
class TimeDelay : public TimeSync {
 public:
  using TimeSync::TimeSync;

  TimeDelay() : TimeSync(TIME_DELAY) {}
  TimeDelay(ProtocolHeader header) : TimeSync(header) {}
};

}  // namespace protocol

}  // namespace painlessmesh
#endif
