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
    /// The terminal prompt for interactive sessions.
    ///
    static constexpr const char* const Prompt = "Botnet: ";

    ///
    /// A built-in command that prints help text for using all the user-provided
    /// commands.
    ///
    static constexpr const char* const BuiltinCommandHelp = "help";

    ///
    /// A built-in command that exits the interactive session.
    ///
    static constexpr const char* const BuiltinCommandExit = "quit";

    ///
    /// Message printed at the start of an interactive session.
    ///
    static constexpr const char* const ShellMessageStart = "Starting shell...";

    ///
    /// Message printed when an interactive session ends.
    ///
    static constexpr const char* const ShellMessageEnd = "Shell has exited.";
    
    ///
    /// The maximum time the service method will use...mostly.
    /// This class tries to keep this deadline but Arduino is not a realtime
    /// framework so calls into other objects may cause us to exceed our
    /// deadline.
    ///
    static constexpr unsigned long DefaultServiceTimeoutMillis = 800;

    ///
    /// The maximum number of characters for any one command. This limits the
    /// size of internal command buffers and the keys for command lookup tables.
    ///
    static constexpr size_t MaxCommandLength = 16;

    ///
    /// The maximum length of a single line of the terminal.
    ///
    static constexpr size_t LineBufferLength = 100;

    ///
    /// The maximum number of arguments any one command can receive.
    ///
    static constexpr size_t MaxArgumentCount = 4;

    ///
    /// Input character that will trigger the parsing of a complete line.
    ///
    static constexpr char LineDelimeter = '\n';

    ///
    /// Each registered command gets invoked like a mini program.
    ///
    using Command = int (*)(arduino::Stream& t, void* user, size_t argc, const char* const argv[]);

    ///
    /// The number of commands stored in this object.
    ///
    static constexpr size_t NumberOfCommands = CommandCount;

    ///
    /// The data structure used to store user-defined commands.
    ///
    struct CommandRecord
    {
        /// The command nameused to activate the command.
        const char        name[MaxCommandLength];

        /// Opaque data pass through to the command when invoked.
        void*             user;

        /// The command program to invoke.
        Command           command;

        /// A pointer to a string of text to emit as part of the help
        /// system. This pointer must remain valid while the terminal
        /// is valid.
        const char* const help;
    };

private:
    Terminal(SerialType& serial, std::array<CommandRecord, CommandCount> &&l)
        : m_commands(l)
        , m_serial(serial)
        , m_line_buffer(nullptr)
        , m_last_command(nullptr)
        , m_last_command_arguments{0}
        , m_last_command_arguments_length(1)
        , m_line_buffer_fill{0}
        , m_running_interactive_shell(false)
    {
        static_assert(MaxArgumentCount > 1, "MaxArgumentCount must allow at least one argument per command.");
    }

    virtual ~Terminal()
    {
        free(m_line_buffer);
    }

