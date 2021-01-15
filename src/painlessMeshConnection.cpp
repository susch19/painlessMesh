//
//  painlessMeshConnection.cpp
//
//
//  Created by Bill Gray on 7/26/16.
//
//

#include "painlessMeshConnection.h"
#include "painlessMesh.h"

#include "painlessmesh/base64.hpp"
#include "painlessmesh/configuration.hpp"
#include "painlessmesh/logger.hpp"
using namespace painlessmesh;

//#include "lwip/priv/tcpip_priv.h"

extern LogClass Log;

static painlessmesh::buffer::temp_buffer_t shared_buffer;

ICACHE_FLASH_ATTR MeshConnection::MeshConnection(
    AsyncClient *client_ptr, painlessmesh::Mesh<MeshConnection> *pMesh,
    bool is_station) {
  using namespace painlessmesh;
  station = is_station;
  mesh = pMesh;
  client = client_ptr;

  client->setNoDelay(true);
  if (station) {  // we are the station, start nodeSync
    Log(CONNECTION, "meshConnectedCb(): we are STA\n");
  } else {
    Log(CONNECTION, "meshConnectedCb(): we are AP\n");
  }
}

ICACHE_FLASH_ATTR MeshConnection::~MeshConnection() {
  Log(CONNECTION, "~MeshConnection():\n");
  this->close();
  if (!client->freeable()) {
    Log(CONNECTION, "~MeshConnection(): Closing pcb\n");
    client->close(true);
  }
  client->abort();
  delete client;
}

void MeshConnection::initTCPCallbacks() {
  using namespace logger;
  client->onDisconnect(
      [self = this->shared_from_this()](void *arg, AsyncClient *client) {
        // Making a copy of self->mesh pointer, because self->close() can
        // invalidate the original pointer, causing a segmentation fault
        // when trying to access self->mesh afterwards
        auto m = self->mesh;
        if (m->semaphoreTake()) {
          Log(CONNECTION, "onDisconnect(): dropping %u now= %u\n", self->nodeId,
              m->getNodeTime());
          self->close();
          m->semaphoreGive();
        }
      },
      NULL);

  client->onData(
      [self = this->shared_from_this()](void *arg, AsyncClient *client,
                                        void *data, size_t len) {
        using namespace logger;
        if (self->mesh->semaphoreTake()) {
          Log(COMMUNICATION, "Recieved Length: %zu\n", len);
          painlessmesh::protocol::ProtocolHeader head = {};
          int offset = 0;
          auto enc =
              base64::encode(static_cast<const unsigned char *>(data), len);
          Log(COMMUNICATION, "Recieved encoded as b64: %s\n", enc.c_str());
          head.deserializeFrom(
              std::string(static_cast<const char *>(data), len), offset);
          Log(COMMUNICATION, "onData(): fromId=%u, type=%zu, routing=%zu\n",
              self ? self->nodeId : 0, head.type, head.routing);

          // if(data[0-1] > len, if package 2)
          // auto variable = receiveBuffer.popFront();
          // variable += data;

          self->receiveBuffer.push(
              std::string(static_cast<const char *>(data), len), shared_buffer);

          // Signal that we are done
          self->client->ack(len);
          // do nothing
          self->readBufferTask.forceNextIteration();

          self->mesh->semaphoreGive();
        }
      },
      NULL);

  client->onAck(
      [self = this->shared_from_this()](void *arg, AsyncClient *client,
                                        size_t len, uint32_t time) {
        using namespace logger;
        if (self->mesh->semaphoreTake()) {
          self->sentBufferTask.forceNextIteration();
          self->mesh->semaphoreGive();
        }
      },
      NULL);

  client->onError(
      [self = this->shared_from_this()](void *arg, AsyncClient *client,
                                        int8_t err) {
        if (self->mesh->semaphoreTake()) {
          // When AsyncTCP gets an error it will call both
          // onError and onDisconnect
          // so we handle this in the onDisconnect callback
          Log(CONNECTION, "tcp_err(): MeshConnection %s\n",
              client->errorToString(err));
          self->mesh->semaphoreGive();
        }
      },
      NULL);
}

void MeshConnection::initTasks() {
  using namespace logger;

  timeOutTask.set(NODE_TIMEOUT, TASK_ONCE, [self = this->shared_from_this()]() {
    Log(CONNECTION, "Time out reached\n");
    self->close();
  });
  mesh->mScheduler->addTask(timeOutTask);

  this->nodeSyncTask.set(
      TASK_MINUTE, TASK_FOREVER, [self = this->shared_from_this()]() {
        Log(SYNC, "nodeSyncTask(): request with %u\n", self->nodeId);
        auto p = self->request(self->mesh->asNodeTree());
        router::send<protocol::NodeSyncRequest, MeshConnection>(p, self);
        self->timeOutTask.disable();
        self->timeOutTask.restartDelayed();
      });
  mesh->mScheduler->addTask(this->nodeSyncTask);
  if (station)
    this->nodeSyncTask.enable();
  else
    this->nodeSyncTask.enableDelayed(10 * TASK_SECOND);

  receiveBuffer = painlessmesh::buffer::ReceiveBuffer<TSTRING>();
  readBufferTask.set(
      TASK_SECOND, TASK_FOREVER, [self = this->shared_from_this()]() {
        Log(GENERAL, "readBufferTask()\n");
        if (!self->receiveBuffer.empty()) {
          Log(GENERAL, "readBufferTask not empty()\n");
          TSTRING frnt = self->receiveBuffer.front();
          self->receiveBuffer.pop_front();
          if (!self->receiveBuffer.empty())
            self->readBufferTask.forceNextIteration();

          Log(GENERAL, "popped front of recieve: %zu\n", frnt.size());
          router::routePackage<MeshConnection>(
              (*self->mesh), self->shared_from_this(), frnt,
              self->mesh->callbackList, self->mesh->getNodeTime());
          Log(GENERAL, "routed successfully\n");
        }

        Log(GENERAL, "readBufferTask()...\n");
      });
  mesh->mScheduler->addTask(readBufferTask);
  readBufferTask.enableDelayed();

  sentBufferTask.set(
      TASK_SECOND, TASK_FOREVER, [self = this->shared_from_this()]() {
        Log(GENERAL, "sentBufferTask()\n");
        if (!self->sentBuffer.empty() && self->client->canSend()) {
          Log(GENERAL, "readBufferTask not empty() and can send\n");
          auto ret = self->writeNext();
          if (ret)
            self->sentBufferTask.forceNextIteration();
          else
            self->sentBufferTask.delay(100 * TASK_MILLISECOND);
        }
      });
  mesh->mScheduler->addTask(sentBufferTask);
  sentBufferTask.enableDelayed();
}

