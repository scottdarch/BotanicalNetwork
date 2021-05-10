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

#include "config.hpp"
#include "HomeNet.hpp"

namespace BotanyNet
{
struct MonotonicClock
{
    virtual unsigned long getUptimeSeconds() = 0;

protected:
    virtual ~MonotonicClock() = default;
};

/**
 * A node in our BOT(any)NET. Botnet notes are MQTT clients that chirp json.
 */
class Node
{
public:
    static constexpr size_t      MaxBrokerUrlLen     = 24u;
    static constexpr const char* BotnetChirpTemplate = "{\n"
                                                       "\t\"node\": %u,\n"
                                                       "\t\"diagnostic\": {\n"
                                                       "\t\t\"status\": %u,\n"
                                                       "\t\t\"battery\": %u,\n"
                                                       "\t\t\"reserved_16\": 0,\n"
                                                       "\t\t\"uptime_sec\": %lu\n"
                                                       "\t},\n"
                                                       "\t\"data\": %s\n"
                                                       "}";
    static constexpr size_t MaxTopicNameLen      = 12u;
    static constexpr size_t TopicNameBufferLen   = MaxTopicNameLen + 6u;
    static constexpr size_t MaxDataLen           = 128u;
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
        m_should_connect = false;
        m_mqtt_client.stop();
        m_hostname_lookup_started = false;
        m_mqtt_client_address = INADDR_NONE;
    }

    int isConnected()
    {
        return m_mqtt_client.connected();
    }

    int sendHumidity(float humidity)
    {
        return sendFloat("humidity", humidity);
    }

    int sendTemperatureC(float degressC)
    {
        return sendFloat("tempc", degressC);
    }

    int sendFloat(const char* const topic, float value)
    {
        m_data_buffer[0]           = 0;
        const int written_or_error = snprintf(m_data_buffer, MaxDataLen, "%.2f", value);
        if (written_or_error < 0)
        {
            BOTNET_SERIAL_DEBUG_PRINTLN("Unknown snprintf error writing data.");
            return written_or_error;
        }
        if (written_or_error >= MaxDataLen)
        {
            BOTNET_SERIAL_DEBUG_PRINTLN("Internal buffer overflow in data buffer.");
            return -5;
        }
        return sendData(topic, m_data_buffer);
    }

    int sendData(const char* const topic, const char* const data)
    {
        if (!topic || strlen(topic) > MaxTopicNameLen)
        {
            return -2;
        }
        if (!data || strlen(data) > MaxDataLen)
        {
            return -2;
        }

        const int topic_written = snprintf(m_topic_buffer, TopicNameBufferLen, "btnt/%s", topic);
        if (topic_written < 0)
        {
            BOTNET_SERIAL_DEBUG_PRINTLN("Unknown snprintf error writing topic.");
            return topic_written;
        }
        if (topic_written >= TopicNameBufferLen)
        {
            BOTNET_SERIAL_DEBUG_PRINTLN("Internal buffer overflow in topic name buffer.");
            return -5;
        }

        const int chirp_written = snprintf(m_chirp_buffer,
                                           BotnetChipBufferSize,
                                           BotnetChirpTemplate,
                                           m_nodeid,
                                           0u,
                                           0u,
                                           m_clock.getUptimeSeconds(),
                                           data);

        if (chirp_written < 0)
        {
            BOTNET_SERIAL_DEBUG_PRINTLN("Unknown snprintf error.");
            return chirp_written;
        }
        if (chirp_written >= BotnetChipBufferSize)
        {
            BOTNET_SERIAL_DEBUG_PRINTLN("Internal buffer overflow in output buffer.");
            return -5;
        }

        BOTNET_SERIAL_DEBUG_PRINT("Topic: ");
        BOTNET_SERIAL_DEBUG_PRINTLN(m_topic_buffer);
        BOTNET_SERIAL_DEBUG_PRINTLN(m_chirp_buffer);

        if (!m_mqtt_client.connected())
        {
            return -3;
        }
        if (!m_mqtt_client.beginMessage(m_topic_buffer, true))
        {
            BOTNET_SERIAL_DEBUG_PRINTLN("beginMessage failed!");
            return -3;
        }
        else
        {
            m_mqtt_client.print(m_chirp_buffer);
            if (!m_mqtt_client.endMessage())
            {
                BOTNET_SERIAL_DEBUG_PRINTLN("endMessage failed!");
                return -3;
            }
            return 0;
        }
    }

