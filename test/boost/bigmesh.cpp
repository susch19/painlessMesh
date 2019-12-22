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

class MeshTest : public PMesh, public bigmesh::meshid::Mesh {
 public:
  MeshTest(Scheduler *scheduler, size_t id, boost::asio::io_service &io)
      : io_service(io) {
    this->nodeId = id;
    this->initialize(scheduler, this->nodeId);

    pServer = std::make_shared<AsyncServer>(io_service, this->nodeId);
    painlessmesh::tcp::initServer<bigmesh::Connection, PMesh>(*pServer,
                                                              (*this));

    bigmesh::meshid::initialize(this);
  }

  void connect(MeshTest &mesh) {
    auto pClient = new AsyncClient(io_service);
    painlessmesh::tcp::connect<bigmesh::Connection, PMesh>(
        (*pClient), boost::asio::ip::address::from_string("127.0.0.1"),
        mesh.nodeId, (*this));
  }

  std::shared_ptr<AsyncServer> pServer;
  boost::asio::io_service &io_service;

  friend void bigmesh::meshid::initialize<MeshTest>(MeshTest *);
};

class Nodes {
 public:
  Nodes(Scheduler *scheduler, size_t n, boost::asio::io_service &io,
        bool loop = true)
      : io_service(io) {
    size_t j = n + 1;
    if (loop) j = runif(1, n - 1);
    for (size_t i = 0; i < n; ++i) {
      auto m = std::make_shared<MeshTest>(scheduler, i + baseID, io_service);
      if (i > 0) m->connect((*nodes[runif(0, i - 1)]));
      nodes.push_back(m);

      if (i == j) nodes[0]->connect((*nodes[runif(1, i)]));
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
 *
 * There is a corner case, when the node with the highest nodeid (weight) is the
 * only one that can bridge two separate networks (and has a low meshid). It
 * could be permanently switching, because once it leaves one submesh, that will
 * take a higher meshID and this node will impose a lower meshID on the other
 * submesh, causing it to switch back again and this keeps repeating. To
 * overcome this we need to add a corner case, where when the node whose
 * nodeid==meshid.weight switches we set its originalID to the other ids+1 (we
 * might need a bit of think about what happens if the other one has id 254 (255
 * is reserved), maybe we should limit it to 240 by default, that gives us some
 * leeway.
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

  REQUIRE(mesh1.meshID() == mesh2.meshID());
}

SCENARIO("Mesh ID is correctly passed around") {
  delay(1000);
  using namespace logger;
  Log.setLogLevel(ERROR);

  Scheduler scheduler;
  boost::asio::io_service io_service;
  Nodes n(&scheduler, 12, io_service, false);

  for (auto i = 0; i < 1000; ++i) {
    n.execute();
    delay(10);
  }

  auto id = n.nodes[11]->meshID();
  for (auto &&node : n.nodes) {
    if (node->subs.size() > 0) REQUIRE((*node->subs.begin())->nodeId != 0);
    REQUIRE(node->meshID() == id);
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
    auto i = node->meshID();
    if (m.count(i) == 0) m[i] = 0;
    ++m[i];
  }
  // There should be two meshes now
  REQUIRE(m.size() == 2);
}

SCENARIO("Mesh ID is correctly passed around in looped nodes") {
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

  auto id = n.nodes[11]->meshID();
  for (auto &&node : n.nodes) {
    if (node->subs.size() > 0) REQUIRE((*node->subs.begin())->nodeId != 0);
    REQUIRE(node->meshID() == id);
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
    auto i = node->meshID();
    if (m.count(i) == 0) m[i] = 0;
    ++m[i];
  }
  // There should be one or two meshes now
  REQUIRE(m.size() <= 2);

  auto nid2 = runif(0, 11);
  while (nid2 == nid) nid2 = runif(0, 11);

  (*n.nodes[nid2]->subs.begin())->close();
  for (auto i = 0; i < 1000; ++i) {
    n.execute();
    delay(10);
  }
  m.clear();
  for (auto &&node : n.nodes) {
    auto i = node->meshID();
    if (m.count(i) == 0) m[i] = 0;
    ++m[i];
  }
  // There should be two or three meshes now
  REQUIRE(m.size() >= 2);
  REQUIRE(m.size() <= 3);
}

SCENARIO("Broadcast is working") {
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

  auto pkg = bigmesh::aodv::BroadcastBasePackage(2);
  n.nodes[0]->sendPackage(&pkg);

  for (auto i = 0; i < 1000; ++i) {
    n.execute();
    delay(10);
  }

  auto pkg2 = bigmesh::aodv::BroadcastBasePackage(2);
  n.nodes[0]->sendPackage(&pkg2);

  for (auto i = 0; i < 1000; ++i) {
    n.execute();
    delay(10);
  }

  auto pkg3 = bigmesh::aodv::BroadcastBasePackage(2);
  auto nid = runif(0, 11);
  n.nodes[nid]->sendPackage(&pkg3);

  for (auto i = 0; i < 1000; ++i) {
    n.execute();
    delay(10);
  }
}
