#ifndef _BIGMESH_AODV_HPP_
#define _BIGMESH_AODV_HPP_

namespace bigmesh {
namespace aodv {
class MessageID {
 public:
  MessageID() {}
  MessageID(size_t id) : id(id) {}

  bool update(const MessageID &mid) {
    if (this->operator<(mid)) {
      id = mid.id;
      return true;
    }
    return false;
  }

  bool operator<(const MessageID &mid) const {
    if (id == 0) return true;
    return (int)id - (int)mid.id < 0;
  }

  bool operator==(const MessageID &mid) const { return id == mid.id; }

  MessageID &operator++() {
    ++id;
    if (id == 0) ++id;
    return *this;
  }

  size_t value() const { return id; };

 protected:
  size_t id = 0;
};
};  // namespace aodv
};  // namespace bigmesh

#endif
