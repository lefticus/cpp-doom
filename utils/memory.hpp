#ifndef CRISPY_DOOM_MEMORY_HPP
#define CRISPY_DOOM_MEMORY_HPP

#include <new>
#include <cstdlib>
#include <cstring>
#include <type_traits>

#include "../src/z_zone.hpp"

// todo fix me
template<typename DataType>
auto create_struct()
{
    auto *mem = malloc(sizeof(DataType));
    return new (mem) DataType{};
}

template <typename DataType> auto create_struct(const std::size_t size) {
  auto *mem = malloc(sizeof(DataType) * size);
  return static_cast<DataType *>(new (mem) DataType[size]);
}

template<typename DataType>
auto zone_malloc(int tag)
{
   auto *mem = Z_Malloc(sizeof(DataType), tag, nullptr);
   return new (mem) DataType{};
}

template<typename DataType>
auto zone_malloc(int tag, const std::size_t size)
{
   auto *mem = Z_Malloc(sizeof(DataType) * size, tag, nullptr);
   return new (mem) DataType[size];
}

// PTR is typically the pointer to the pointer that stores the Z_Malloc'd value
// and is used for statistics gathering, but it can be anything really.
template<typename DataType, typename PtrType>
auto zone_malloc(int tag, const std::size_t size, PtrType ptr)
{
   auto *mem = Z_Malloc(sizeof(DataType) * size, tag, static_cast<void*>(ptr));
   return new (mem) DataType[size];
}

template<typename DataType>
auto zone_calloc(int tag)
{
   auto *mem = Z_Malloc(sizeof(DataType), tag, nullptr);
   std::memset( mem, 0, sizeof(DataType));
   return new (mem) DataType{};
}

template<typename DataType>
auto zone_calloc(int tag, const std::size_t size)
{
   auto *mem = Z_Malloc(sizeof(DataType) * size, tag, nullptr);
   std::memset( mem, 0, sizeof(DataType) * size);
   return new (mem) DataType[size];
}

// PTR is typically the pointer to the pointer that stores the Z_Malloc'd value
// and is used for statistics gathering, but it can be anything really.
template<typename DataType, typename PtrType>
auto zone_calloc(int tag, const std::size_t size, PtrType ptr)
{
   auto *mem = Z_Malloc(sizeof(DataType) * size, tag, static_cast<void*>(ptr));
   std::memset( mem, 0, sizeof(DataType) * size);
   return new (mem) DataType[size];
}

#endif // CRISPY_DOOM_MEMORY_HPP
