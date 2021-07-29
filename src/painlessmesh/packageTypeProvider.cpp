#include <painlessmesh/packageTypeProvider.hpp>

namespace painlessmesh {
std::unordered_map<
    int, std::function<std::shared_ptr<VariantBase>(protocol::ProtocolHeader)>>
    PackageTypeProvider::map;
}