void ICACHE_FLASH_ATTR MeshConnection::close() {
  if (!connected) return;

  Log(CONNECTION, "MeshConnection::close() %u.\n", this->nodeId);
  this->connected = false;

  this->timeSyncTask.setCallback(NULL);
  this->nodeSyncTask.setCallback(NULL);
  this->readBufferTask.setCallback(NULL);
  this->sentBufferTask.setCallback(NULL);
  this->timeOutTask.setCallback(NULL);
  this->timeSyncTask.disable();
  this->nodeSyncTask.disable();
  this->readBufferTask.disable();
  this->sentBufferTask.disable();
  this->timeOutTask.disable();

  this->client->onDisconnect(NULL, NULL);
  this->client->onError(NULL, NULL);
  this->client->onData(NULL, NULL);
  this->client->onAck(NULL, NULL);

  mesh->addTask(
      [mesh = this->mesh, nodeId = this->nodeId, station = this->station]() {
        Log(CONNECTION, "closingTask(): dropping %u now= %u\n", nodeId,
            mesh->getNodeTime());
        mesh->changedConnectionCallbacks.execute(nodeId);
        mesh->droppedConnectionCallbacks.execute(nodeId, station);
      });

  if (client->connected()) {
    Log(CONNECTION, "close(): Closing pcb\n");
    client->close();
  }

  receiveBuffer.clear();
  sentBuffer.clear();
  NodeTree::clear();
  Log(CONNECTION, "MeshConnection::close() done. Was station: %d.\n",
      this->station);
}

bool ICACHE_FLASH_ATTR MeshConnection::addMessage(std::string &message,
                                                  bool priority) {
  auto b64 = base64::encode(message);
#ifdef DEBUG
  Serial.printf("Trying to add message with size %zu, %s\n", message.size(),
                b64.c_str());
#endif
  if (ESP.getFreeHeap() - message.length() >=
      MIN_FREE_MEMORY) {  // If memory heap is enough, queue the message
    if (priority) {
      Log(COMMUNICATION,
          "addMessage(): Package sent to queue beginning -> %d , "
          "FreeMem: %d\n",
          sentBuffer.size(), ESP.getFreeHeap());
      sentBuffer.push(message, priority);
    } else {
      if (sentBuffer.size() < MAX_MESSAGE_QUEUE) {
        Log(COMMUNICATION,
            "addMessage(): Package sent to queue end -> %d , FreeMem: "
            "%d\n",
            sentBuffer.size(), ESP.getFreeHeap());
        sentBuffer.push(message, priority);
      } else {
        Log(ERROR, "addMessage(): Message queue full -> %d , FreeMem: %d\n",
            sentBuffer.size(), ESP.getFreeHeap());
        sentBufferTask.forceNextIteration();
        return false;
      }
    }
    sentBufferTask.forceNextIteration();
    return true;
  } else {
    // connection->sendQueue.clear(); // Discard all messages if free memory is
    // low
    Log(DEBUG, "addMessage(): Memory low, message was discarded\n");
    sentBufferTask.forceNextIteration();
    return false;
  }
}

bool ICACHE_FLASH_ATTR MeshConnection::writeNext() {
  if (sentBuffer.empty()) {
    Log(COMMUNICATION, "writeNext(): sendQueue is empty\n");
    return false;
  }
  auto len = sentBuffer.requestLength(shared_buffer.length);
  auto snd_len = client->space();
  Log(COMMUNICATION, "Having space %zu and snd_len %zu\n", len, snd_len);
  if (len > snd_len) len = snd_len;
  if (len > 0) {
    // sentBuffer.read(len, shared_buffer);
    // auto written = client->write(shared_buffer.buffer, len, 1);
    auto data_ptr = sentBuffer.readPtr(len);
    auto written = client->write(data_ptr, len, 1);
    if (written == len) {
      Log(COMMUNICATION, "writeNext(): Package sent\n");
      // Log(COMMUNICATION, "writeNext(): Package sent %s\n", data_ptr);
      // client->send();  // TODO only do this for priority messages
      sentBuffer.freeRead();
      sentBufferTask.forceNextIteration();
      return true;
    } else if (written == 0) {
      Log(COMMUNICATION,
          "writeNext(): tcp_write Failed node=%u. Resending later\n", nodeId);
      return false;
    } else {
      Log(ERROR,
          "writeNext(): Less written than requested. Please report bug on "
          "the issue tracker\n");
      return false;
    }
  } else {
    Log(COMMUNICATION, "writeNext(): tcp_sndbuf not enough space\n");
    return false;
  }
}
