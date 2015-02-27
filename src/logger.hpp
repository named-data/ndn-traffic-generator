/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
/**
 * Copyright (C) 2014-2015  University of Arizona.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Jerald Paul Abraham <jeraldabraham@email.arizona.edu>
 */

#ifndef NTG_LOGGER_HPP
#define NTG_LOGGER_HPP

#include <cstdlib>
#include <fstream>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace ndn {

class Logger
{
public:
  Logger(const std::string& module)
    : m_module(module)
  {
  }

  void
  shutdownLogger()
  {
    if (m_logFile.is_open())
      {
        log("Terminating Logging Operations" , true, true);
        m_logFile.close();
      }
  }

  static std::string
  getTimestamp()
  {
    boost::posix_time::ptime now;
    now = boost::posix_time::second_clock::local_time();
    return to_simple_string(now);
  }

  void
  log(const std::string& logLine, bool printTime, bool printToConsole)
  {
    if (m_logLocation.length() > 0)
      {
        if (printTime)
          m_logFile << getTimestamp() << " - ";
        m_logFile << logLine << std::endl;
        m_logFile.flush();
        if (printToConsole)
          {
            if (printTime)
              std::cout << getTimestamp() << " - ";
            std::cout << logLine << std::endl;
          }
      }
    else
      {
        if (printTime)
          std::cout << getTimestamp() << " - ";
        std::cout << logLine << std::endl;
      }
  }

  void
  initializeLog(const std::string& instanceId)
  {
    const char* envVar = std::getenv("NDN_TRAFFIC_LOGFOLDER");
    m_logLocation = "";
    if (envVar != nullptr)
      m_logLocation = envVar;

    std::string logFilename;
    if (boost::filesystem::exists(boost::filesystem::path(m_logLocation)))
      {
        if (boost::filesystem::is_directory(boost::filesystem::path(m_logLocation)))
          {
            logFilename = m_logLocation + "/" + m_module + "_" + instanceId + ".log";
            m_logFile.open(logFilename.c_str(), std::ofstream::out | std::ofstream::trunc);
            if (m_logFile.is_open())
              std::cout << "Log File Initialized: " << logFilename << std::endl;
            else
              {
                std::cout << "ERROR - Unable To Initialize A Log File At: "
                          << m_logLocation << std::endl
                          << "Using Default Output For Logging." << std::endl;
                m_logLocation = "";
              }
          }
        else
          {
            std::cout << "Environment Variable NDN_TRAFFIC_LOGFOLDER Should Be A Folder."
                      << std::endl
                      << "Using Default Output For Logging." << std::endl;
            m_logLocation = "";
          }
      }
    else
      {
        std::cout << "Environment Variable NDN_TRAFFIC_LOGFOLDER Not Set." << std::endl
                  << "Using Default Output For Logging." << std::endl;
        m_logLocation = "";
      }
  }

private:
  std::string m_module;
  std::string m_logLocation;
  std::ofstream m_logFile;
};

} // namespace ndn

#endif // NTG_LOGGER_HPP
