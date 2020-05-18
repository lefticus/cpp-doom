#ifndef CRISPY_DOOM_MEMORY_HPP
#define CRISPY_DOOM_MEMORY_HPP

#include <new>
#include <cstdlib>
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
auto zmalloc(int size, int tag, void *ptr)
{
  return static_cast<DataType>(Z_Malloc(size, tag, ptr));
}


#endif // CRISPY_DOOM_MEMORY_HPP
