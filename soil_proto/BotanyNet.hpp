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
#ifndef BOTANYNET_BOTANYNET_INCLUDED_H
#define BOTANYNET_BOTANYNET_INCLUDED_H

#include <MqttClient.h>
#include <algorithm>
#include <cinttypes>
#include "HomeNet.hpp"

namespace BotanyNet
{

struct MonotonicClock
{
    virtual std::uint64_t getUptimeSeconds() const = 0;

protected:
    virtual ~MonotonicClock() = default;
};

/**
 * A node in our BOT(any)NET. Botnet notes are MQTT clients that chirp json.
 */
class Node
{
public:
    static constexpr size_t MaxBrokerUrlLen = 24u;
    static constexpr const char* BotnetChirpTemplate = "    {\
        \"node\": %" PRIu16 ",\
        \"diagnostic\": {\
            \"status\": %" PRIu16 ",\
            \"battery\": 255,\
            \"reserved_24\": 0,\
            \"uptime_sec\": 0,\
        }.\
        \"data\": \"%s\"\
    }";
    static constexpr size_t MaxTopicNameLen = 12u;
    static constexpr size_t MaxDataLen = 128u;
    static constexpr size_t BotnetChipBufferSize = strlen(BotnetChirpTemplate) + 13 + MaxDataLen + 1;

    Node(std::uint16_t nodeid, MonotonicClock& clock, HomeNet& network, const char* const broker, int port)
        : m_nodeid(nodeid)
        , m_clock(clock)
        , m_net(network)
        , m_mqtt_client(network.getClient())
        , m_mqtt_broker{0}
        , m_mqtt_broker_port(port)
        , m_hostname_lookup_started(false)
        , m_mqtt_client_address(INADDR_NONE)
        , m_should_connect(false)
        , m_topic_buffer{0}
        , m_data_buffer{0}
        , m_chirp_buffer{0}
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
        sendFloat("humidity", humidity);
    }

    void sendTemperatureC(float degressC)
    {
        sendFloat("tempc", degressC);
    }

    void sendFloat(const char* const topic, float value)
    {
        m_data_buffer[0] = 0;
        const int written_or_error = snprintf(m_data_buffer, MaxDataLen, "%f", value);
        // TODO: handle errors as node status bits
        if (written_or_error > 0)
        {
            sendData(topic, m_data_buffer);
        }
    }

    void sendData(const char* const topic, const char* const data)
    {
        if (!topic || strlen(topic) > MaxTopicNameLen)
        {
            return;
        }
        if (!data || strlen(data) > MaxDataLen)
        {
            return;
        }

        // TODO, handle buffer overflow
        snprintf(m_topic_buffer, MaxTopicNameLen + 6, "btnt/%s", topic);
        snprintf(m_chirp_buffer,
                 BotnetChipBufferSize,
                 BotnetChirpTemplate,
                 m_nodeid,
                 0u,
                 data);
        
        Serial.println(m_chirp_buffer);

        if (!m_mqtt_client.connected())
        {
            return;
        }
        if (!m_mqtt_client.beginMessage(m_topic_buffer))
        {
            Serial.println("beginMessage failed!");
        }
        else
        {
            m_mqtt_client.print(m_chirp_buffer);
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

    const std::uint16_t m_nodeid;
    MonotonicClock& m_clock;
    HomeNet&   m_net;
    MqttClient m_mqtt_client;
    char       m_mqtt_broker[MaxBrokerUrlLen + 1];
    const int  m_mqtt_broker_port;
    bool       m_hostname_lookup_started;
    IPAddress  m_mqtt_client_address;
    bool       m_should_connect;
    char       m_topic_buffer[MaxTopicNameLen + 6];
    char       m_data_buffer[MaxDataLen];
    char       m_chirp_buffer[BotnetChipBufferSize];
};

}  // namespace BotanyNet

#endif // BOTANYNET_BOTANYNET_INCLUDED_H
