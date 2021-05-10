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
#ifndef BOTANYNET_SOILPROBE_H
#define BOTANYNET_SOILPROBE_H

#include <Arduino.h>
#include <limits>

#include "config.hpp"

namespace BotanyNet
{
/**
 * Super-cheap soil probe from SparkFun (https://www.sparkfun.com/products/13322)
 */
class SoilProbe final
{
public:
    using AnalogReadType = int;
    static constexpr size_t MaxADCValue = (1 << ADCResolutionBits) - 1;

    SoilProbe(int power_pin, int adc_pin)
        : m_power_pin(power_pin)
        , m_adc_pin(adc_pin)
    {}

    void start()
    {
        analogReadResolution(ADCResolutionBits);
        analogReadCorrection(ADCOffset, ADCGain);

        pinMode(m_power_pin, OUTPUT);
        digitalWrite(m_power_pin, LOW);
        pinMode(m_adc_pin, INPUT);
    }

    void stop()
    {
        digitalWrite(m_power_pin, LOW);
        // High impedance
        pinMode(m_power_pin, INPUT);
    }

    /**
     * Blocking ADC read of the sensor for now. Interrupts in the future?
     * We'll see.
     */
    float readSoil()
    {
        digitalWrite(m_power_pin, HIGH);
        delay(10);
        const AnalogReadType reading = analogRead(m_adc_pin);
        digitalWrite(m_power_pin, LOW);
        return reading / static_cast<float>(MaxADCValue);
    }

private:
    const int m_power_pin;
    const int m_adc_pin;
};

}  // namespace BotanyNet

#endif  // BOTANYNET_SOILPROBE_H
