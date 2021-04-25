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
#include "arduino_secrets.h"
#include <ArduinoMqttClient.h>
#include <MqttClient.h>
#include "HomeNet.h"
#include "SoilProbe.h"
#include "BotanyNet.h"

// +--------------------------------------------------------------------------+
// | NETWORKING
// +--------------------------------------------------------------------------+

BotanyNet::HomeNet homenet(SECRET_SSID, SECRET_SSID);

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

// +--------------------------------------------------------------------------+
// | SENSORS AND DISPLAYS
// +--------------------------------------------------------------------------+
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

// +--------------------------------------------------------------------------+

SHT1x sht1x(11, 12);

#define BLACK 0
#define WHITE 1

BotanyNet::SoilProbe probe(A0, 6);

BotanyNet::Node bnclient(mqttClient, MQTT_BROKER, MQTT_PORT);

// +--------------------------------------------------------------------------+

void display_value(int soil, float humidity, float temp)
{
    display.setTextSize(1);
    display.setTextColor(BLACK);
    display.clearDisplay();
    display.setCursor(0, 0);

    display.print("s: ");
    display.println(soil);

    display.print("h: ");
    display.print(humidity);
    display.println('%');

    display.print("t: ");
    display.print(temp);
    display.println('c');
    display.refresh();
}

// +--------------------------------------------------------------------------+
// | ARDUINO
// +--------------------------------------------------------------------------+
 
void setup(void)
{
    Serial.begin(115200);
    while (!Serial)
    {
        delay(500);
    }
    Serial.println("Hello!");

    // Connect to wifi
    homenet.connect();

    // start & clear the display
    display.begin();
    display.clearDisplay();
    display.setRotation(2);
    display.cp437(true);

    Serial.println("started?");

    homenet.printCurrentNet();
    homenet.printWifiData();
    homenet.printStatus();

    display.setTextSize(5);
    display.setTextColor(BLACK);
    display.clearDisplay();
    display.setCursor(0, 0);

    display.println("...");

    display.refresh();
}

void loop(void)
{
    display_value(probe.read_soil(), sht1x.readHumidity(), sht1x.readTemperatureC());
    delay(500);
}
