#pragma once
#include "Monstey_Havok.hpp"
#include <map>
#include <bit>
#include <algorithm>
namespace Monstey_PhysicsFixture {
using namespace Monstey;using B=Monstey_Bytes;using J=Monstey_Json;
void Float(B &bytes, float value) {
  Monstey_AppendU32(bytes, std::bit_cast<std::uint32_t>(value));
}
B Transform() {
  B bytes;
  for (float value : {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f})
    Float(bytes, value);
  return bytes;
}
B Box() {
  B bytes;
  Monstey_AppendU32(bytes, 2);
  Monstey_AppendU32(bytes, 0xffc01234);
  Float(bytes, -2.75f);
  Float(bytes, .125f);
  for (float value : {1.f, 2.f, 3.f})
    Float(bytes, value);
  return bytes;
}
B Null() {
  B bytes;
  Monstey_AppendU32(bytes, 0);
  return bytes;
}
B Wrapped(B child) {
  B bytes;
  Monstey_AppendU32(bytes, 5);
  auto transform = Transform();
  bytes.insert(bytes.end(), transform.begin(), transform.end());
  bytes.insert(bytes.end(), child.begin(), child.end());
  return bytes;
}
B Compound(std::initializer_list<B> children) {
  B bytes;
  Monstey_AppendU32(bytes, 6);
  Monstey_AppendU32(bytes, static_cast<std::uint32_t>(children.size()));
  for (const auto &child : children)
    bytes.insert(bytes.end(), child.begin(), child.end());
  return bytes;
}
B Tag() {
  B bytes;
  for (auto word : {12u, 0x11223344u, 0x55667788u, 0xffffd00du, 8u})
    Monstey_AppendU32(bytes, word);
  bytes.insert(bytes.end(), {0x40, 0, 0, 8, 'T', 'A', 'G', '0'});
  Monstey_AppendU32(bytes, 0xffffdead);
  return bytes;
}
B Definition(bool prefab) {
  B bytes;
  Monstey_AppendU32(bytes, 0xa5a51234);
  const std::string name = prefab ? "worlds/test.prefab" : "";
  Monstey_AppendU16(bytes, static_cast<std::uint16_t>(name.size()));
  bytes.insert(bytes.end(), name.begin(), name.end());
  if (prefab) {
    Monstey_AppendU32(bytes, 0xdeadbeef);
    Monstey_AppendU32(bytes, 0x7fc12345);
  }
  for (auto word : {0x3f800000u, 0x80000000u, 0xffc00000u, 0x40000000u,
                    0x40400000u, 0x40800000u})
    Monstey_AppendU32(bytes, word);
  return bytes;
}
B Fixture(const std::vector<B> &shapes, bool prefab = false) {
  B bytes;
  Monstey_AppendU32(bytes, static_cast<std::uint32_t>(shapes.size()));
  Monstey_AppendU32(bytes, static_cast<std::uint32_t>(shapes.size()));
  for (std::size_t i = 0; i < shapes.size(); ++i)
    Monstey_AppendU32(bytes, static_cast<std::uint32_t>(10 + i));
  for (std::size_t i = 0; i < shapes.size(); ++i)
    Monstey_AppendU16(bytes, static_cast<std::uint16_t>(i));
  for (std::size_t i = 0; i < shapes.size(); ++i) {
    const auto def = Definition(prefab);
    bytes.insert(bytes.end(), def.begin(), def.end());
  }
  for (auto word : {0xfafab0b0u, 2u, static_cast<std::uint32_t>(shapes.size())})
    Monstey_AppendU32(bytes, word);
  for (const auto &shape : shapes) {
    const auto transform = Transform();
    bytes.insert(bytes.end(), transform.begin(), transform.end());
    bytes.insert(bytes.end(), shape.begin(), shape.end());
  }
  Monstey_AppendU32(bytes, 0xfafabfbf);
  return bytes;
}
}