public:
    ///
    /// Extend the Arduino convention for Serial bool conversions to this class.
    ///
    operator bool()
    {
        return static_cast<bool>(m_serial);
    }

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
    }

    ///
    /// End the serial device and free internal buffers.
    ///
    void end()
    {
        m_serial.end();
        deinitInternalBuffers();
        m_running_interactive_shell = false;
    }

    ///
    /// Set the internal state of the terminal to run an interactive session
    /// rather then just being a passive output device.
    ///
    void startInteractiveSession()
    {
        clearScreen();
        home();
        println(ShellMessageStart);
        prompt();
        m_running_interactive_shell = true;
    }

    ///
    /// End the interactive session and revert to a passive output device.
    ///
    void endInteractiveSession()
    {
        if (m_running_interactive_shell)
        {
            clearLineBuffer();
            clearCommand();
            println();
            clearScreen();
            println(ShellMessageEnd);
            m_running_interactive_shell = false;
        }
    }

    ///
    /// Returns if this object is running an interactive session or not.
    ///
    bool isInteractive() const
    {
        return m_running_interactive_shell;
    }

    ///
    /// Give CPU time to the terminal.
    ///
    void service(const unsigned long serviceTimeoutMillis = DefaultServiceTimeoutMillis)
    {
        unsigned long       now      = millis();
        const unsigned long deadline = now + serviceTimeoutMillis;
        while (deadline > now)
        {
            if (read_for(deadline - now))
            {
                handleCommand(true);
            }
            now = millis();
        }
    }

    ///
    /// This method will not exit for delayTimeMillis or until the line delimeter
    /// is read. While entered it will process serial input.
    ///
    /// @return true if command delimeter was read and the delay exited early.
    ///
    bool delayWithInput(unsigned long delayTimeMillis)
    {
        unsigned long       now         = millis();
        const unsigned long delay_until = now + delayTimeMillis;
        while (delay_until > now)
        {
            if (read_for(delay_until - now))
            {
                return true;
            }
            delay(1);
            now = millis();
        }
        return false;
    }

    ///
    /// Print out interactive terminal help text.
    ///
    void printHelp()
    {
        println();
        println("    The Botnet terminal provides an extermely simple and memory");
        println("    efficient shell for running basic commands. All commands are");
        print(  "    case-sensitive and input is limited to ");
        print(LineBufferLength);
        println(" characters");
        println("    for a single command.");
        println();
        println("    +------------------------------------------------------------------+");
        println("    | Available Commands");
        println("    +------------------------------------------------------------------+");
        println();
        print(  "    ");
        println(BuiltinCommandHelp);
        println("        Print this message.");
        println();
        print(  "    ");
        println(BuiltinCommandExit);
        println("        Exit the interactive shell.");
        println();

        for(const CommandRecord& record: m_commands)
        {
            print("    ");
            println(record.name);
            print("        ");
            println(record.help);
            println();
        }
        println("    +------------------------------------------------------------------+");
        println();
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
    /// Clear the last command buffer.
    ///
    void clearCommand()
    {
        m_last_command = nullptr;
        m_last_command_arguments_length = 0;
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

        const char* const name = m_last_command;

        if (strcmp(name, BuiltinCommandExit) == 0)
        {
            endInteractiveSession();
        }
        else if (strcmp(name, BuiltinCommandHelp) == 0 || (strlen(name) == 1 && name[0] == '?'))
        {
            println();
            printHelp();
        }
        else
        {
            for(const CommandRecord& record: m_commands)
            {
                if (strcmp(name, record.name) == 0)
                {
                    println();
                    record.command(*this, record.user,  m_last_command_arguments_length, m_last_command_arguments);
                    break;
                }
            }
        }

        if (m_running_interactive_shell)
        {
            // we didn't exit so setup the prompt for the next command:
            clearLineBuffer();
            println();
            clearLine();
            prompt();
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
    #define CSI "\x1B\x5B"

    void clearScreen()
    {
        print(CSI "2J");
    }

    void clearLine()
    {
        print(CSI "2K");
    }

    void clearToEndOfLine()
    {
        print(CSI "1K");
    }

    void home()
    {
        print(CSI "0G");
    }

    void previousLine()
    {
        print(CSI "1F");
    }

    void nextLine()
    {
        print(CSI "1E");
    }

    void backspace()
    {
        print(CSI "1D" CSI "0K");
    }

    void prompt()
    {
        print(Prompt);
    }

    void moveCursorTo(unsigned one_based_row, unsigned one_based_column)
    {
        print(CSI);
        print(max(1,one_based_row));
        print(';');
        print(max(1,one_based_column));
        print('H');
    }

    void moveCursorToColumn(unsigned one_based_column)
    {
        print(CSI);
        print(max(1, one_based_column));
        print('G');
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
    // +----------------------------------------------------------------------+
    // | PRIVATE :: BUFFER INITIALIZATION
    // +----------------------------------------------------------------------+

    void initInternalBuffers()
    {
        static_assert(LineBufferLength >= MaxCommandLength, "Line buffer must be at least large enough to store one command.");
        // Allocate raw storage. +1 so we can add a null after the last string.
        m_line_buffer  = new char[LineBufferLength + 1];
        clearCommand();
    }

    void deinitInternalBuffers()
    {
        free(m_line_buffer);
        m_line_buffer = nullptr;
        clearCommand();
    }

    // +----------------------------------------------------------------------+
    // | PRIVATE :: I/O
    // +----------------------------------------------------------------------+
    ///
    /// Call to try to read input from the serial device. This method returns immediatly
    /// if no input is available or if the command delimeter was read. It will continue
    /// to read into the line buffer for readTimeMillis while input is available.
    ///
    /// This allows us to process chunks of input and handle commands outside of this
    /// frame.
    /// 
    /// @param  readTimeMillis  The number of milliseconds to read for.
    /// @return true if command delimeter was read else false.
    ///
    bool read_for(unsigned long readTimeMillis)
    {
        unsigned long       now      = millis();
        const unsigned long deadline = now + readTimeMillis;
        while (deadline > now && m_serial.available())
        {
            const char read = m_serial.read();
            if (read == LineDelimeter)
            {
                tokenize();
                return true;
            }
            else if (!handleControlChar(read))
            {
                // This was not a control character. Treat as input.
                if (!appendToLineBuffer(read))
                {
                    println();
                    print(CSI "41mBUFFER LIMIT" CSI "0m");
                    previousLine();
                    moveCursorToColumn(strlen(Prompt) + m_line_buffer_fill);
                }
                m_serial.print(read);
            }
            now = millis();
        }
        return false;
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
        if (m_line_buffer && m_line_buffer_fill < LineBufferLength - 1)
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

    // +----------------------------------------------------------------------+
    // | PRIVATE :: TOKENIZATION
    // +----------------------------------------------------------------------+

    ///
    /// Parse the line buffer and store the results in our "last command"
    /// data members.
    ///
    void tokenize()
    {
        size_t lineBufferOffset = 0;
        if (makeNextToken(lineBufferOffset, m_last_command) > 0)
        {
            // we have a command. Let's find us some arguments!
            m_last_command_arguments[0] = m_last_command;
            m_last_command_arguments_length = 1;
            while (m_last_command_arguments_length < MaxArgumentCount + 1 &&
                   makeNextToken(lineBufferOffset, m_last_command_arguments[m_last_command_arguments_length]) > 0)
            {
                m_last_command_arguments_length += 1;
            }
        }
    }

    size_t makeNextToken(size_t& inoutLineBufferOffset, const char*& outToken)
    {
        size_t i = inoutLineBufferOffset;
        size_t token_len = 0;
        const char* start_of_token = nullptr;
        bool reading_token = false;
        while (i < m_line_buffer_fill)
        {
            const char next_char = m_line_buffer[i];
            const bool is_word_deliniator = (arduino::isWhitespace(next_char) || 0 == next_char);
            if (!is_word_deliniator && !reading_token)
            {
                // Found start of the next token.
                reading_token = true;
                start_of_token = &m_line_buffer[i];
            }
            else if (is_word_deliniator && reading_token)
            {
                break;
            }
            else if (reading_token)
            {
                token_len += 1;
            }
            i += 1;
        }
        if (reading_token)
        {
            // We've sized the line buffer so that its max fill is always 1 less then its capacity.
            m_line_buffer[i] = 0;
            outToken = start_of_token;
            i += 1;
        }
        else
        {
            outToken = nullptr;
        }
        inoutLineBufferOffset = i;
        return token_len;
    }

    // +----------------------------------------------------------------------+
    // | DATA MEMBERS
    // +----------------------------------------------------------------------+
    const std::array<CommandRecord, CommandCount>   m_commands;
    SerialType&                                     m_serial;
    char*                                           m_line_buffer;
    const char*                                     m_last_command;
    const char*                                     m_last_command_arguments[MaxArgumentCount + 1];
    size_t                                          m_last_command_arguments_length;
    size_t                                          m_line_buffer_fill;
    bool                                            m_running_interactive_shell;
};

}  // namespace BotanyNet

#endif  // BOTANYNET_TERMINAL_INCLUDED_H
