#ifndef _PAINLESS_MESH_PACKAGETYPEPROVIDER_HPP_
#define _PAINLESS_MESH_PACKAGETYPEPROVIDER_HPP_

#include <memory>
#include <unordered_map>

#include "variant.hpp"
#include "protocol.hpp"

namespace painlessmesh{
// typedef PackageInterface* (*create_ptr)();
class PackageTypeProvider {
 public:
  template <class T>
  static void add(int typeId) {
    map.emplace(typeId, [](protocol::ProtocolHeader header) {
      auto t = new T(header);

      return std::shared_ptr<VariantBase>(new Variant<T>(t, true));
    });
  };

  static std::shared_ptr<VariantBase> get(protocol::ProtocolHeader header) {
    return map[header.type](header);
  }

 protected:
  static std::unordered_map<
      int, std::function<std::shared_ptr<VariantBase>(protocol::ProtocolHeader)>>
      map;
};
}
#endif
