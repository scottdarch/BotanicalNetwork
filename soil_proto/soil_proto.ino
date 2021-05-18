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
#include <SAMD_AnalogCorrection.h>
#include <ArduinoECCX08.h>
#include <ECCX08.h>

#include "config.hpp"
#include "HomeNet.hpp"
#include "SoilProbe.hpp"
#include "BotanyNet.hpp"
#include "Terminal.hpp"

// +--------------------------------------------------------------------------+
// | TERMINAL
// +--------------------------------------------------------------------------+
using BotnetTerminal = BotanyNet::Terminal<typeof(Serial), 1>;

int commandMode(arduino::Stream& t, int argc, const char* argv[])
{
    t.print("called mode.");
    return 0;
}


static const char* const CommandHelpMode = "This is a test of our command structure.";

template<> BotnetTerminal BotnetTerminal::terminal(
    Serial,
    std::array<BotnetTerminal::CommandRecord, BotnetTerminal::NumberOfCommands>
    {
        {
            {
                "mode",
                commandMode,
                CommandHelpMode
            }
        }
    }
);


namespace
{
    BotnetTerminal& terminal = BotnetTerminal::terminal;
}

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
        terminal.print("Sleep for ");
        terminal.print(sleepTimeSeconds);
        terminal.println("sec.");

        if (!m_low_power_sleep)
        {
            delay(sleepTimeSeconds * 1000);
            digitalWrite(LED_BUILTIN, 1);
        }
        else
        {
            while(1)
            {
                const uint8_t now_sec = m_rtc.getSeconds();
                const uint8_t now_min = m_rtc.getMinutes();
                const uint8_t now_hours = m_rtc.getHours();
                m_rtc.setAlarmSeconds((now_sec + sleepTimeSeconds) % 60);
                m_rtc.setAlarmMinutes((now_min + (sleepTimeSeconds / 60) % 60));
                m_rtc.setAlarmHours((now_hours + (sleepTimeSeconds / 3600) % 3600));
                if (m_rtc.getMinutes() == now_min && m_rtc.getHours() == now_hours)
                {
                    break;
                }
                // Else the minute rolled over while we were setting the alarm
                // Try again.
            }
            m_rtc.standbyMode();
        }
    }

    // +----------------------------------------------------------------------+
    // | BotanyNet::MonotonicClock
    // +----------------------------------------------------------------------+
    virtual unsigned long getUptimeSeconds() override final
    {
        return m_rtc.getEpoch() - EPOCH_TIME_OFF;
    }

    // +----------------------------------------------------------------------+
    static RTCMonotonicClock singleton;

private:
    RTCZero m_rtc;
    const bool m_low_power_sleep;
};

RTCMonotonicClock RTCMonotonicClock::singleton(EnableLowPowerMode);

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
BotanyNet::SoilProbe probe(PinSoilProbePower, PinSoilProbeADC);

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
        terminal.println("LAN is not connected. Starting over...");
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
        terminal.println("Connecting to LAN...");
        const int result = homenet.connect();
        terminal.print("Connection result (");
        terminal.print(result);
        terminal.println(')');
        state = SketchState::WaitingForLan;
    }
    else if (wifi_status == WL_CONNECTED)
    {
        terminal.println("Connected to LAN.");
        homenet.printStatus(Serial);
        homenet.printCurrentNet(Serial);
        homenet.printWifiData(Serial);
        state = SketchState::Online;
    }
    else
    {
        homenet.printStatus(Serial);
        terminal.print("wifi not connected (");
        terminal.print(wifi_status);
        terminal.println(")...");
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
        terminal.println("MQTT connect was closed. Reconnecting...");
        state = SketchState::Online;
    }
}

void handleWaitingForMqtt(const unsigned long now_millis)
{
    (void) now_millis;
    if (state == SketchState::Online)
    {
        terminal.println("Trying to connect to botnet...");
        bnclient.requestConnection();
        state = SketchState::WaitingForMqtt;
    }
    else if (bnclient.isConnected())
    {
        terminal.println("connected to botnet.");
        state = SketchState::BotnetOnline;
    }
}

static unsigned long last_update_at_millis = 0;

void runBotnet(const unsigned long now_millis)
{
    if (now_millis - last_update_at_millis > 1000)
    {
        terminal.println("Reading...");
        const float humidity            = sht1x.readHumidity();
        const float soil                = probe.readSoil();
        const float temperature_celcius = sht1x.readTemperatureC();
        bnclient.sendHumidity(humidity);
        bnclient.sendTemperatureC(temperature_celcius);
        bnclient.sendFloat("soilr", soil);
    }
    digitalWrite(LED_BUILTIN, 0);
    if (EnableLowPowerMode)
    {
        // Shutdown the wifi to save power. Our state machine will detect
        // this and reconnect after we wake up.
        homenet.shutdown();
        probe.stop();
    }
    clock.sleep(SampleDelaySec);
    if (EnableLowPowerMode)
    {
        probe.start();
        terminal.begin(115200);
    }
    digitalWrite(LED_BUILTIN, 1);
}

// +--------------------------------------------------------------------------+
// | ARDUINO
// +--------------------------------------------------------------------------+

void setup(void)
{
    terminal.begin(115200);

    probe.start();
    // (thanks https://www.element14.com/community/community/project14/iot-in-the-cloud/blog/2019/05/27/the-windchillator-reducing-the-sleep-current-of-the-arduino-mkr-wifi-1010-to-800-ua)
    // Turn off crypto part to save power.
    ECCX08.begin();
    ECCX08.end();

    terminal.startupDelay();

    terminal.print("Starting prototype bot(any)net node ");
    terminal.println(BotnetNodeId);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, 0);

    clock.begin();

    terminal.println("Sensor started...");
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
        terminal.print("ERROR: unknown state encountered ");
        terminal.println(static_cast<std::underlying_type<SketchState>::type>(state));
        while (1)
        {}
    }
    }
}
