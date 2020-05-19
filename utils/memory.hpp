#ifndef CRISPY_DOOM_MEMORY_HPP
#define CRISPY_DOOM_MEMORY_HPP

#include <new>
#include <cstdlib>
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

template<typename PtrDataType>
auto zone_malloc_ptr(int tag)
{
   return zone_malloc<typename std::remove_pointer<PtrDataType>::type>(tag);
}

template<typename PtrDataType>
auto zone_malloc_ptr(int tag, const std::size_t size)
{
   return zone_malloc<typename std::remove_pointer<PtrDataType>::type>(tag, size);
}

template<typename DataType>
auto zmalloc(int size, int tag, void *ptr)
{
  return static_cast<DataType>(Z_Malloc(size, tag, ptr));
}


#endif // CRISPY_DOOM_MEMORY_HPP
