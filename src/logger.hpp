/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2023, Arizona Board of Regents.
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

#ifndef NDNTG_LOGGER_HPP
#define NDNTG_LOGGER_HPP

#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>

#include <boost/container/static_vector.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <ndn-cxx/util/time.hpp>

namespace ndntg {

class Logger
{
public:
  explicit
  Logger(std::string_view module)
    : m_module(module)
  {
  }

  void
  log(std::string_view logLine, bool printTimestamp, bool printToConsole)
  {
    boost::container::static_vector<std::reference_wrapper<std::ostream>, 2> destinations;
    if (!m_logLocation.empty()) {
      destinations.emplace_back(m_logFile);
    }
    if (m_logLocation.empty() || printToConsole) {
      destinations.emplace_back(std::cout);
    }

    if (printTimestamp) {
      if (m_wantUnixTime) {
        using namespace ndn::time;
        auto now = std::to_string(toUnixTimestamp<microseconds>(system_clock::now()).count() / 1e6);
        for (auto dest : destinations) {
          dest.get() << '[' << now << "] ";
        }
      }
      else {
        auto now = boost::posix_time::microsec_clock::local_time();
        for (auto dest : destinations) {
          dest.get() << '[' << now << "] ";
        }
      }
    }

    for (auto dest : destinations) {
      dest.get() << logLine << std::endl;
    }
  }

  void
  initialize(const std::string& instanceId, const std::string& timestampFormat)
  {
    m_wantUnixTime = timestampFormat.empty();
    if (!timestampFormat.empty()) {
      std::cout.imbue(std::locale(std::cout.getloc(),
                                  new boost::posix_time::time_facet(timestampFormat.data())));
    }

    m_logLocation = "";
    if (const char* envVar = std::getenv("NDN_TRAFFIC_LOGFOLDER"); envVar != nullptr) {
      m_logLocation = envVar;
    }
    if (m_logLocation.empty()) {
      std::cout << "Environment variable NDN_TRAFFIC_LOGFOLDER not set.\n"
                << "Using default output for logging." << std::endl;
      return;
    }

    boost::filesystem::path logdir(m_logLocation);
    if (boost::filesystem::exists(logdir)) {
      if (boost::filesystem::is_directory(logdir)) {
        auto logfile = logdir / (m_module + '_' + instanceId + ".log");
        m_logFile.open(logfile.string(), std::ofstream::out | std::ofstream::trunc);
        if (m_logFile.is_open()) {
          if (!timestampFormat.empty()) {
            m_logFile.imbue(std::locale(m_logFile.getloc(),
                                        new boost::posix_time::time_facet(timestampFormat.data())));
          }
          std::cout << "Log file initialized: " << logfile << std::endl;
        }
        else {
          std::cout << "ERROR: Unable to initialize a log file at: " << m_logLocation << "\n"
                    << "Using default output for logging." << std::endl;
          m_logLocation = "";
        }
      }
      else {
        std::cout << "NDN_TRAFFIC_LOGFOLDER is not a directory.\n"
                  << "Using default output for logging." << std::endl;
        m_logLocation = "";
      }
    }
    else {
      std::cout << "NDN_TRAFFIC_LOGFOLDER does not exist.\n"
                << "Using default output for logging." << std::endl;
      m_logLocation = "";
    }
  }

private:
  const std::string m_module;
  std::string m_logLocation;
  std::ofstream m_logFile;
  bool m_wantUnixTime = true;
};

} // namespace ndntg

#endif // NDNTG_LOGGER_HPP
