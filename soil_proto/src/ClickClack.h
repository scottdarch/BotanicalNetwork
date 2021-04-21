/*
    MIT License

    Copyright (c) 2021 Scott A Dixon

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#ifndef IO_THIRTYTWOBITS_CLICKCLACK_H
#define IO_THIRTYTWOBITS_CLICKCLACK_H

#include <stdint.h>
#include <stddef.h>

namespace io
{
namespace thirtytwobits
{
template <size_t ROW_COUNT, size_t COL_COUNT, size_t EVENT_BUFFER_SIZE = 10>
class ClickClack final
{
public:
    ClickClack();
    ~ClickClack();

    ClickClack(const ClickClack&)  = delete;
    ClickClack(const ClickClack&&) = delete;
    ClickClack& operator=(const ClickClack&) = delete;
    ClickClack& operator=(const ClickClack&&) = delete;
};
};  // namespace thirtytwobits
};  // namespace io

#endif  // IO_THIRTYTWOBITS_CLICKCLACK_H