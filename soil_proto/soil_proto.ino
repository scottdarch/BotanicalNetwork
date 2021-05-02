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

#include <type_traits>
#include <Arduino.h>
#include <SHT1x.h>
#include <ArduinoMqttClient.h>
#include <RTCZero.h>

#include "config.hpp"
#include "HomeNet.hpp"
#include "SoilProbe.hpp"
#include "BotanyNet.hpp"

// +--------------------------------------------------------------------------+
// | NETWORKING
// +--------------------------------------------------------------------------+
// NOTE: To setup access to wifi use the arduino ConnectWithWPA example once.
// This will write the SSID and key into NVM.

// HomeNet singleton storage.
BotanyNet::HomeNet BotanyNet::HomeNet::singleton;

// alias the singleton to something nice to look at.
constexpr BotanyNet::HomeNet& homenet = BotanyNet::HomeNet::singleton;

// +--------------------------------------------------------------------------+
// | TIME KEEPING AND POWER
// +--------------------------------------------------------------------------+
void alarmWake()
{
    __NOP;
}

/**
 * Implement the Bot(any)Net clock using RTCZero.
 */
class RTCMonotonicClock : public BotanyNet::MonotonicClock
{
public:
    RTCMonotonicClock(bool enableLowPowerSleep)
        : m_rtc()
        , m_low_power_sleep(enableLowPowerSleep)
    {}

    void begin()
    {
        m_rtc.begin(true);

        if (m_low_power_sleep)
        {
            m_rtc.enableAlarm(m_rtc.MATCH_SS);
            m_rtc.attachInterrupt(alarmWake);
        }
    }

    void sleep(const uint8_t sleepTimeSeconds)
    {
        Serial.print("Sleep for ");
        Serial.print(sleepTimeSeconds);
        Serial.println("sec.");

        if (!m_low_power_sleep)
        {
            delay(sleepTimeSeconds * 1000);
            digitalWrite(LED_BUILTIN, 1);
        }
        else
        {
            const uint8_t now_sec = m_rtc.getSeconds();
            m_rtc.setAlarmSeconds((now_sec + sleepTimeSeconds) % 60);
            m_rtc.standbyMode();
        }
    }

    // +----------------------------------------------------------------------+
    // | BotanyNet::MonotonicClock
    // +----------------------------------------------------------------------+
    virtual unsigned long getUptimeSeconds() override final
    {
        return m_rtc.getEpoch();
    }

    // +----------------------------------------------------------------------+
    static RTCMonotonicClock singleton;

private:
    RTCZero m_rtc;
    const bool m_low_power_sleep;
};

RTCMonotonicClock RTCMonotonicClock::singleton(!DisableSleep);

// Nice name for our clock.
constexpr RTCMonotonicClock& clock = RTCMonotonicClock::singleton;

// Setup our BotanyNet client.
BotanyNet::Node bnclient(BotnetNodeId, RTCMonotonicClock::singleton, homenet, BotnetBroker, BotnetPort);

// +--------------------------------------------------------------------------+
// | SENSORS AND DISPLAYS
// +--------------------------------------------------------------------------+

// Soil humidity and temperature sensor (fancy)
SHT1x sht1x(PinSht1Data, PinSht1Clk);

// Soil probe using resistence (cheapo)
BotanyNet::SoilProbe probe(PinSoilProbeADC, PinSoilProbePower);

// +--------------------------------------------------------------------------+
// | SKETCH
// +--------------------------------------------------------------------------+

enum class SketchState : int
{
    Initializing   = 0,
    WaitingForLan  = 1,
    Online         = 2,
    WaitingForMqtt = 3,
    BotnetOnline   = 4
};

static SketchState state = SketchState::Initializing;

void checkLan(const unsigned long now_millis)
{
    homenet.service(now_millis);
    if (homenet.getStatus() != WL_CONNECTED)
    {
        Serial.println("LAN is not connected. Starting over...");
        homenet.printStatus(Serial);
        bnclient.disconnect();
        state = SketchState::Initializing;
    }
}

void handleWaitingForLan(const unsigned long now_millis)
{
    (void) now_millis;
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
        homenet.printStatus(Serial);
        homenet.printCurrentNet(Serial);
        homenet.printWifiData(Serial);
        state = SketchState::Online;
    }
    else
    {
        homenet.printStatus(Serial);
        Serial.print("wifi not connected (");
        Serial.print(wifi_status);
        Serial.println(")...");
        digitalWrite(LED_BUILTIN, 1);
        delay(250);
        digitalWrite(LED_BUILTIN, 0);
        delay(250);
    }
}

void checkMqtt(const unsigned long now_millis)
{
    bnclient.service(now_millis);
    if (state == SketchState::BotnetOnline && !bnclient.isConnected())
    {
        Serial.println("MQTT connect was closed. Reconnecting...");
        state = SketchState::Online;
    }
}

void handleWaitingForMqtt(const unsigned long now_millis)
{
    (void) now_millis;
    if (state == SketchState::Online)
    {
        Serial.println("Trying to connect to botnet...");
        bnclient.requestConnection();
        state = SketchState::WaitingForMqtt;
    }
    else if (state == SketchState::WaitingForMqtt)
    {
        if (bnclient.isConnected())
        {
            Serial.println("connected to botnet.");
            state = SketchState::BotnetOnline;
        }
    }
}

static unsigned long last_update_at_millis = 0;

void runBotnet(const unsigned long now_millis)
{
    if (now_millis - last_update_at_millis > 1000)
    {
        Serial.println("Reading...");
        const float humidity            = sht1x.readHumidity();
        const int   soil                = probe.readSoil();
        const float temperature_celcius = sht1x.readTemperatureC();
        bnclient.sendHumidity(humidity);
        bnclient.sendTemperatureC(temperature_celcius);
        bnclient.sendFloat("soilr", static_cast<float>(soil));
    }
    digitalWrite(LED_BUILTIN, 0);
    clock.sleep(SampleDelaySec);
    digitalWrite(LED_BUILTIN, 1);
}

// +--------------------------------------------------------------------------+
// | ARDUINO
// +--------------------------------------------------------------------------+

void setup(void)
{
    Serial.begin(115200);
#ifdef BOTNET_ENABLE_SERIAL_INTERNAL_DEBUG
    const unsigned long delay_start = millis();
    while (!Serial && (delay_start + StartupDelayMillis) > millis())
    {
        digitalWrite(LED_BUILTIN, 1);
        delay(500);
        digitalWrite(LED_BUILTIN, 0);
        delay(500);
    }
#endif
    Serial.print("Starting prototype bot(any)net node ");
    Serial.println(BotnetNodeId);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, 0);

    clock.begin();

    Serial.println("Sensor started...");
}

void loop(void)
{
    const unsigned long now_millis = millis();
    checkLan(now_millis);
    checkMqtt(now_millis);
    switch (state)
    {
    case SketchState::Initializing:
    case SketchState::WaitingForLan: {
        handleWaitingForLan(now_millis);
    }
    break;
    case SketchState::Online:
    case SketchState::WaitingForMqtt: {
        handleWaitingForMqtt(now_millis);
    }
    break;
    case SketchState::BotnetOnline: {
        runBotnet(now_millis);
    }
    break;
    default: {
        Serial.print("ERROR: unknown state encountered ");
        Serial.println(static_cast<std::underlying_type<SketchState>::type>(state));
        while (1)
        {}
    }
    }
}
