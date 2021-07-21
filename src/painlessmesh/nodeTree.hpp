#ifndef _PAINLESS_MESH_NODETREE_HPP_
#define _PAINLESS_MESH_NODETREE_HPP_

#include <list>
#include <sstream>
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

  std::string toString(bool pretty = false) {
    /*{"nodeId":1,"subs":[{"nodeId":763956430,"root":true,"subs":[{"nodeId":763955710},{"nodeId":3257231619,"subs":[{"nodeId":3257168800,"subs":[{"nodeId":3257168818,"subs":[{"nodeId":3257232294}]}]}]},{"nodeId":3257233774},{"nodeId":3257144719,"subs":[{"nodeId":3257153413},{"nodeId":3257232527}]}]}]}*/
    std::stringstream ss;
    toString(pretty, ss);

    return ss.str();
  }

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

 private:
  void toString(bool pretty, std::stringstream& ss) {
    ss << "{\"nodeId\":" << nodeId;
    if (root) ss << ",\"root\":true";
    if (subs.size() > 0) {
      ss << ",\"subs\":[";
      for (auto&& sub : subs) {
        sub.toString(pretty, ss);
        if (sub != subs.back()) ss << ",";
      }
      ss << "]";
    }
    ss << "}";
  }
};
}  // namespace protocol

}  // namespace painlessmesh
#endif