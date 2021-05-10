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
//  Parts of this class adapted from https://www.arduino.cc/en/Tutorial/LibraryExamples/ConnectWithWPA
//  by Tom Igoe
//
#ifndef BOTANYNET_HOMENET_INCLUDED_H
#define BOTANYNET_HOMENET_INCLUDED_H

#include <WiFiNINA.h>
#include <utility/wifi_drv.h>
#include <WiFiUdp.h>
#include <algorithm>
#include <ArduinoMDNS.h>

#include "config.hpp"
#include "arduino_secrets.h"

namespace BotanyNet
{
/**
 * Singleton controller of a home network. This version assumes WIFI with MDNS
 * nodes available.
 *
 * See comments on the `singleton` class member for how to include this object
 * into your sketch.
 */
class HomeNet final
{
    HomeNet()
        : m_wifi_client()
        , m_udp()
        , m_mdns(m_udp)
        , m_mdns_init(false)
    {
        WiFi.setTimeout(WiFiTimeoutMillis);
    }

public:
    /**
     * Forms a botany newtwork node name to report via mDNS.
     */
    static constexpr const char* NodeName = "botnet.node" BOTANYNET_XSTR(BotnetNodeId);

    /**
     * These are µCs. Don't use overly flowery hostnames, okay?
     */
    static constexpr size_t MaxHostNameLen = 64;

    enum class Result : int
    {
        SUCCESS          = 1,
        OKAY_FALSE       = 0,
        INVALID_ARGUMENT = -2,
        INVALID_STATE    = -3,
        UNKNOWN_ERROR    = -4
    };

private:
    /**
     * Used for the one-at-a-time host name resolution.
     */
    struct HostNameRecord
    {
        char      hostname[MaxHostNameLen + 1];
        IPAddress addr;
    };

public:
    uint8_t connect()
    {
        WiFiDrv::pinMode(WiFiNINAPinLEDRed, OUTPUT);
        WiFiDrv::pinMode(WiFiNINAPinLEDGreen, OUTPUT);
        WiFiDrv::pinMode(WiFiNINAPinLEDBlue, OUTPUT);

        WiFiDrv::digitalWrite(WiFiNINAPinLEDRed, LOW);
        WiFiDrv::digitalWrite(WiFiNINAPinLEDGreen, LOW);
        WiFiDrv::analogWrite(WiFiNINAPinLEDBlue, 15);
#ifdef BOTNET_ENABLE_SERIAL_INTERNAL_DEBUG
        {
            arduino::String fv = WiFi.firmwareVersion();
            if (fv < WIFI_FIRMWARE_LATEST_VERSION)
            {
                BOTNET_SERIAL_DEBUG_PRINT("Please upgrade the firmware from ");
                BOTNET_SERIAL_DEBUG_PRINT(fv);
                BOTNET_SERIAL_DEBUG_PRINT(" to ");
                BOTNET_SERIAL_DEBUG_PRINTLN(WIFI_FIRMWARE_LATEST_VERSION);
            }
        }
#endif

        WiFi.setHostname(HomeNet::NodeName);

        // attempt to connect to WiFi network:
        return WiFi.begin(SECRET_SSID, SECRET_PASS);
    }

    uint8_t getStatus() const
    {
        const uint8_t realStatus = WiFi.status();
        if (WL_CONNECTED == realStatus && !m_mdns_init)
        {
            // Report idle while waiting for mdns since we are pretending that
            // MDNS comes with a wifi connection.
            return WL_IDLE_STATUS;
        }
        return realStatus;
    }

    void service(unsigned long now_millis)
    {
        (void) now_millis;
        const int wifi_status = WiFi.status();
        if (m_mdns_init)
        {
            m_mdns.run();
        }
        else if (wifi_status == WL_CONNECTED)
        {
            WiFiDrv::analogWrite(WiFiNINAPinLEDBlue, 0);
            WiFiDrv::digitalWrite(WiFiNINAPinLEDBlue, LOW);
            m_mdns.begin(WiFi.localIP(), HomeNet::NodeName);
            m_mdns.setNameResolvedCallback(HomeNet::mdnsCallback);
            m_mdns_init = true;
            BOTNET_SERIAL_DEBUG_PRINTLN("MDNS is running.");
            if (EnableLowPowerMode)
            {
                WiFi.lowPowerMode();
            }
            else
            {
                WiFiDrv::analogWrite(WiFiNINAPinLEDGreen, 50);
                WiFi.noLowPowerMode();
            }
        }
        else
        {
            BOTNET_SERIAL_DEBUG_PRINT("Reason code: ");
            BOTNET_SERIAL_DEBUG_PRINTLN(WiFi.reasonCode());
        }
    }

