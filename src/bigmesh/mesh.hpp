#ifndef _BIGMESH_MESH_HPP_
#define _BIGMESH_MESH_HPP_

#include "painlessmesh/configuration.hpp"

#include "painlessmesh/connection.hpp"
#include "painlessmesh/logger.hpp"
#include "painlessmesh/plugin.hpp"
#include "painlessmesh/tcp.hpp"

namespace bigmesh {

template <class T>
class Mesh : public painlessmesh::plugin::PackageHandler<T> {
 public:
  uint32_t nodeId = 0;

  void initialize(uint32_t id) {
    using namespace painlessmesh::logger;

    if (!isExternalScheduler) {
      mScheduler = new Scheduler();
    }
    this->nodeId = id;

#ifdef ESP32
    xSemaphore = xSemaphoreCreateMutex();
#endif

    mScheduler->enableAll();

    this->droppedConnectionCallbacks.push_back([this](uint32_t nodeId,
                                                      bool station) {
      Log(MESH_STATUS, "Dropped connection %u, station %d\n", nodeId, station);
      this->eraseClosedConnections();
    });

    this->newConnectionCallbacks.push_back([this](uint32_t nodeId) {
      Log(MESH_STATUS, "New connection %u\n", nodeId);
    });
  }

  void initialize(Scheduler *scheduler, uint32_t id) {
    this->setScheduler(scheduler);
    this->initialize(id);
  }

  /** Perform crucial maintenance task
   *
   * Add this to your loop() function. This routine runs various maintenance
   * tasks.
   */
  void execute() {
    if (semaphoreTake()) {
      mScheduler->execute();
      semaphoreGive();
    }
    return;
  }

  /**
   * Check whether this node is a root node.
   */
  bool isRoot() { return this->root; };

  bool sendPackage(painlessmesh::protocol::PackageInterface *pkg) {
    auto variant = painlessmesh::protocol::Variant(pkg);
    TSTRING msg;
    variant.printTo(msg, false);
    if (variant.routing() == painlessmesh::router::NEIGHBOUR) {
      for (auto &&sub : this->subs) {
        if (variant.dest() == 0 || variant.dest() == sub->nodeId) {
          sub->write(msg);
        }
      }
      return true;
    }

    std::cout << "sendPackage not implemented" << std::endl;
    return false;
  };

  /** Callback that gets called on any incoming TCP connection
   *
   * Note that once the node is know, onNewConnection is called and users
   * generally should subscribe to that event. The unknownConnection is meant
   * for low level usage.
   */
  void onUnknownConnection(std::function<void()> onUnknownConnection) {
    unknownConnectionCallbacks.push_back(onUnknownConnection);
  }

  /** Callback that gets called every time the local node makes a new
   * connection.
   *
   * \code
   * mesh.onNewConnection([](auto nodeId) {
   *    // Do something with the event
   *    Serial.println(String(nodeId));
   * });
   * \endcode
   */
  void onNewConnection(std::function<void(uint32_t nodeId)> onNewConnection) {
    newConnectionCallbacks.push_back([onNewConnection](uint32_t nodeId) {
      if (nodeId != 0) onNewConnection(nodeId);
    });
  }

  /** Callback that gets called every time the local node drops a connection.
   *
   * \code
   * mesh.onDroppedConnection([](auto nodeId) {
   *    // Do something with the event
   *    Serial.println(String(nodeId));
   * });
   * \endcode
   */
  void onDroppedConnection(
      std::function<void(uint32_t nodeId)> onDroppedConnection) {
    droppedConnectionCallbacks.push_back(
        [onDroppedConnection](uint32_t nodeId, bool station) {
          if (nodeId != 0) onDroppedConnection(nodeId);
        });
  }

  inline std::shared_ptr<Task> addTask(unsigned long aInterval,
                                       long aIterations,
                                       std::function<void()> aCallback) {
    return painlessmesh::plugin::PackageHandler<T>::addTask(
        (*this->mScheduler), aInterval, aIterations, aCallback);
  }

  inline std::shared_ptr<Task> addTask(std::function<void()> aCallback) {
    return painlessmesh::plugin::PackageHandler<T>::addTask((*this->mScheduler),
                                                            aCallback);
  }

  ~Mesh() {
    this->stop();
    if (!isExternalScheduler) delete mScheduler;
  }

  std::list<std::shared_ptr<T>> subs;

 protected:
  void eraseClosedConnections() {
    using namespace painlessmesh::logger;
    Log(CONNECTION, "eraseClosedConnections():\n");
    this->subs.remove_if(
        [](const std::shared_ptr<T> &conn) { return !conn->connected(); });
  }

  void setScheduler(Scheduler *baseScheduler) {
    this->mScheduler = baseScheduler;
    isExternalScheduler = true;
  }

  /**
   * Wrapper function for ESP32 semaphore function
   *
   * Waits for the semaphore to be available and then returns true
   *
   * Always return true on ESP8266
   */
  bool semaphoreTake() {
#ifdef ESP32
    return xSemaphoreTake(xSemaphore, (TickType_t)10) == pdTRUE;
#else
    return true;
#endif
  }

  /**
   * Wrapper function for ESP32 semaphore give function
   *
   * Does nothing on ESP8266 hardware
   */
  void semaphoreGive() {
#ifdef ESP32
    xSemaphoreGive(xSemaphore);
#endif
  }

  bool root = false;

  // Callback functions
  painlessmesh::callback::List<> unknownConnectionCallbacks;
  painlessmesh::callback::List<uint32_t> newConnectionCallbacks;
  painlessmesh::callback::List<uint32_t, bool> droppedConnectionCallbacks;

#ifdef ESP32
  SemaphoreHandle_t xSemaphore = NULL;
#endif

  bool isExternalScheduler = false;
  Scheduler *mScheduler;

  friend T;
  friend void painlessmesh::tcp::initServer<T, Mesh>(AsyncServer &, Mesh &);
  friend void painlessmesh::tcp::connect<T, Mesh>(AsyncClient &, IPAddress,
                                                  uint16_t, Mesh &);
};

class Connection : public painlessmesh::tcp::BufferedConnection {
 public:
  uint32_t nodeId = 0;
  Mesh<Connection> *mesh;
  bool station = false;

  Connection(AsyncClient *client, Mesh<bigmesh::Connection> *mesh, bool station)
      : painlessmesh::tcp::BufferedConnection(client),
        mesh(mesh),
        station(station) {}

  void initTasks() {
    this->initialize(mesh->mScheduler);

    this->onReceive(
        [mesh = mesh, self = this->shared_from_this()](TSTRING str) {
          auto variant = painlessmesh::protocol::Variant(str);
          mesh->callbackList.execute(variant.type(), variant, self, 0);
        });

    this->onDisconnect([mesh = mesh, self = this->shared_from_this()]() {
      mesh->addTask([mesh, nodeId = self->nodeId, station = self->station]() {
        mesh->droppedConnectionCallbacks.execute(nodeId, station);
      });
      self->nodeId = 0;
    });
    mesh->unknownConnectionCallbacks.execute();
  }

  // For compatibility with MeshConnection
  void initTCPCallbacks() {}

  bool addMessage(TSTRING msg, bool priority = false) {
    return this->write(msg, priority);
  }

 protected:
  std::shared_ptr<Connection> shared_from_this() { return shared_from(this); }
};  // namespace bigmesh
};  // namespace bigmesh

#endif
