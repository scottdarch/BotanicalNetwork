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

    Node(HomeNet& network, MqttClient& client, const char* const broker, int port)
        : m_net(network)
        , m_mqtt_client(client)
        , m_mqtt_broker{0}
        , m_mqtt_broker_port(port)
    {
        if (broker)
        {
            strncpy(m_mqtt_broker, broker, std::min(strlen(broker), MaxBrokerUrlLen) + 1);
        }
    }

    int connect()
    {
        m_net.resolveHost(m_mqtt_broker);
        int error = MQTT_SUCCESS;
        // if (!m_mqtt_client.connect(m_mqtt_broker, m_mqtt_broker_port))
        // {
        //     error = m_mqtt_client.connectError();
        // }
        return error;
    }

    bool connected()
    {
        return m_mqtt_client.connected();
    }

    void sendSoilHumidity(float humidity)
    {
        if (!m_mqtt_client.beginMessage("botanynet/soilhum"))
        {
            Serial.println("beginMessage failed!");
        }
        else
        {
            m_mqtt_client.print(humidity);
            if (!m_mqtt_client.endMessage())
            {
                Serial.println("endMessage failed!");
            }
        }
    }

private:
    HomeNet&   m_net;
    MqttClient m_mqtt_client;
    char       m_mqtt_broker[MaxBrokerUrlLen + 1];
    const int  m_mqtt_broker_port;
};

}  // namespace BotanyNet

#endif // BOTANYNET_BOTANYNET_INCLUDED_H
