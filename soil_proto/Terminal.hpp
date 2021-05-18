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
#ifndef BOTANYNET_TERMINAL_INCLUDED_H
#define BOTANYNET_TERMINAL_INCLUDED_H

#include <Stream.h>
#include <array>

#include "config.hpp"

namespace BotanyNet
{
///
/// A simple terminal program for interacting with a Botnet sensor node over m_serial.
///
template <typename SerialType, size_t CommandCount>
class Terminal : public arduino::Stream
{
public:
    ///
    /// The maximum time the service method will use...mostly.
    /// This class tries to keep this deadline but Arduino is not a realtime
    /// framework so calls into other objects may cause us to exceed our
    /// deadline.
    ///
    static constexpr unsigned long ServiceTimeoutMillis = 800;

    ///
    /// The maximum number of characters for any one command. This limits the
    /// size of internal command buffers and the keys for command lookup tables.
    ///
    static constexpr size_t MaxCommandLength = 10;

    ///
    /// The maximum length of a single line of the terminal.
    ///
    static constexpr size_t LineBufferLength = 100;

    ///
    /// Input character that will trigger the parsing of a complete line.
    ///
    static constexpr char LineDelimeter = '\n';

    ///
    /// Each registered command gets invoked like a mini program.
    ///
    using Command = int (*)(arduino::Stream& t, int argc, const char* argv[]);

    ///
    /// The number of commands stored in this object.
    ///
    static constexpr size_t NumberOfCommands = CommandCount;

    struct CommandRecord
    {
        const char        name[MaxCommandLength];
        Command           command;
        const char* const help;
    };

private:
    Terminal(SerialType& serial, std::array<CommandRecord, CommandCount> &&l)
        : m_commands(l)
        , m_serial(serial)
        , m_alert_led(0)
        , m_line_buffer(nullptr)
        , m_last_command(nullptr)
        , m_line_buffer_fill{0}
    {}

    virtual ~Terminal()
    {
        free(m_line_buffer);
    }

public:
    ///
    /// Call to start the serial device and to allocate internal buffers.
    ///
    void begin(uint32_t baudrate)
    {
        if (nullptr == m_line_buffer)
        {
            initInternalBuffers();
        }
        m_serial.begin(baudrate);
        if (m_alert_led != 0)
        {
            pinMode(m_alert_led, OUTPUT);
            digitalWrite(m_alert_led, LOW);
        }
    }

    ///
    /// End the serial device and free internal buffers.
    ///
    void end()
    {
        m_serial.end();
        free(m_line_buffer);
        m_line_buffer = nullptr;
    }

    ///
    /// Method should be called in setup. This blocks for a few seconds to allow a user
    /// to interrupt the program and start an interactive session instead.
    ///
    void startupDelay()
    {
        unsigned long       now         = millis();
        const unsigned long delay_until = StartupDelayMillis + now;
        while (!m_serial && delay_until > now)
        {
            delayForOneSecond();
            now = millis();
        }

        while (delay_until > now)
        {
            clearLine();
            home();
            print("Starting program in ");
            print((delay_until - now) / 1000U);
            print(" seconds...");

            if (delayForOneSecond())
            {
                home();
                clearLine();
                println("Interrupted startup. Reset to continue.");
                nextLine();
                home();
                prompt();
                while (true)
                {
                    if (service())
                    {
                        clearLine();
                        previousLine();
                        clearLine();
                        nextLine();
                        nextLine();
                        home();
                        handleCommand(true);
                        previousLine();
                        home();
                        prompt();
                    }
                }
            }
            now = millis();
        }
    }

    ///
    /// Call to give CPU time to the terminal.
    ///
    /// @return true if command delimeter was read else false.
    ///
    bool service()
    {
        unsigned long       now      = millis();
        const unsigned long deadline = now + ServiceTimeoutMillis;
        while (deadline > now && m_serial.available())
        {
            const char read = m_serial.read();
            if (read == LineDelimeter)
            {
                readCommand(m_last_command);
                home();
                clearToEndOfLine();
                clearLineBuffer();
                return true;
            }
            else if (!handleControlChar(read))
            {
                // This was not a control character. Treat as input.
                if (!appendToLineBuffer(read))
                {
                    previousLine();
                    print("\e[41mBELL\e[40m");
                    nextLine();
                    print("\e[");
                    print(m_line_buffer_fill);
                    print("C");
                }
                m_serial.print(read);
            }
            now = millis();
        }
        return false;
    }

    ///
    /// This method will not exit for delayTimeMillis but while entered
    /// it will process input.
    ///
    /// @return true if command delimeter was read and the delay exited early. If outTimeRemainingMillis
    ///         was provided this will be the time left in the delay when the command was received.
    ///
    bool delayWithInput(unsigned long delayTimeMillis, unsigned long* outTimeRemainingMillis = nullptr)
    {
        unsigned long       now         = millis();
        const unsigned long delay_until = now + delayTimeMillis;
        while (delay_until > now)
        {
            if (service())
            {
                if (outTimeRemainingMillis != nullptr)
                {
                    *outTimeRemainingMillis = (delay_until - now);
                }
                return true;
            }
            delay(1);
            now = millis();
        }
        if (outTimeRemainingMillis != nullptr)
        {
            *outTimeRemainingMillis = 0;
        }
        return false;
    }

    // +----------------------------------------------------------------------+
    // | TERMINAL COMMANDS
    // +----------------------------------------------------------------------+

