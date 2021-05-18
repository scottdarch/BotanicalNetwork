//              \.
// BOT(any)NET  . -
// ------------.---------------------------------------------------------------
//
//  MIT License
//
//  Copyright (c) 2021 Scott A Dixon
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.
//
#ifndef BOTANYNET_CONFIG_INCLUDED_H
#define BOTANYNET_CONFIG_INCLUDED_H

#include <cstdint>

#ifndef BOTANYNET_XSTR
#    define BOTANYNET_XSTR(s) BOTANYNET_STR(s)
#    define BOTANYNET_STR(s) #s
#endif

#ifndef EPOCH_TIME_OFF
#   define EPOCH_TIME_OFF 946684800  // This is 1st January 2000, 00:00:00 in epoch time
#endif

// +--------------------------------------------------------------------------+
// | CONFIGURATION
// +--------------------------------------------------------------------------+
// We need to wait for a bit so we have a chance to attach a serial console or 
// start programming a firmware. Once we start sleeping the device becomes
// unavailable for either.
constexpr unsigned long StartupDelayMillis = 10000;
constexpr uint8_t SampleDelaySec = 600;
static_assert(SampleDelaySec < 3600, "We're assuming that we are sleeping < 1 hour.");

#define BotnetNodeId 1
constexpr const char* BotnetBroker = "botnet";
constexpr int BotnetPort = 1883;

constexpr int PinSht1Data = 11;
constexpr int PinSht1Clk = 12;
constexpr int PinSoilProbePower = 7;
constexpr int PinSoilProbeADC = A1;

constexpr unsigned long MDNSTimeoutMillis = 5000;
constexpr unsigned long WiFiTimeoutMillis = 10000;

// For debugging set to false to disable sleep, keep the serial console attached, and run
// the wifi at full speed.
constexpr bool EnableLowPowerMode = false;

// ADC Calibration provided by CorrectADCResponse sketch for SAMD cores.
constexpr size_t ADCResolutionBits = 12u;
constexpr size_t ADCOffset = 15;
constexpr size_t ADCGain = 2063;

// Tri-colour LED attached to UBlox WiFi module.
constexpr int WiFiNINAPinLEDRed = 26;
constexpr int WiFiNINAPinLEDGreen = 25;
constexpr int WiFiNINAPinLEDBlue = 27;

// If defined then components may log stuff to Serial. If not defined only the top
// level sketch is allowed to use the Serial terminal.
// #define BOTNET_ENABLE_SERIAL_INTERNAL_DEBUG

#ifdef BOTNET_ENABLE_SERIAL_INTERNAL_DEBUG
#define BOTNET_SERIAL_DEBUG_PRINT(DATA) Serial.print(DATA)
#else
#define BOTNET_SERIAL_DEBUG_PRINT(DATA) do {} while(0)
#endif

#ifdef BOTNET_ENABLE_SERIAL_INTERNAL_DEBUG
#define BOTNET_SERIAL_DEBUG_PRINTLN(DATA) Serial.println(DATA)
#else
#define BOTNET_SERIAL_DEBUG_PRINTLN(DATA) do {} while(0)
#endif

#endif  // BOTANYNET_CONFIG_INCLUDED_H
