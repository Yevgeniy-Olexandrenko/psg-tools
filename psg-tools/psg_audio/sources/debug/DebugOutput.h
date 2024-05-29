#pragma once

#if defined(_DEBUG)
#include <cassert>
#include <fstream>
#include <cstdarg>
#endif

#include "DebugConfig.h"
#include "DebugPayload.h"

template<bool ENABLE>
class DebugOutput
{
public:
    void open(const std::string& tag, const DebugPayload* payload = nullptr)
    {
#if defined(_DEBUG)
        if (ENABLE)
        {
            close(payload);
            assert(!tag.empty());
            m_file.open("dbg_" + tag + ".txt");
            if (m_file && payload)
            {
                payload->print_header(m_file);
                print_endline();
            }
        }
#endif
    }

    void print_payload(const DebugPayload* payload)
    {
#if defined(_DEBUG)
        if (ENABLE)
        {
            print_payload("", payload);
        }
#endif
    }

    void print_payload(const char tag, const DebugPayload* payload)
    {
#if defined(_DEBUG)
        if (ENABLE)
        {
            print_payload(std::string(1, tag), payload);
        }
#endif
    }

    void print_payload(const std::string& tag, const DebugPayload* payload)
    {
#if defined(_DEBUG)
        if (ENABLE)
        {
            if (m_file)
            {
                assert(payload != nullptr);
                if (!tag.empty()) m_file << tag << " : ";
                payload->print_payload(m_file);
                print_endline();
            }
        }
#endif
    }

    void print_message(const char* format, ...)
    {
#if defined(_DEBUG)
        if (ENABLE)
        {
            if (m_file)
            {
                char buffer[256];
                va_list args;
                va_start(args, format);
                vsnprintf_s(buffer, sizeof(buffer), format, args);
                va_end(args);
                m_file << buffer;
            }
        }
#endif
    }

    void print_endline()
    {
#if defined(_DEBUG)
        if (ENABLE)
        {
            if (m_file) m_file << std::endl;
        }
#endif
    }

    void close(const DebugPayload* payload = nullptr)
    {
#if defined(_DEBUG)
        if (ENABLE)
        {
            if (m_file)
            {
                if (payload)
                {
                    payload->print_footer(m_file);
                    print_endline();
                }
                m_file.close();
            }
        }
#endif
    }

#if defined(_DEBUG)
private:
    std::ofstream m_file;
#endif
};