    Result startResolvingHostname(const char* const hostname, bool isLocal)
    {
        if (!hostname)
        {
            // invalid argument
            return Result::INVALID_ARGUMENT;
        }
        const size_t hostNameLen = strlen(hostname);

        if (hostNameLen > MaxHostNameLen)
        {
            // Can't look this up because it's null or too long.
            return Result::INVALID_ARGUMENT;
        }
        if (!m_mdns_init)
        {
            // Called too soon
            return Result::INVALID_STATE;
        }
        if (m_mdns.isResolvingName())
        {
            m_mdns.cancelResolveName();
        }

        // Reset any previously cached record.
        m_hostname_record.hostname[0] = 0;
        m_hostname_record.addr        = INADDR_NONE;

        Result methodResult = Result::UNKNOWN_ERROR;

        if (isLocal)
        {
            const int result = m_mdns.resolveName(hostname, MDNSTimeoutMillis);
            if (1 == result)
            {
                methodResult = Result::SUCCESS;
            }
            else
            {
                BOTNET_SERIAL_DEBUG_PRINT("MDNS returned some sort of error (");
                BOTNET_SERIAL_DEBUG_PRINT(result);
                BOTNET_SERIAL_DEBUG_PRINT(") when resolving: ");
                BOTNET_SERIAL_DEBUG_PRINTLN(hostname);
            }
        }
        else
        {
            const int result = WiFi.hostByName(hostname, m_hostname_record.addr);

            if (1 == result)
            {
                strncpy(m_hostname_record.hostname, hostname, hostNameLen + 1);
                methodResult = Result::SUCCESS;
            }
            else
            {
                BOTNET_SERIAL_DEBUG_PRINT("hostByName returned some sort of error (");
                BOTNET_SERIAL_DEBUG_PRINT(result);
                BOTNET_SERIAL_DEBUG_PRINT(") when resolving: ");
                BOTNET_SERIAL_DEBUG_PRINTLN(hostname);
            }
        }
        return methodResult;
    }

    Result getHostName(const char* const hostname, IPAddress& outAddr) const
    {
        if (hostname && strncmp(hostname, m_hostname_record.hostname, std::min(strlen(hostname), MaxHostNameLen)) == 0)
        {
            outAddr = m_hostname_record.addr;
            return Result::SUCCESS;
        }
        else
        {
            return Result::OKAY_FALSE;
        }
    }

    arduino::Client& getClient()
    {
        return m_wifi_client;
    }

    void shutdown()
    {
        m_udp.flush();
        // since flush isn't actually implemented we have this hack...
        delay(100);
        m_udp.stop();
        WiFi.end();
        m_mdns_init = false;
    }

    // +----------------------------------------------------------------------+
    // | DEBUG FACILITIES
    // +----------------------------------------------------------------------+
    static const char* statusString(const uint8_t status)
    {
        switch (status)
        {
        case WL_NO_SHIELD:
            return "WL_NO_SHIELD";
        case WL_IDLE_STATUS:
            return "WL_IDLE_STATUS";
        case WL_NO_SSID_AVAIL:
            return "WL_NO_SSID_AVAIL";
        case WL_SCAN_COMPLETED:
            return "WL_SCAN_COMPLETED";
        case WL_CONNECTED:
            return "WL_CONNECTED";
        case WL_CONNECT_FAILED:
            return "WL_CONNECT_FAILED";
        case WL_CONNECTION_LOST:
            return "WL_CONNECTION_LOST";
        case WL_DISCONNECTED:
            return "WL_DISCONNECTED";
        case WL_AP_LISTENING:
            return "WL_AP_LISTENING";
        case WL_AP_CONNECTED:
            return "WL_AP_CONNECTED";
        case WL_AP_FAILED:
            return "WL_AP_FAILED";
        default:
            return "(unknown status)";
        }
    }

