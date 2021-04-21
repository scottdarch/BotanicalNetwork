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

#ifndef IO_THIRTYTWOBITS_MULTIFUNCTIONDISPLAY_H
#define IO_THIRTYTWOBITS_MULTIFUNCTIONDISPLAY_H

#include <stdint.h>
#include <stddef.h>
#include <Adafruit_SSD1306.h>

namespace io
{
namespace thirtytwobits
{
template <size_t ROW_COUNT, size_t COL_COUNT, size_t EVENT_BUFFER_SIZE = 10>
class MultiFunctionDisplay final
{
public:
    MultiFunctionDisplay();
    ~MultiFunctionDisplay();

    MultiFunctionDisplay(const MultiFunctionDisplay&)  = delete;
    MultiFunctionDisplay(const MultiFunctionDisplay&&) = delete;
    MultiFunctionDisplay& operator=(const MultiFunctionDisplay&) = delete;
    MultiFunctionDisplay& operator=(const MultiFunctionDisplay&&) = delete;

private:
    #define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 32  // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
};
};  // namespace thirtytwobits
};  // namespace io

#endif  // IO_THIRTYTWOBITS_MULTIFUNCTIONDISPLAY_H