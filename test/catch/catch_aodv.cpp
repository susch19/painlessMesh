#define CATCH_CONFIG_MAIN

#include "catch2/catch.hpp"

#include "painlessmesh/configuration.hpp"

#include "bigmesh/aodv.hpp"

#include "catch_utils.hpp"

SCENARIO("MessageID comparison works") {
  using namespace bigmesh::aodv;
  auto id1 = MessageID(10);
  auto id2 = MessageID(11);
  REQUIRE(id1 < id2);
  REQUIRE(!(id2 < id1));

  // Should wrap around when max value is reached
  auto id3 = MessageID(-1);
  REQUIRE(id3 < id1);

  auto id4 = MessageID();
  REQUIRE(id3 < id4);
  // We update anyway, because id4 == 0
  REQUIRE(id4.update(id3));
  REQUIRE(id4 == id3);

  REQUIRE(!id2.update(id1));
  REQUIRE(id2 == MessageID(11));

  REQUIRE(id1.update(id2));
  REQUIRE(id1 == MessageID(11));

  ++id1;
  REQUIRE(id1 == MessageID(12));

  // Increment should skip past 0
  id3 = MessageID(-1);
  ++id3;
  REQUIRE(id3 == MessageID(1));
}
