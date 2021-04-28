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
#ifndef BOTANYNET_BOTANYNET_INCLUDED_H
#define BOTANYNET_BOTANYNET_INCLUDED_H

#include <MqttClient.h>
#include <algorithm>
#include "HomeNet.h"

namespace BotanyNet
{

class Node
{
public:
    static constexpr size_t MaxBrokerUrlLen = 24u;

    Node(HomeNet& network, const char* const broker, int port)
        : m_net(network)
        , m_mqtt_client(network.getClient())
        , m_mqtt_broker{0}
        , m_mqtt_broker_port(port)
        , m_hostname_lookup_started(false)
        , m_mqtt_client_address(INADDR_NONE)
        , m_should_connect(false)
    {
        if (broker)
        {
            strncpy(m_mqtt_broker, broker, std::min(strlen(broker), MaxBrokerUrlLen) + 1);
        }
        // TODO: hostname TTL
    }

    void requestConnection()
    {
        
        m_should_connect = true;
    }

    void disconnect()
    {
        m_should_connect =  false;
        m_mqtt_client.stop();
        m_hostname_lookup_started = false;
    }

    bool isConnected()
    {
        return m_mqtt_client.connected();
    }

    void sendHumidity(float humidity)
    {
        sendFloat("botanynet/humidity", humidity);
    }

    void sendTemperatureC(float degressC)
    {
        sendFloat("botanynet/tempc", degressC);
    }

    void sendFloat(const char* const topic, float value)
    {
        // TODO: control topic prefix internally
        // TODO: json structure including nodeid
        // TODO: ensure is compatible with homeassistant plant cards.
        if (!m_mqtt_client.connected())
        {
            return;
        }
        if (!m_mqtt_client.beginMessage(topic))
        {
            Serial.println("beginMessage failed!");
        }
        else
        {
            m_mqtt_client.print(value);
            if (!m_mqtt_client.endMessage())
            {
                Serial.println("endMessage failed!");
            }
        }
    }

    void service(unsigned long now_millis)
    {
        (void)now_millis;
        // TODO: hostname TTL
        if (m_mqtt_client_address != INADDR_NONE)
        {
            if (m_should_connect)
            {
                connectNow();
            }
        }
        else if (!m_hostname_lookup_started)
        {
            if (HomeNet::Result::SUCCESS == m_net.startResolvingHostname(m_mqtt_broker))
            {
                m_hostname_lookup_started = true;
            }
        }
        else if (m_should_connect && !m_mqtt_client.connected())
        {
            if (HomeNet::Result::SUCCESS == m_net.getHostName(m_mqtt_broker, m_mqtt_client_address))
            {
                connectNow();
            }
        }
    }

private:
    void connectNow()
    {
        if (!m_mqtt_client.connect(m_mqtt_client_address, m_mqtt_broker_port))
        {
            const int error = m_mqtt_client.connectError();
            Serial.print("MQTT connection error (");
            Serial.print(error);
            Serial.println(')');
        }
        else
        {
            Serial.print("Connected to MQTT broker ");
            Serial.println(m_mqtt_broker);
            m_should_connect = false;
        }
    }

    HomeNet&   m_net;
    MqttClient m_mqtt_client;
    char       m_mqtt_broker[MaxBrokerUrlLen + 1];
    const int  m_mqtt_broker_port;
    bool       m_hostname_lookup_started;
    IPAddress  m_mqtt_client_address;
    bool       m_should_connect;
};

}  // namespace BotanyNet

#endif // BOTANYNET_BOTANYNET_INCLUDED_H
