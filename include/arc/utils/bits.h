/*
 * File: bits.h
 * Project: libarc
 * File Created: Monday, 14th December 2020 9:44:00 pm
 * Author: Minjun Xu (mjxu96@outlook.com)
 * -----
 * MIT License
 * Copyright (c) 2020 Minjun Xu
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef LIBARC__UTILS__BITS_H
#define LIBARC__UTILS__BITS_H

#include <type_traits>
#include <bitset>

namespace arc {
namespace utils {

template<typename T>
requires std::is_arithmetic_v<T>
T BitSet(T in_coming, int bit_pos, bool val) {
  assert(bit_pos <= sizeof(T) * 8u);
  if (val) {
    return (in_coming | (1UL << bit_pos));
  }
  return (in_coming & ~(1UL << bit_pos));
}

template<typename T>
requires std::is_arithmetic_v<T>
bool GetBit(T in_coming, int bit_pos) {
  assert(bit_pos <= sizeof(T) * 8u);
  return (in_coming & ~(1UL << bit_pos)) > 0;
}

template<typename T>
requires std::is_arithmetic_v<T>
std::bitset<sizeof(T) * 8U> GetStdBitSet(T in_coming) {
  return std::bitset<sizeof(T) * 8U>(in_coming);
}

template<typename T>
requires std::is_arithmetic_v<T>
T GetLowerBits(T in_coming, int lower_bits) {
  assert(lower_bits <= sizeof(T) * 8u);
  T all_ones = ~(0);
  return (in_coming & (~(all_ones << lower_bits)));
}


} // namespace utils
} // namespace arc


#endif /* LIBARC__UTILS__BITS_H */
