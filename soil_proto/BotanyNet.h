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
#include <MqttClient.h>
#include <algorithm>

namespace BotanyNet
{

class Node
{
public:
    static constexpr size_t MaxBrokerUrlLen = 24u;

    Node(MqttClient& client, const char* const broker, int port)
        : m_mqtt_client(client)
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
        int error = 0;
        if (!m_mqtt_client.connect(m_mqtt_broker, m_mqtt_broker_port))
        {
            error = m_mqtt_client.connectError();
            Serial.print("MQTT connection failed! Error code = ");
            Serial.println(error);
        }
        return error;
    }

    void poll()
    {
        m_mqtt_client.poll();
    }

    void sendSoilHumidity(float humidity)
    {
        m_mqtt_client.beginMessage("botanynet/soilhum");
        m_mqtt_client.print(humidity);
        m_mqtt_client.endMessage();
    }
private:
    MqttClient m_mqtt_client;
    char m_mqtt_broker[MaxBrokerUrlLen + 1];
    const int m_mqtt_broker_port;
};

} // namespace BotanyNet