/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2019, Arizona Board of Regents.
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

#ifndef NTG_UTIL_HPP
#define NTG_UTIL_HPP

#include "logger.hpp"

#include <cctype>
#include <string>

#include <boost/algorithm/string/predicate.hpp>

namespace ndn {

static inline bool
extractParameterAndValue(const std::string& input, std::string& parameter, std::string& value)
{
  static const std::string allowedCharacters = ":/+._-%";
  parameter = "";
  value = "";

  std::size_t i = 0;
  // parse the parameter name
  while (i < input.length() && input[i] != '=') {
    parameter += input[i];
    i++;
  }
  if (i == input.length()) {
    return false;
  }

  // skip the equal sign
  i++;

  // parse the value
  while (i < input.length() &&
         (std::isalnum(input[i]) || allowedCharacters.find(input[i]) != std::string::npos)) {
    value += input[i];
    i++;
  }

  return !parameter.empty() && !value.empty() && i == input.length();
}

static inline bool
parseBoolean(const std::string& input)
{
  if (boost::iequals(input, "no") || boost::iequals(input, "off") ||
      boost::iequals(input, "false") || input == "0")
    return false;

  if (boost::iequals(input, "yes") || boost::iequals(input, "on") ||
      boost::iequals(input, "true") || input == "1")
    return true;

  throw std::invalid_argument("'" + input + "' is not a valid boolean value");
}

template<typename TrafficConfigurationType>
static inline bool
readConfigurationFile(const std::string& filename,
                      std::vector<TrafficConfigurationType>& patterns,
                      Logger& logger)
{
  std::ifstream patternFile(filename);
  if (!patternFile) {
    logger.log("ERROR: Unable to open traffic configuration file: " + filename, false, true);
    return false;
  }

  logger.log("Reading traffic configuration file: " + filename, true, true);

  int lineNumber = 0;
  std::string patternLine;
  while (getline(patternFile, patternLine)) {
    lineNumber++;
    if (std::isalpha(patternLine[0])) {
      TrafficConfigurationType trafficConf;
      bool shouldSkipLine = false;
      if (trafficConf.parseConfigurationLine(patternLine, logger, lineNumber)) {
        while (getline(patternFile, patternLine) && std::isalpha(patternLine[0])) {
          lineNumber++;
          if (!trafficConf.parseConfigurationLine(patternLine, logger, lineNumber)) {
            shouldSkipLine = true;
            break;
          }
        }
        lineNumber++;
      }
      else {
        shouldSkipLine = true;
      }
      if (!shouldSkipLine) {
        if (trafficConf.checkTrafficDetailCorrectness()) {
          patterns.push_back(std::move(trafficConf));
        }
      }
    }
  }

  return true;
}

} // namespace ndn

#endif // NTG_UTIL_HPP
