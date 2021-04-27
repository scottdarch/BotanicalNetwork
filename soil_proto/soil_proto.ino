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

#include <type_traits>
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

// To setup access to wifi use the arduino ConnectWithWPA example once. This will
// write the SSID and key into NVM.
BotanyNet::HomeNet homenet;

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

BotanyNet::Node bnclient(homenet, mqttClient, MQTT_BROKER, MQTT_PORT);

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
// | SKETCH
// +--------------------------------------------------------------------------+

enum class SketchState : int
{
    Initializing = 0,
    WaitingForLan = 1,
    Online = 2,
    WaitingForMqtt = 3,
    BotnetOnline = 4
};

static SketchState state = SketchState::Initializing;

void checkLan(const unsigned long now_millis)
{
    (void)now_millis;
    homenet.service();
    if (homenet.getStatus() != WL_CONNECTED)
    {
        Serial.println("LAN is not connected. Starting over...");
        homenet.printStatus();
        state = SketchState::Initializing;
    }
}

void handleWaitingForLan(const unsigned long now_millis)
{
    (void)now_millis;
    const uint8_t wifi_status = homenet.getStatus();
    if (wifi_status != WL_CONNECTED && state == SketchState::Initializing)
    {
        Serial.println("Connecting to LAN...");
        const int result = homenet.connect();
        Serial.print("Connection result (");
        Serial.print(result);
        Serial.println(')');
        state = SketchState::WaitingForLan;
    }
    else if (wifi_status == WL_CONNECTED)
    {
        Serial.println("Connected to LAN.");
        homenet.printStatus();
        homenet.printCurrentNet();
        homenet.printWifiData();
        state = SketchState::Online;
    }
    else
    {
        homenet.printStatus();
        Serial.print("wifi not connected (");
        Serial.print(wifi_status);
        Serial.println(")...");
        delay(1000);
    }
}

void checkMqtt(const unsigned long now_millis)
{
    (void)now_millis;
    if (state == SketchState::BotnetOnline && !bnclient.connected())
    {
        Serial.println("MQTT connect was closed. Reconnecting...");
        state = SketchState::Online;
    }
}

void handleWaitingForMqtt(const unsigned long now_millis)
{
    (void)now_millis;
    if (state == SketchState::Online)
    {
        Serial.println("Trying to connect to botnet...");
        const int connection_result = bnclient.connect();
        if (connection_result == MQTT_SUCCESS)
        {
            state = SketchState::WaitingForMqtt;
        }
        else
        {
            state = SketchState::WaitingForMqtt;
            Serial.print("MQTT connection failed! Error code = ");
            Serial.println(connection_result);
            delay(1500);
        }
    }
    else
    {
        // TODO: if connected then advance state.
    }
}

static unsigned long last_update_at_millis = 0;

void runBotnet(const unsigned long now_millis)
{
    if (now_millis - last_update_at_millis > 1000)
    {
        Serial.println("Reading...");
        const float humidity = sht1x.readHumidity();
        const int soil = probe.readSoil();
        const float temperature_celcius = sht1x.readTemperatureC();
        display_value(soil, humidity, temperature_celcius);
        bnclient.sendSoilHumidity(humidity);
    }
    delay(100);
}

// +--------------------------------------------------------------------------+
// | ARDUINO
// +--------------------------------------------------------------------------+

void setup(void)
{
    Serial.begin(115200);
    while (!Serial)
    {
        delay(100);
    }
    Serial.print("Starting prototype botanynet node ");
    Serial.println(BOTNET_NODEID);

    // start & clear the display
    display.begin();
    display.clearDisplay();
    display.setRotation(2);
    display.cp437(true);

    display.setTextSize(5);
    display.setTextColor(BLACK);
    display.clearDisplay();
    display.setCursor(0, 0);

    display.println("...");

    display.refresh();

    Serial.println("display cleared...");
}

void loop(void)
{
    const unsigned long now_millis = millis();
    checkLan(now_millis);
    checkMqtt(now_millis);
    switch(state)
    {
        case SketchState::Initializing:
        case SketchState::WaitingForLan:
        {
            handleWaitingForLan(now_millis);
        }
        break;
        case SketchState::Online:
        case SketchState::WaitingForMqtt:
        {
            handleWaitingForMqtt(now_millis);
        }
        break;
        case SketchState::BotnetOnline:
        {
            runBotnet(now_millis);
        }
        break;
        default:
        {
            Serial.print("ERROR: unknown state encountered ");
            Serial.println(static_cast<std::underlying_type<SketchState>::type>(state));
            while(1){}
        }
    }
}
