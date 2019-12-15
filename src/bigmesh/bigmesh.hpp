#ifndef _BIGMESH_HPP_
#define _BIGMESH_HPP_

namespace bigmesh {

struct BigMeshStorage {
  std::map<uint32_t, int> sizes;
  int originalID;
  int id;
  uint32_t source = 0;
  uint32_t weight = 0;

  int size() {
    int s = 1;
    for (auto& kv : sizes)
      s += kv.second;
    return s;
  }
};

// Note that highest ID always wins
class MeshIDPackage : public painlessmesh::protocol::PackageInterface {
 public:
  painlessmesh::router::Type routing;
  int type;
  int noJsonFields = 6;

  int id = 0;  // Highest is 255, reserved for rooted nodes 
  int size = 0;

  // Should be the id of the direct neighbour this originated from
  uint32_t from = 0;
  uint32_t weight = 0;

  MeshIDPackage(int type) : routing(painlessmesh::router::NEIGHBOUR), type(type) {}

  MeshIDPackage(JsonObject jsonObj) {
    id = jsonObj["id"];
    size = jsonObj["size"];
    type = jsonObj["type"];
    from = jsonObj["from"];
    weight = jsonObj["weight"];
    routing = static_cast<painlessmesh::router::Type>(jsonObj["routing"].as<int>());
  }

  JsonObject addTo(JsonObject&& jsonObj) const {
    jsonObj["id"] = id;
    jsonObj["size"] = size;
    jsonObj["routing"] = static_cast<int>(routing);
    jsonObj["type"] = type;
    jsonObj["from"] = from;
    jsonObj["weight"] = weight;
    return jsonObj;
  }


  size_t jsonObjectSize() const { return JSON_OBJECT_SIZE(noJsonFields); }
};
};  // namespace bigmesh

#endif
