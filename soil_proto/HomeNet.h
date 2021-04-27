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
#include <MDNS_Generic.h>

namespace BotanyNet
{
void nameFound(const char* name, IPAddress ip)
{
    if (ip != INADDR_NONE)
    {
        Serial.print("The IP address for '");
        Serial.print(name);
        Serial.print("' is ");
        Serial.println(ip);
    }
    else
    {
        Serial.print("Resolving '");
        Serial.print(name);
        Serial.println("' timed out.");
    }
}

class HomeNet final
{
public:
    HomeNet()
        : m_udp()
        , m_mdns(m_udp)
        , m_mdns_init(false)
    {
        WiFi.setTimeout(10000);
    }

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

    int ping(const char* const url) const
    {
        return WiFi.ping(url);
    }

    uint8_t getStatus() const
    {
        return WiFi.status();
    }

    void printStatus() const
    {
        Serial.println(statusString(WiFi.status()));
    }

    void service()
    {
        if (!m_mdns_init && WiFi.status() == WL_CONNECTED)
        {
            m_mdns.begin(WiFi.localIP(), "botnode1");
            m_mdns.setNameResolvedCallback(nameFound);
            m_mdns_init = true;
            Serial.println("MDNS is running.");
        }
        if (m_mdns_init)
        {
            m_mdns.run();
        }
    }

    void resolveHost(const char* const hostname)
    {
        if (m_mdns_init && !m_mdns.isResolvingName())
        {
            m_mdns.resolveName(hostname, 5000);
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

private:
    WiFiUDP m_udp;
    MDNS    m_mdns;
    bool    m_mdns_init;
};

}  // namespace BotanyNet

#endif // BOTANYNET_HOMENET_INCLUDED_H
