//--------------------------------------------------------------------------------------------------
/**
 * Definition of the Process class, which is used to hold the details of a single process
 * defined in a "run:" subsection of a "processes:" section in a .adef file.
 *
 * Copyright (C) Sierra Wireless, Inc. 2013. All rights reserved. Use of this work is subject to license.
 **/
//--------------------------------------------------------------------------------------------------

#ifndef PROCESS_H_INCLUDE_GUARD
#define PROCESS_H_INCLUDE_GUARD

namespace legato
{


class Process
{
    public:
        Process() {};
        Process(const Process&) = delete;
        Process(Process&&);
        virtual ~Process() {}

    public:
        Process& operator =(const Process&) = delete;

    private:
        std::string m_Name;
        std::string m_ExePath;
        std::list<std::string> m_CommandLineArgs;
        uint16_t m_DebugPort;   ///< gdb port number to use for debugging.  0 = no debugging.

    public:
        void Name(const std::string& name) { m_Name = name; }
        void Name(std::string&& name) { m_Name = std::move(name); }
        const std::string& Name() const { return m_Name; }

        void ExePath(const std::string& path) { m_ExePath = path; }
        void ExePath(std::string&& path) { m_ExePath = std::move(path); }
        const std::string& ExePath() const { return m_ExePath; }

        void AddCommandLineArg(const std::string& arg) { m_CommandLineArgs.push_back(arg); }
        void AddCommandLineArg(std::string&& arg) { m_CommandLineArgs.push_back(arg); }
        const std::list<std::string>& CommandLineArgs() const { return m_CommandLineArgs; }

        void EnableDebugging(uint16_t debugPortNumber) { m_DebugPort = debugPortNumber; }
        bool IsDebuggingEnabled() const { return (m_DebugPort != 0); }
        uint16_t DebugPort() const { return m_DebugPort; }
};


}

#endif // PROCESS_H_INCLUDE_GUARD