    void printCurrentNet(arduino::Stream& printto) const
    {
        // print the SSID of the network you're attached to:
        printto.print("SSID: ");
        printto.println(WiFi.SSID());

        // print the MAC address of the router you're attached to:
        byte bssid[6];
        WiFi.BSSID(bssid);
        printto.print("BSSID: ");
        printto.print(bssid[5], HEX);
        printto.print(":");
        printto.print(bssid[4], HEX);
        printto.print(":");
        printto.print(bssid[3], HEX);
        printto.print(":");
        printto.print(bssid[2], HEX);
        printto.print(":");
        printto.print(bssid[1], HEX);
        printto.print(":");
        printto.println(bssid[0], HEX);

        // print the received signal strength:
        long rssi = WiFi.RSSI();
        printto.print("signal strength (RSSI):");
        printto.println(rssi);

        // print the encryption type:
        byte encryption = WiFi.encryptionType();
        printto.print("Encryption Type:");
        printto.println(encryption, HEX);
        printto.println();
    }

    void printWifiData(arduino::Stream& printto) const
    {
        // print your WiFi shield's IP address:
        IPAddress ip = WiFi.localIP();
        printto.print("IP Address: ");
        printto.println(ip);
        printto.println(ip);

        // print your MAC address:
        byte mac[6];
        WiFi.macAddress(mac);
        printto.print("MAC address: ");
        printto.print(mac[5], HEX);
        printto.print(":");
        printto.print(mac[4], HEX);
        printto.print(":");
        printto.print(mac[3], HEX);
        printto.print(":");
        printto.print(mac[2], HEX);
        printto.print(":");
        printto.print(mac[1], HEX);
        printto.print(":");
        printto.println(mac[0], HEX);
    }

    void printStatus(arduino::Stream& printto) const
    {
        printto.println(statusString(WiFi.status()));
    }

    // +----------------------------------------------------------------------+
    // | SINGLETON
    // +----------------------------------------------------------------------+
    /**
     * You must declare the storage space for this in your sketch. For example:
     * ```
     *     // HomeNet singleton storage.
     *     BotanyNet::HomeNet BotanyNet::HomeNet::singleton;
     *
     *     // alias the singleton to something if you want to avoid the clunkly
     *     // static access syntax.
     *     constexpr BotanyNet::HomeNet& my_home_network = BotanyNet::HomeNet::singleton;
     * ```
     * Why?
     * Because you don't want your firmware to be executing constructors in include statements.
     * This is particularly bad because subsequent includes can alias things after the class
     * is constructed and the order that the singleton is constructed can change based
     * on include guards and which other header is first to include this one. Yes Arduino
     * does this a lot but it's just a bad plan.
     */
    static HomeNet singleton;

private:
    int onNameFound(const char* hostname, const IPAddress& ip)
    {
        if (!hostname)
        {
            BOTNET_SERIAL_DEBUG_PRINTLN("Unknown error (name ptr was null?)");
            return -2;
        }
        else if (ip != INADDR_NONE)
        {
            BOTNET_SERIAL_DEBUG_PRINT("The IP address for '");
            BOTNET_SERIAL_DEBUG_PRINT(hostname);
            BOTNET_SERIAL_DEBUG_PRINT("' is ");
            BOTNET_SERIAL_DEBUG_PRINTLN(ip);
            const size_t hostNameLen = strlen(hostname);
            strncpy(m_hostname_record.hostname, hostname, std::min(hostNameLen, MaxHostNameLen) + 1);
            m_hostname_record.addr = ip;
            return 0;
        }
        else
        {
            BOTNET_SERIAL_DEBUG_PRINT("Resolving '");
            BOTNET_SERIAL_DEBUG_PRINT(hostname);
            BOTNET_SERIAL_DEBUG_PRINTLN("' timed out.");
            return -1;
        }
    }

    static void mdnsCallback(const char* name, IPAddress ip)
    {
        HomeNet::singleton.onNameFound(name, ip);
    }

    WiFiClient     m_wifi_client;
    WiFiUDP        m_udp;
    MDNS           m_mdns;
    bool           m_mdns_init;
    HostNameRecord m_hostname_record;
};

}  // namespace BotanyNet

#endif  // BOTANYNET_HOMENET_INCLUDED_H
