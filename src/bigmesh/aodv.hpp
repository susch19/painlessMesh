#ifndef _BIGMESH_AODV_HPP_
#define _BIGMESH_AODV_HPP_

#include <list>

#include "painlessmesh/configuration.hpp"

#include "painlessmesh/plugin.hpp"

namespace bigmesh {
namespace aodv {
class MessageID {
 public:
  MessageID() {}
  MessageID(size_t id) : id(id) {}

  MessageID(JsonObject jsonObj) { this->id = jsonObj["mid"]; }

  bool update(const MessageID &mid) {
    if (this->operator<(mid)) {
      id = mid.id;
      return true;
    }
    return false;
  }

  bool operator<(const MessageID &mid) const {
    if (id == 0) return true;
    return (int)id - (int)mid.id < 0;
  }

  bool operator==(const MessageID &mid) const { return id == mid.id; }

  MessageID &operator++() {
    ++id;
    if (id == 0) ++id;
    return *this;
  }

  size_t value() const { return id; };

  JsonObject addTo(JsonObject &&jsonObj) const {
    jsonObj["mid"] = id;
    return jsonObj;
  }

 protected:
  size_t id = 0;
};

class BroadcastBasePackage : public painlessmesh::protocol::PackageInterface {
 public:
  painlessmesh::router::Type routing;
  int type;
  int noJsonFields = 4;

  uint32_t from = 0;
  MessageID mid;

  BroadcastBasePackage(int type)
      : routing(painlessmesh::router::BROADCAST), type(type) {}

  BroadcastBasePackage(JsonObject jsonObj) {
    type = jsonObj["type"];
    from = jsonObj["from"];
    mid = MessageID(jsonObj);

    routing =
        static_cast<painlessmesh::router::Type>(jsonObj["routing"].as<int>());
  }

  JsonObject addTo(JsonObject &&jsonObj) const {
    jsonObj["type"] = type;
    jsonObj["from"] = from;
    jsonObj["routing"] = static_cast<int>(routing);
    return mid.addTo(std::move(jsonObj));
  }

  size_t jsonObjectSize() const { return JSON_OBJECT_SIZE(noJsonFields); }
};

class Mesh {
 protected:
  MessageID broadcastID;
  std::map<uint32_t, MessageID> broadcastIDMap;
};
};  // namespace aodv

namespace protocol {
class Variant : public painlessmesh::protocol::Variant {
 public:
  using painlessmesh::protocol::Variant::Variant;
  aodv::MessageID mid() {
    if (jsonObj.containsKey("mid"))
      return aodv::MessageID(jsonObj["mid"].as<size_t>());
    return aodv::MessageID();
  }
};
};  // namespace protocol
};  // namespace bigmesh

#endif
