#define CATCH_CONFIG_MAIN

#include "catch2/catch.hpp"

#include "Arduino.h"

#include "catch_utils.hpp"

#include "boost/asynctcp.hpp"

WiFiClass WiFi;
ESPClass ESP;

#include "painlessMeshConnection.h"

#include "painlessmesh/mesh.hpp"

#include "bigmesh/bigmesh.hpp"

using PMesh = painlessmesh::Mesh<MeshConnection>;

using namespace painlessmesh;
painlessmesh::logger::LogClass Log;

class MeshTest : public PMesh {
 public:
  MeshTest(Scheduler *scheduler, size_t id, boost::asio::io_service &io)
      : io_service(io) {
    this->nodeId = id;
    this->init(scheduler, this->nodeId);
    timeOffset = runif(0, 1e09);
    pServer = std::make_shared<AsyncServer>(io_service, this->nodeId);
    painlessmesh::tcp::initServer<MeshConnection, PMesh>(*pServer, (*this));

    this->bmStorage.originalID = runif(1, 254);
    this->bmStorage.weight = this->nodeId;
    if (this->isRoot()) {
      this->bmStorage.originalID = 255;
      this->bmStorage.weight = 0 - 1;  // Max value
    }
    this->bmStorage.id = this->bmStorage.originalID;
    this->bmStorage.source = this->nodeId;

    idTask = this->addTask(10 * TASK_SECOND, TASK_FOREVER, [this]() {
      auto pkg = bigmesh::MeshIDPackage(33);
      pkg.id = this->bmStorage.id;
      pkg.weight = this->bmStorage.weight;

      pkg.from = this->nodeId;
      this->sendPackage(&pkg);
    });

    this->onPackage(33, [this](painlessmesh::protocol::Variant var) {
      auto pkg = var.to<bigmesh::MeshIDPackage>();
      if (pkg.from == this->bmStorage.source &&
          pkg.weight != this->bmStorage.weight) {
        // Something changed upstream
        if (pkg.weight > this->nodeId) {
          this->bmStorage.id = pkg.id;
          this->bmStorage.weight = pkg.weight;
        } else {
          // They lost their id rights, so now I am the boss again
          // Until I hear differently
          this->bmStorage.id = this->bmStorage.originalID;
          this->bmStorage.source = this->nodeId;
          this->bmStorage.weight = this->nodeId;
        }
        this->idTask->forceNextIteration();
      } else {
        if (pkg.weight > this->bmStorage.weight) {
          this->bmStorage.id = pkg.id;
          this->bmStorage.source = pkg.from;
          this->bmStorage.weight = pkg.weight;
          this->idTask->forceNextIteration();
        }
      }
      return false;
    });

    // Add handler for new connections
    this->onNewConnection(
        [this](auto nodeId) { this->idTask->forceNextIteration(); });

    // Handler for broken connections
    this->onDroppedConnection([this](auto nodeId) {
      if (nodeId == this->bmStorage.source) {
        this->bmStorage.id = this->bmStorage.originalID;
        this->bmStorage.source = this->nodeId;
        this->bmStorage.weight = this->nodeId;
        this->idTask->forceNextIteration();
      }
    });
  }

  void connect(MeshTest &mesh) {
    auto pClient = new AsyncClient(io_service);
    painlessmesh::tcp::connect<MeshConnection, PMesh>(
        (*pClient), boost::asio::ip::address::from_string("127.0.0.1"),
        mesh.nodeId, (*this));
  }

  bigmesh::BigMeshStorage bmStorage;
  std::shared_ptr<Task> idTask;
  std::shared_ptr<AsyncServer> pServer;
  boost::asio::io_service &io_service;
};

class Nodes {
 public:
  Nodes(Scheduler *scheduler, size_t n, boost::asio::io_service &io)
      : io_service(io) {
    for (size_t i = 0; i < n; ++i) {
      auto m = std::make_shared<MeshTest>(scheduler, i + baseID, io_service);
      if (i > 0) m->connect((*nodes[runif(0, i - 1)]));
      nodes.push_back(m);
    }
  }
  void update() {
    for (auto &&m : nodes) {
      m->update();
      io_service.poll();
    }
  }

  void stop() {
    for (auto &&m : nodes) m->stop();
  }

  auto size() { return nodes.size(); }

  std::shared_ptr<MeshTest> get(size_t nodeId) {
    return nodes[nodeId - baseID];
  }

  size_t baseID = 6481;
  std::vector<std::shared_ptr<MeshTest>> nodes;
  boost::asio::io_service &io_service;
};

SCENARIO("The MeshTest class works correctly") {
  using namespace logger;
  Scheduler scheduler;
  Log.setLogLevel(ERROR);
  boost::asio::io_service io_service;

  MeshTest mesh1(&scheduler, 6841, io_service);
  MeshTest mesh2(&scheduler, 6842, io_service);
  mesh2.connect(mesh1);

  for (auto i = 0; i < 100000; ++i) {
    mesh1.update();
    mesh2.update();
    io_service.poll();
  }

  REQUIRE(mesh1.bmStorage.id == mesh2.bmStorage.id);
}
