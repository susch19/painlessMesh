#ifndef _PAINLESS_MESH_NODETREE_HPP_
#define _PAINLESS_MESH_NODETREE_HPP_

#include <list>
#include <string>

namespace painlessmesh {
namespace protocol  // save renaming
{

class NodeTree {
 public:
  uint32_t nodeId = 0;
  bool root = false;
  std::list<NodeTree> subs;

  NodeTree() {}
  NodeTree(uint32_t nodeID, bool iAmRoot) {
    nodeId = nodeID;
    root = iAmRoot;
  }

  bool operator==(const NodeTree& b) const {
    if (!(this->nodeId == b.nodeId && this->root == b.root &&
          this->subs.size() == b.subs.size()))
      return false;
    auto itA = this->subs.begin();
    auto itB = b.subs.begin();
    for (size_t i = 0; i < this->subs.size(); ++i) {
      if ((*itA) != (*itB)) {
        return false;
      }
      ++itA;
      ++itB;
    }
    return true;
  }

  bool operator!=(const NodeTree& b) const { return !this->operator==(b); }

  std::string toString(bool pretty = false);

  uint32_t size() {
    uint32_t size = sizeof(nodeId) + sizeof(root);
    size += sizeof(subs.size());
    for (auto&& i : subs) {
      size += i.size();
    }
    return size;
  }

  void clear() {
    nodeId = 0;
    subs.clear();
    root = false;
  }
};
}  // namespace protocol

}  // namespace painlessmesh
#endif