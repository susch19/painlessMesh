#ifndef _PAINLESS_MESH_ROUTER_HPP_
#define _PAINLESS_MESH_ROUTER_HPP_

#include <algorithm>
#include <map>

#include "GDBStub.h"
#include "painlessmesh/callback.hpp"
#include "painlessmesh/layout.hpp"
#include "painlessmesh/logger.hpp"
#include "painlessmesh/packageTypeProvider.hpp"
#include "painlessmesh/protocol.hpp"
#include "painlessmesh/variant.hpp"

extern painlessmesh::logger::LogClass Log;

namespace painlessmesh {

/**
 * Helper functions to route messages
 */
namespace router {
template <class T>
std::shared_ptr<T> findRoute(layout::Layout<T> tree,
                             std::function<bool(std::shared_ptr<T>)> func) {
  auto route = std::find_if(tree.subs.begin(), tree.subs.end(), func);
  if (route == tree.subs.end()) return NULL;
  return (*route);
}

template <class T>
std::shared_ptr<T> findRoute(layout::Layout<T> tree, uint32_t nodeId) {
  return findRoute<T>(tree, [nodeId](std::shared_ptr<T> s) {
    return layout::contains((*s), nodeId);
  });
}

template <class T, class U>
bool send(T& package, std::shared_ptr<U> conn, bool priority = false) {
  auto variant = Variant<T>(&package);
  std::string msg;
  msg.resize(package.size() + sizeof(int));
  int offset =  sizeof(int);
  variant.serializeTo(msg,offset);
  return conn->addMessage(msg, priority);
}

template <class T, class U>
bool send(Variant<T>* variant, std::shared_ptr<U> conn, bool priority = false) {
  TSTRING msg;
  msg.resize(variant->size() + sizeof(int));
  int offset =  sizeof(int);
  variant->serializeTo(msg,offset);
  return conn->addMessage(msg, priority);
}

template <class T, class U>
bool send(T& package, layout::Layout<U> layout) {
  auto variant = Variant<T>(&package);
  std::string msg;
  msg.resize(package.size() + sizeof(int));
  int offset =  sizeof(int);
  variant.serializeTo(msg,offset);
  auto conn = findRoute<U>(layout, variant.package->header.dest);
  if (conn) return conn->addMessage(msg);
  return false;
}

template <class T, class U>
bool send(Variant<T>* variant, layout::Layout<U> layout) {
  std::string msg;

  msg.resize(variant->size() + sizeof(int));
  int offset =  sizeof(int);
  variant->serializeTo(msg,offset);
  auto conn = findRoute<U>(layout, variant.dest());
  if (conn) return conn->addMessage(msg);
  return false;
}
template <class U>
bool send(std::string& msg, protocol::ProtocolHeader& header,
          layout::Layout<U> layout) {
  auto conn = findRoute<U>(layout, header.dest);
  if (conn) return conn->addMessage(msg);
  return false;
}

template <class T, class U>
size_t broadcast(T& package, layout::Layout<U> layout, uint32_t exclude) {
  auto variant = Variant<T>(&package);
  std::string msg;
  msg.resize(package.size() + sizeof(int));
  int offset =  sizeof(int);
  variant.serializeTo(msg,offset);

  size_t i = 0;
  for (auto&& conn : layout.subs) {
    if (conn->nodeId != 0 && conn->nodeId != exclude) {
      auto sent = conn->addMessage(msg);
      if (sent) ++i;
    }
  }
  return i;
}

template <class T>
size_t broadcast(VariantBase* variant, layout::Layout<T> layout,
                 uint32_t exclude) {
  std::string msg;
  msg.resize(variant->size() + sizeof(int));
  int offset =  sizeof(int);
  variant->serializeTo(msg,offset);
  size_t i = 0;
  for (auto&& conn : layout.subs) {
    if (conn->nodeId != 0 && conn->nodeId != exclude) {
      auto sent = conn->addMessage(msg);
      if (sent) ++i;
    }
  }
  return i;
}

template <class T>
size_t broadcast(std::string& msg, layout::Layout<T> layout, uint32_t exclude) {
  size_t i = 0;
  for (auto&& conn : layout.subs) {
    if (conn->nodeId != 0 && conn->nodeId != exclude) {
      auto sent = conn->addMessage(msg);
      if (sent) ++i;
    }
  }
  return i;
}

template <class T>
void routePackage(layout::Layout<T> layout, std::shared_ptr<T> connection,
                  TSTRING pkg, callback::MeshPackageCallbackList<T> cbl,
                  uint32_t receivedAt) {
  using namespace logger;

  Log(COMMUNICATION, "routePackage(): Recvd from %u: %zu\n", connection->nodeId,
      pkg.size());

  int offset = 0;
  protocol::ProtocolHeader header;
  header.deserializeFrom(pkg, offset);

  Log(COMMUNICATION,
      "routePackage(): Recvd header type:%zu, route:%zu, dest:%zu\n",
      header.type, header.routing, header.dest);
  if (header.routing == SINGLE && header.dest != layout.getNodeId()) {
    // Send on without further processing
    Log(COMMUNICATION,
        "routePackage(): Just Route package type:%zu, route:%zu, dest:%zu\n",
        header.type, header.routing, header.dest);
    send<T>(pkg, header, layout);
    return;
  } else if (header.routing == BROADCAST) {
    Log(COMMUNICATION,
        "routePackage(): Broadcast Package type:%zu, route:%zu, dest:%zu\n",
        header.type, header.routing, header.dest);
    broadcast<T>(pkg, layout, connection->nodeId);
  }

  auto variant = PackageTypeProvider::get(header);
  variant->deserializeFrom(pkg);
  Log(COMMUNICATION,
      "routePackage(): Serialized to variant header type:%zu, route:%zu, "
      "dest:%zu\n",
      variant->type(), header.routing, header.dest);
  auto calls =
      cbl.execute(header.type, variant.get(), connection,
                  receivedAt);  // VariantBase*, std::shared_ptr<T>, uint32_t
  // Log(DEBUG, "routePackage(): %zu callbacks executed; %zu\n", calls,
  //     variant->type());
}

template <class T, class U>
void handleNodeSync(T& mesh, protocol::NodeTree* newTree,
                    std::shared_ptr<U> conn) {
  Log(logger::SYNC, "handleNodeSync(): with %u\n", conn->nodeId);

  if (!conn->validSubs(*newTree)) {
    Log(logger::SYNC, "handleNodeSync(): invalid new connection\n");
    conn->close();
    return;
  }

  if (conn->newConnection) {
    auto oldConnection = router::findRoute<U>(mesh, newTree->nodeId);
    if (oldConnection) {
      Log(logger::SYNC,
          "handleNodeSync(): already connected to %u. Closing the new "
          "connection \n",
          conn->nodeId);
      conn->close();
      return;
    }

    mesh.addTask([&mesh, remoteNodeId = newTree->nodeId]() {
      Log(logger::CONNECTION, "newConnectionTask():\n");
      Log(logger::CONNECTION, "newConnectionTask(): adding %u now= %u\n",
          remoteNodeId, mesh.getNodeTime());
      mesh.newConnectionCallbacks.execute(remoteNodeId);
    });

    // Initially interval is every 10 seconds,
    // this will slow down to TIME_SYNC_INTERVAL
    // after first succesfull sync
    // TODO move it to a new connection callback and use initTimeSync from
    // ntp.hpp
    conn->timeSyncTask.set(10 * TASK_SECOND, TASK_FOREVER, [conn, &mesh]() {
      Log(logger::S_TIME, "timeSyncTask(): %u\n", conn->nodeId);
      mesh.startTimeSync(conn);
    });
    mesh.mScheduler->addTask(conn->timeSyncTask);
    if (conn->station)
      // We are STA, request time immediately
      conn->timeSyncTask.enable();
    else
      // We are the AP, give STA the change to initiate time sync
      conn->timeSyncTask.enableDelayed();
    conn->newConnection = false;
  }

  if (conn->updateSubs(*newTree)) {
    mesh.addTask([&mesh, nodeId = newTree->nodeId]() {
      mesh.changedConnectionCallbacks.execute(nodeId);
    });
  } else {
    conn->nodeSyncTask.delay();
    mesh.stability += std::min(1000 - mesh.stability, (size_t)25);
  }
}

template <class T, typename U>
callback::MeshPackageCallbackList<U> addPackageCallback(
    callback::MeshPackageCallbackList<U>&& callbackList, T& mesh) {
  // REQUEST type,
  callbackList.onPackage(
      protocol::NODE_SYNC_REQUEST,
      [&mesh](VariantBase* variant, std::shared_ptr<U> connection,
              uint32_t receivedAt) {
        auto typedVariant = (Variant<protocol::NodeSyncRequest>*)variant;
        auto newTree = typedVariant->package;
        handleNodeSync<T, U>(mesh, newTree, connection);
        auto nodeTree = connection->reply(std::move(mesh.asNodeTree()));
        send<protocol::NodeSyncReply>(nodeTree, connection, true);
        return false;
      });

  // Reply type just handle it
  callbackList.onPackage(
      protocol::NODE_SYNC_REPLY,
      [&mesh](VariantBase* variant, std::shared_ptr<U> connection,
              uint32_t receivedAt) {
        auto typedVariant = (Variant<protocol::NodeSyncReply>*)variant;
        auto newTree = typedVariant->package;
        handleNodeSync<T, U>(mesh, newTree, connection);
        connection->timeOutTask.disable();
        return false;
      });

  return callbackList;
}

}  // namespace router
}  // namespace painlessmesh

#endif
