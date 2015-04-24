#ifndef ID_MAP_HPP
#define ID_MAP_HPP

#include "hash_map.hpp"
#include "uint256.hpp"

uint32_t hash_uint256(const uint256 & a);

template<typename T>
using IdMap = HashMap<uint256, T, hash_uint256>;

#endif