    void service(unsigned long now_millis)
    {
        (void) now_millis;
        // TODO: hostname TTL
        if (m_mqtt_client_address != INADDR_NONE)
        {
            if (m_should_connect)
            {
                connectNow();
            }
            else
            {
                m_mqtt_client.poll();
            }
        }
        else if (!m_hostname_lookup_started)
        {
            if (HomeNet::Result::SUCCESS == m_net.startResolvingHostname(m_mqtt_broker, true))
            {
                BOTNET_SERIAL_DEBUG_PRINT("Starting lookup for MQTT broker ");
                BOTNET_SERIAL_DEBUG_PRINTLN(m_mqtt_broker);
                m_hostname_lookup_started = true;
            }
        }
        else
        {
            m_net.getHostName(m_mqtt_broker, m_mqtt_client_address);
        }
    }

private:
    void printConnectionError(const int error)
    {
#ifdef BOTNET_ENABLE_SERIAL_INTERNAL_DEBUG
        if(error == MQTT_CONNECTION_REFUSED)
        {
            BOTNET_SERIAL_DEBUG_PRINT("MQTT_CONNECTION_REFUSED");
        }
        else if(error == MQTT_CONNECTION_TIMEOUT)
        {
            BOTNET_SERIAL_DEBUG_PRINT("MQTT_CONNECTION_TIMEOUT");
        }
        else if(error == MQTT_SUCCESS)
        {
            BOTNET_SERIAL_DEBUG_PRINT("MQTT_SUCCESS");
        }
        else if(error == MQTT_UNACCEPTABLE_PROTOCOL_VERSION)
        {
            BOTNET_SERIAL_DEBUG_PRINT("MQTT_UNACCEPTABLE_PROTOCOL_VERSION");
        }
        else if(error == MQTT_IDENTIFIER_REJECTED)
        {
            BOTNET_SERIAL_DEBUG_PRINT("MQTT_IDENTIFIER_REJECTED");
        }
        else if(error == MQTT_SERVER_UNAVAILABLE)
        {
            BOTNET_SERIAL_DEBUG_PRINT("MQTT_SERVER_UNAVAILABLE");
        }
        else if(error == MQTT_BAD_USER_NAME_OR_PASSWORD)
        {
            BOTNET_SERIAL_DEBUG_PRINT("MQTT_BAD_USER_NAME_OR_PASSWORD");
        }
        else if(error == MQTT_NOT_AUTHORIZED)
        {
            BOTNET_SERIAL_DEBUG_PRINT("MQTT_NOT_AUTHORIZED");
        }
#else
        (void)error;
#endif
    }

    int connectNow()
    {
        if (!m_mqtt_client.connect(m_mqtt_client_address, m_mqtt_broker_port))
        {
            const int error = m_mqtt_client.connectError();
            BOTNET_SERIAL_DEBUG_PRINT("MQTT connection error (");
            printConnectionError(error);
            BOTNET_SERIAL_DEBUG_PRINTLN(')');
            // Reset the client address in case this was an MDNS error.
            m_mqtt_client_address = INADDR_NONE;
            return error;
        }
        else
        {
            BOTNET_SERIAL_DEBUG_PRINT("Connected to MQTT broker ");
            BOTNET_SERIAL_DEBUG_PRINTLN(m_mqtt_broker);
            m_should_connect = false;
            return 0;
        }
    }

    const std::uint16_t m_nodeid;
    MonotonicClock&     m_clock;
    HomeNet&            m_net;
    MqttClient          m_mqtt_client;
    char                m_mqtt_broker[MaxBrokerUrlLen + 1];
    const int           m_mqtt_broker_port;
    bool                m_hostname_lookup_started;
    IPAddress           m_mqtt_client_address;
    bool                m_should_connect;
    char                m_topic_buffer[TopicNameBufferLen];
    char                m_data_buffer[MaxDataLen];
    char                m_chirp_buffer[BotnetChipBufferSize];
};

}  // namespace BotanyNet

#endif  // BOTANYNET_BOTANYNET_INCLUDED_H
