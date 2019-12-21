#define CATCH_CONFIG_MAIN

#include "catch2/catch.hpp"

#include "Arduino.h"

#include "catch_utils.hpp"

#include "boost/asynctcp.hpp"

WiFiClass WiFi;
ESPClass ESP;

#include "bigmesh/mesh.hpp"
#include "bigmesh/meshid.hpp"

#include "painlessmesh/connection.hpp"

using PMesh = bigmesh::Mesh<bigmesh::Connection>;

using namespace painlessmesh;
painlessmesh::logger::LogClass Log;

class MeshTest : public PMesh {
 public:
  MeshTest(Scheduler *scheduler, size_t id, boost::asio::io_service &io)
      : io_service(io) {
    this->nodeId = id;
    this->init(scheduler, this->nodeId);

    pServer = std::make_shared<AsyncServer>(io_service, this->nodeId);
    painlessmesh::tcp::initServer<bigmesh::Connection, PMesh>(*pServer,
                                                              (*this));

    this->meshId.originalID = runif(1, 254);
    if (this->isRoot()) {
      this->meshId.originalID = 255;
    }
    this->meshId.id = this->meshId.originalID;
    this->meshId.source = this->nodeId;

    idTask = this->addTask(10 * TASK_SECOND, TASK_FOREVER, [this]() {
      auto pkg = bigmesh::MeshIDPackage(33);
      pkg.id = this->meshId.id;
      pkg.from = this->nodeId;
      this->sendPackage(&pkg);
    });

    this->onPackage(33, [this](painlessmesh::protocol::Variant var,
                               std::shared_ptr<bigmesh::Connection> connection,
                               uint32_t arrivaltime) {
      auto pkg = var.to<bigmesh::MeshIDPackage>();

      // If node id of the connection was unset then set it now
      if (connection->nodeId == 0) {
        connection->nodeId = pkg.from;
        this->newConnectionCallbacks.execute(pkg.from);
      }

      if (this->meshId.id != pkg.id) {
        if (connection->station &&
            (this->meshId.id != pkg.id || this->meshId.source != pkg.from)) {
          this->meshId.id = pkg.id;
          this->meshId.source = pkg.from;
          this->idTask->forceNextIteration();
        }
        this->idTask->forceNextIteration();
      }
      return false;
    });

    this->onUnknownConnection([this]() { this->idTask->forceNextIteration(); });

    // Handler for broken connections
    this->onDroppedConnection([this](auto nodeId) {
      if (nodeId == this->meshId.source) {
        this->meshId.id = this->meshId.originalID;
        this->meshId.source = this->nodeId;
        this->idTask->forceNextIteration();
      }
    });
  }

  void connect(MeshTest &mesh) {
    auto pClient = new AsyncClient(io_service);
    painlessmesh::tcp::connect<bigmesh::Connection, PMesh>(
        (*pClient), boost::asio::ip::address::from_string("127.0.0.1"),
        mesh.nodeId, (*this));
  }

  bigmesh::MeshID meshId;
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
  void execute() {
    for (auto &&m : nodes) {
      m->execute();
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

/** For routing we need a broadcast id, whenever a higher id is received,
 * set current id to that.
 * Ignore any lower values.. Note that best to use uint, but with 10 > max,
 * to adjust for max+1 == 0
 * Probably also need to use 0 as an invalid initial ID, so we dont start by
 * disregarding all broadcasts
 *
 * We need RouteRequest broadcast, which when forwarded will at last step to
 * track.
 *
 * Also we need a RouteError message, which is send back when route is out
 * of date
 *
 * When scanning for new meshes, really should take into account how often
 * I've seen the other nodes, only when detected couple of times switch over.
 * Also might want to connect to anything if not connected to anything
 *
 * We need a on mesh id change callback, that will change the mac id
 *
 * Time sync will need to be changed to accept the time from the source, not
 * anyone else
 *
 * Stability could measure how stable the mesh id is
 *
 * How to deal with time outs?
 */

SCENARIO("The MeshTest class works correctly") {
  using namespace logger;
  Scheduler scheduler;
  Log.setLogLevel(ERROR);
  boost::asio::io_service io_service;

  MeshTest mesh1(&scheduler, 6841, io_service);
  MeshTest mesh2(&scheduler, 6842, io_service);
  mesh2.connect(mesh1);

  for (auto i = 0; i < 100000; ++i) {
    mesh1.execute();
    mesh2.execute();
    io_service.poll();
  }

  REQUIRE(mesh1.meshId.id == mesh2.meshId.id);
}

SCENARIO("Mesh ID is correctly passed around") {
  delay(1000);
  using namespace logger;
  Log.setLogLevel(ERROR);

  Scheduler scheduler;
  boost::asio::io_service io_service;
  Nodes n(&scheduler, 12, io_service);

  for (auto i = 0; i < 1000; ++i) {
    n.execute();
    delay(10);
  }

  auto id = n.nodes[11]->meshId.id;
  for (auto &&node : n.nodes) {
    if (node->subs.size() > 0) REQUIRE((*node->subs.begin())->nodeId != 0);
    REQUIRE(node->meshId.id == id);
  }

  // Disconnect
  auto nid = runif(0, 11);
  (*n.nodes[nid]->subs.begin())->close();
  for (auto i = 0; i < 1000; ++i) {
    n.execute();
    delay(10);
  }
  std::map<uint32_t, int> m;
  for (auto &&node : n.nodes) {
    auto i = node->meshId.id;
    if (m.count(i) == 0) m[i] = 0;
    ++m[i];
  }
  // There should be two meshes now
  REQUIRE(m.size() == 2);
}
