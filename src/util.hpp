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

#include <cctype>
#include <string>

namespace ndn {

static inline bool
parseParameterAndValue(const std::string& input, std::string& parameter, std::string& value)
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

} // namespace ndn

#endif // NTG_UTIL_HPP