    ///
    /// Returns if there is a pending command in the last command buffer.
    ///
    bool hasCommand() const
    {
        return (m_line_buffer && m_last_command[0] != 0);
    }

    ///
    /// Returns a pointer to the internal command buffer.
    /// This memory will become invalid if end() is called.
    ///
    const char* getCommand() const
    {
        return m_last_command;
    }

    ///
    /// Clear the last command buffer.
    ///
    void clearCommand()
    {
        if (m_line_buffer)
        {
            m_last_command[0] = 0;
        }
    }

    ///
    /// Process the command in the last command buffer.
    ///
    bool handleCommand(bool clear = true)
    {
        if (!hasCommand())
        {
            return false;
        }

        const char* name = getCommand();

        for(const CommandRecord& record: m_commands)
        {
            if (strcmp(name, record.name) == 0)
            {
                doCommand(record);
                break;
            }
        }

        if (clear)
        {
            clearCommand();
        }

        return true;
    }

    // +----------------------------------------------------------------------+
    // | ANSI ESCAPE CODES
    // |        https://en.wikipedia.org/wiki/ANSI_escape_code
    // +----------------------------------------------------------------------+
    void clearLine()
    {
        print("\e[2K");
    }

    void clearToEndOfLine()
    {
        print("\e[1K");
    }

    void home()
    {
        print("\e[0G");
    }

    void previousLine()
    {
        print("\e[1F");
    }

    void nextLine()
    {
        print("\e[1E");
    }

    void backspace()
    {
        print("\e[1D\e[0K");
    }

    void prompt()
    {
        print("Botnet: ");
    }
    // +----------------------------------------------------------------------+
    // | arduino::Print
    // +----------------------------------------------------------------------+
    virtual size_t write(uint8_t c) override
    {
        return m_serial.write(c);
    }

    // +----------------------------------------------------------------------+
    // | arduino::Stream
    // +----------------------------------------------------------------------+
    virtual int available() override
    {
        return m_serial.available();
    }

    virtual int read() override
    {
        return m_serial.read();
    }

    virtual int peek() override
    {
        return m_serial.peek();
    }

    // +----------------------------------------------------------------------+
    // | SINGLETON
    // +----------------------------------------------------------------------+
    /**
     * You must declare the storage space for this in your sketch. For example:
     * ```
     *     // Terminal specialization.
     *     using BotnetTerminal = BotanyNet::Terminal<typeof(Serial)>;
     *
     *     // Terminal static storage.
     *     template<> BotnetTerminal BotnetTerminal::terminal(Serial);
     *
     * ```
     * Why?
     * Because you don't want your firmware to be executing constructors in include statements.
     * This is particularly bad because subsequent includes can alias things after the class
     * is constructed and the order that the singleton is constructed can change based
     * on include guards and which other header is first to include this one. Yes Arduino
     * does this a lot but it's just a bad plan.
     */
    static Terminal terminal;

private:
    void initInternalBuffers()
    {
        constexpr size_t TotalBuffersLength = LineBufferLength + MaxCommandLength + 1;

        // Allocate raw storage
        m_line_buffer  = new char[TotalBuffersLength];
        m_last_command = m_line_buffer + LineBufferLength;
        clearCommand();
    }

    int doCommand(const CommandRecord& record)
    {
        const char* argv[] = {"soil_proto"};
        return record.command(*this, 1, argv);
    }

    bool handleControlChar(const char c)
    {
        if (c < 0x20 || c > 0x7E)
        {
            if (c == 0x8 && m_line_buffer_fill > 0)
            {
                m_line_buffer_fill -= 1;
                backspace();
            }
            return true;
        }
        else
        {
            return false;
        }
    }

    bool appendToLineBuffer(const char c)
    {
        if (m_line_buffer_fill < LineBufferLength - 1)
        {
            m_line_buffer[m_line_buffer_fill] = c;
            m_line_buffer_fill += 1;
            return true;
        }
        else
        {
            return false;
        }
    }

    void clearLineBuffer()
    {
        m_line_buffer_fill = 0;
    }

    size_t readCommand(char* out) const
    {
        size_t bytes_read = 0;
        while (bytes_read < m_line_buffer_fill && bytes_read < MaxCommandLength)
        {
            out[bytes_read] = m_line_buffer[bytes_read];
            if (out[bytes_read] == 0)
            {
                break;
            }
            bytes_read += 1;
        }
        out[bytes_read] = 0;
        return bytes_read;
    }

    bool delayForOneSecond()
    {
        if (m_alert_led > 0)
        {
            digitalWrite(LED_BUILTIN, 1);
            if (delayWithInput(500))
            {
                return true;
            }
            digitalWrite(LED_BUILTIN, 0);
            if (delayWithInput(500))
            {
                return true;
            }
        }
        else
        {
            if (delayWithInput(1000))
            {
                return true;
            }
        }
        return false;
    }

    // LED pin used by the terminal as a sort of visual bell.
    // Set to 0 for none.
    std::array<CommandRecord, CommandCount> m_commands;
    SerialType&                             m_serial;
    const int                               m_alert_led;
    char*                                   m_line_buffer;
    char*                                   m_last_command;
    size_t                                  m_line_buffer_fill;
};

}  // namespace BotanyNet

#endif  // BOTANYNET_TERMINAL_INCLUDED_H
