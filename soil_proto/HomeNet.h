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
//  Parts of this class adapted from https://www.arduino.cc/en/Tutorial/LibraryExamples/ConnectWithWPA
//  by Tom Igoe
//
#ifndef BOTANYNET_HOMENET_INCLUDED_H
#define BOTANYNET_HOMENET_INCLUDED_H

#include <WiFi101.h>
#include <WiFiUdp.h>
#include <algorithm>
#include <ArduinoMDNS.h>

#ifndef BOTANYNET_XSTR
#    define BOTANYNET_XSTR(s) BOTANYNET_STR(s)
#    define BOTANYNET_STR(s) #    s
#endif

namespace BotanyNet
{
/**
 * See comments on the `singleton` class member for how to include this object
 * into your sketch.
 */
class HomeNet final
{
    HomeNet()
        : m_udp()
        , m_mdns(m_udp)
        , m_mdns_init(false)
    {
        WiFi.setTimeout(10000);
    }

public:
    /**
     * Forms a botany newtwork node name to report via mDNS.
     */
    static constexpr const char* NodeName = "botnet.node" BOTANYNET_STR(BOTNET_NODEID);

    /**
     * These are µCs. Don't use overly flowery hostnames, okay?
     */
    static constexpr size_t MaxHostNameLen = 64;

    enum class Result : int
    {
        SUCCESS = 1,
        OKAY_FALSE = 0,
        INVALID_ARGUMENT = -2,
        INVALID_STATE = -3,
        UNKNOWN_ERROR = -4
    };

private:
    /**
     * Used for the one-at-a-time host name resolution.
     */
    struct HostNameRecord
    {
        char hostname[MaxHostNameLen + 1];
        IPAddress  addr;
    };

public:
    uint8_t connect()
    {
        return WiFi.begin();
    }

    void printCurrentNet() const
    {
        // print the SSID of the network you're attached to:
        Serial.print("SSID: ");
        Serial.println(WiFi.SSID());

        // print the MAC address of the router you're attached to:
        byte bssid[6];
        WiFi.BSSID(bssid);
        Serial.print("BSSID: ");
        Serial.print(bssid[5], HEX);
        Serial.print(":");
        Serial.print(bssid[4], HEX);
        Serial.print(":");
        Serial.print(bssid[3], HEX);
        Serial.print(":");
        Serial.print(bssid[2], HEX);
        Serial.print(":");
        Serial.print(bssid[1], HEX);
        Serial.print(":");
        Serial.println(bssid[0], HEX);

        // print the received signal strength:
        long rssi = WiFi.RSSI();
        Serial.print("signal strength (RSSI):");
        Serial.println(rssi);

        // print the encryption type:
        byte encryption = WiFi.encryptionType();
        Serial.print("Encryption Type:");
        Serial.println(encryption, HEX);
        Serial.println();
    }

    void printWifiData() const
    {
        // print your WiFi shield's IP address:
        IPAddress ip = WiFi.localIP();
        Serial.print("IP Address: ");
        Serial.println(ip);
        Serial.println(ip);

        // print your MAC address:
        byte mac[6];
        WiFi.macAddress(mac);
        Serial.print("MAC address: ");
        Serial.print(mac[5], HEX);
        Serial.print(":");
        Serial.print(mac[4], HEX);
        Serial.print(":");
        Serial.print(mac[3], HEX);
        Serial.print(":");
        Serial.print(mac[2], HEX);
        Serial.print(":");
        Serial.print(mac[1], HEX);
        Serial.print(":");
        Serial.println(mac[0], HEX);
    }

    uint8_t getStatus() const
    {
        return WiFi.status();
    }

    void printStatus() const
    {
        Serial.println(statusString(WiFi.status()));
    }

    void service(unsigned long now_millis)
    {
        (void)now_millis;
        if (!m_mdns_init && WiFi.status() == WL_CONNECTED)
        {
            m_mdns.begin(WiFi.localIP(), HomeNet::NodeName);
            m_mdns.setNameResolvedCallback(HomeNet::mdnsCallback);
            m_mdns_init = true;
            Serial.println("MDNS is running.");
        }
        if (m_mdns_init)
        {
            m_mdns.run();
        }
    }

    Result startResolvingHostname(const char* const hostname)
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
        m_hostname_record.addr = INADDR_NONE;

        m_mdns.resolveName(hostname, 5000);
        return Result::SUCCESS;
    }

    Result getHostName(const char* const hostname, IPAddress& outAddr) const
    {
        if (hostname && strncmp(hostname, m_hostname_record.hostname, MaxHostNameLen) == 0)
        {
            outAddr = m_hostname_record.addr;
            return Result::SUCCESS;
        }
        else
        {
            return Result::OKAY_FALSE;
        }
    }

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
        case WL_PROVISIONING:
            return "WL_PROVISIONING";
        case WL_PROVISIONING_FAILED:
            return "WL_PROVISIONING_FAILED";
        default:
            return "(unknown status)";
        }
    }

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
    void onNameFound(const char* hostname, const IPAddress& ip)
    {
        if (!hostname)
        {
            Serial.println("Unknown error (name ptr was null?)");
        }
        else if (ip != INADDR_NONE)
        {
            Serial.print("The IP address for '");
            Serial.print(hostname);
            Serial.print("' is ");
            Serial.println(ip);
            const size_t hostNameLen = strlen(hostname);
            strncpy(m_hostname_record.hostname, hostname, std::min(hostNameLen, MaxHostNameLen) + 1);
            m_hostname_record.addr = ip;
        }
        else
        {
            Serial.print("Resolving '");
            Serial.print(hostname);
            Serial.println("' timed out.");
        }
    }

    static void mdnsCallback(const char* name, IPAddress ip)
    {
        HomeNet::singleton.onNameFound(name, ip);
    }

    WiFiUDP        m_udp;
    MDNS           m_mdns;
    bool           m_mdns_init;
    HostNameRecord m_hostname_record;
};

}  // namespace BotanyNet

#endif  // BOTANYNET_HOMENET_INCLUDED_H
