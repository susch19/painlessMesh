#ifndef _BIGMESH_MESHID_HPP_
#define _BIGMESH_MESHID_HPP_

#include "painlessmesh/plugin.hpp"

#include "bigmesh/aodv.hpp"

namespace bigmesh {
namespace meshid {
class ID {
 public:
  int originalID = 0;
  int id = 0;
  uint32_t source = 0;
  uint32_t weight = 0;
};

// Note that highest ID always wins
class Package : public painlessmesh::protocol::PackageInterface {
 public:
  painlessmesh::router::Type routing;
  int type;
  int noJsonFields = 6;

  int id = 0;  // Highest is 255, reserved for rooted nodes
  int size = 0;

  // Should be the id of the direct neighbour this originated from
  uint32_t from = 0;
  uint32_t weight = 0;

  Package(int type) : routing(painlessmesh::router::NEIGHBOUR), type(type) {}

  Package(JsonObject jsonObj) {
    id = jsonObj["id"];
    size = jsonObj["size"];
    type = jsonObj["type"];
    from = jsonObj["from"];
    weight = jsonObj["weight"];

    routing =
        static_cast<painlessmesh::router::Type>(jsonObj["routing"].as<int>());
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

class Mesh {
 public:
  size_t meshID() { return mMeshID.id; }

 protected:
  meshid::ID mMeshID;
  std::shared_ptr<Task> idTask;
};

template <typename T>
void initialize(T* mesh_ptr) {
  mesh_ptr->mMeshID.originalID = runif(1, 240);
  mesh_ptr->mMeshID.weight = mesh_ptr->nodeId;
  if (mesh_ptr->isRoot()) {
    mesh_ptr->mMeshID.originalID = 255;
    mesh_ptr->mMeshID.weight = 0 - 1;  // max value
  }
  mesh_ptr->mMeshID.id = mesh_ptr->mMeshID.originalID;
  mesh_ptr->mMeshID.source = mesh_ptr->nodeId;

  mesh_ptr->idTask =
      mesh_ptr->addTask(10 * TASK_SECOND, TASK_FOREVER, [mesh_ptr]() {
        // idTask = mesh_ptr->addTask(10, TASK_FOREVER, [mesh_ptr]() {
        auto pkg = meshid::Package(1);
        pkg.id = mesh_ptr->mMeshID.id;
        pkg.from = mesh_ptr->nodeId;
        pkg.weight = mesh_ptr->mMeshID.weight;
        mesh_ptr->sendPackage(&pkg);
      });

  mesh_ptr->onPackage(
      1, [mesh_ptr](painlessmesh::protocol::Variant var,
                    std::shared_ptr<bigmesh::Connection> connection,
                    uint32_t arrivaltime) {
        auto pkg = var.to<bigmesh::meshid::Package>();

        // If node id of the connection was unset then set it now
        if (connection->nodeId == 0) {
          connection->nodeId = pkg.from;
          mesh_ptr->newConnectionCallbacks.execute(pkg.from);
        }

        // Did anything change?
        if (pkg.weight != mesh_ptr->mMeshID.weight) {
          if (pkg.from == mesh_ptr->mMeshID.source) {
            // Something changed upstream
            if (pkg.weight > mesh_ptr->nodeId) {
              mesh_ptr->mMeshID.id = pkg.id;
              mesh_ptr->mMeshID.weight = pkg.weight;
            } else {
              // They lost their id rights, so now I am the boss again
              // Until I hear differently
              mesh_ptr->mMeshID.id = mesh_ptr->mMeshID.originalID;
              mesh_ptr->mMeshID.source = mesh_ptr->nodeId;
              mesh_ptr->mMeshID.weight = mesh_ptr->nodeId;
            }
          } else if (pkg.weight > mesh_ptr->mMeshID.weight) {
            mesh_ptr->mMeshID.id = pkg.id;
            mesh_ptr->mMeshID.source = pkg.from;
            mesh_ptr->mMeshID.weight = pkg.weight;
          }
          mesh_ptr->idTask->forceNextIteration();
        }

        return false;
      });

  mesh_ptr->onUnknownConnection(
      [mesh_ptr]() { mesh_ptr->idTask->forceNextIteration(); });

  // Handler for broken connections
  mesh_ptr->onDroppedConnection([mesh_ptr](auto nodeId) {
    if (nodeId == mesh_ptr->mMeshID.source) {
      mesh_ptr->mMeshID.id = mesh_ptr->mMeshID.originalID;
      mesh_ptr->mMeshID.source = mesh_ptr->nodeId;
      mesh_ptr->mMeshID.weight = mesh_ptr->nodeId;
      mesh_ptr->idTask->forceNextIteration();
    }
  });
}
};  // namespace meshid
};  // namespace bigmesh

#endif
