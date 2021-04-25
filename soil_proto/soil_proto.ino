// ========================================================
// ========  ==============================================
// ========  ==============================================
// ========  =================  =======================  ==
// =    ===  ===   ===  = ===    =======  = ====   ===    =
// =  =  ==  ==  =  ==     ===  ========     ==  =  ===  ==
// =  =  ==  =====  ==  =  ===  ========  =  ==     ===  ==
// =    ===  ===    ==  =  ===  ========  =  ==  ======  ==
// =  =====  ==  =  ==  =  ===  ========  =  ==  =  ===  ==
// =  =====  ===    ==  =  ===   =======  =  ===   ====   =
// ========================================================
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

#include <Adafruit_GFX.h>
#include <Adafruit_SharpMem.h>
#include "SHT1x.h"

// any pins can be used
#define SHARP_SCK 9
#define SHARP_MOSI 8
#define SHARP_SS 7

// Set the size of the display here, e.g. 144x168!
Adafruit_SharpMem display(SHARP_SCK, SHARP_MOSI, SHARP_SS, 96, 96);
// The currently-available SHARP Memory Display (144x168 pixels)
// requires > 4K of microcontroller RAM; it WILL NOT WORK on Arduino Uno
// or other <4K "classic" devices!  The original display (96x96 pixels)
// does work there, but is no longer produced.

SHT1x sht1x(11, 12);

#define BLACK 0
#define WHITE 1

int read_soil()
{
    digitalWrite(6, HIGH);     // turn D6 "On"
    delay(10);                 // wait 10 milliseconds
    int val = analogRead(A0);  // Read the SIG value form sensor
    digitalWrite(6, LOW);      // turn D6 "Off"
    return val;                // send current moisture value
}

void display_value(int soil, float humidity, float temp)
{
    display.setTextSize(5);
    display.setTextColor(BLACK);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setRotation(2);
    display.cp437(true);

    display.println(value);
    display.println(humidity);
    display.refresh();
}

void setup(void)
{
    Serial.begin(9600);
    Serial.println("Hello!");

    // start & clear the display
    display.begin();
    display.clearDisplay();
}

void loop(void)
{
    display_value(read_soil(), sht1x.readHumidity(), sht1x.readTemperatureC());
    delay(500);
}
