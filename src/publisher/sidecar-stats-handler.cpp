/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2025,  The University of Memphis,
 *                           Regents of the University of California,
 *                           Arizona Board of Regents.
 *
 * This file is part of NLSR (Named-data Link State Routing).
 * See AUTHORS.md for complete list of NLSR authors and contributors.
 *
 * NLSR is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NLSR is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NLSR, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "sidecar-stats-handler.hpp"
#include <ndn-cxx/encoding/block.hpp>
#include <ndn-cxx/encoding/block-helpers.hpp>
#include <ndn-cxx/encoding/encoding-buffer.hpp>
#include <ndn-cxx/util/string-helper.hpp>
#include <fstream>
#include <sstream>
#include <iostream>

namespace nlsr {

SidecarStatsHandler::SidecarStatsHandler(ndn::mgmt::Dispatcher& dispatcher,
                                         const std::string& logPath)
  : m_logPath(logPath)
{
  // Register dataset handlers
  dispatcher.addStatusDataset(dataset::SIDECAR_STATS_DATASET,
                             ndn::mgmt::makeAcceptAllAuthorization(),
                             std::bind(&SidecarStatsHandler::publishSidecarStats, this, _1, _2, _3));

  dispatcher.addStatusDataset(dataset::SIDECAR_SERVICE_STATS_DATASET,
                             ndn::mgmt::makeAcceptAllAuthorization(),
                             std::bind(&SidecarStatsHandler::publishServiceStats, this, _1, _2, _3));

  dispatcher.addStatusDataset(dataset::SIDECAR_SFC_STATS_DATASET,
                             ndn::mgmt::makeAcceptAllAuthorization(),
                             std::bind(&SidecarStatsHandler::publishSfcStats, this, _1, _2, _3));

  dispatcher.addStatusDataset(dataset::FUNCTION_INFO_DATASET,
                             ndn::mgmt::makeAcceptAllAuthorization(),
                             std::bind(&SidecarStatsHandler::publishFunctionInfo, this, _1, _2, _3));
}

std::map<std::string, std::string>
SidecarStatsHandler::getCurrentStats() const
{
  return getLatestStats();
}

void
SidecarStatsHandler::publishSidecarStats(const ndn::Name& topPrefix,
                                         const ndn::Interest& interest,
                                         ndn::mgmt::StatusDatasetContext& context)
{
  auto stats = getLatestStats();
  
  std::string response = "Sidecar Statistics Dataset\n";
  response += "================================\n";
  
  for (const auto& [key, value] : stats) {
    response += key + ": " + value + "\n";
  }

  context.append(ndn::encoding::makeStringBlock(ndn::tlv::Content, response));
  context.end();
}

void
SidecarStatsHandler::publishServiceStats(const ndn::Name& topPrefix,
                                         const ndn::Interest& interest,
                                         ndn::mgmt::StatusDatasetContext& context)
{
  auto stats = getLatestStats();
  
  std::string serviceStats = "Service Call Statistics\n";
  serviceStats += "=======================\n";
  
  if (stats.find("service_call") != stats.end()) {
    serviceStats += "Call Name: " + stats["call_name"] + "\n";
    serviceStats += "In Time: " + stats["in_time"] + "\n";
    serviceStats += "Out Time: " + stats["out_time"] + "\n";
    serviceStats += "Port: " + stats["port_num"] + "\n";
    serviceStats += "Input Data Size: " + stats["in_datasize"] + "\n";
    serviceStats += "Output Data Size: " + stats["out_datasize"] + "\n";
  } else {
    serviceStats += "No service call data available\n";
  }

  context.append(ndn::encoding::makeStringBlock(ndn::tlv::Content, serviceStats));
  context.end();
}

void
SidecarStatsHandler::publishSfcStats(const ndn::Name& topPrefix,
                                     const ndn::Interest& interest,
                                     ndn::mgmt::StatusDatasetContext& context)
{
  auto stats = getLatestStats();
  
  std::string sfcStats = "SFC Execution Statistics\n";
  sfcStats += "=========================\n";
  
  if (stats.find("sfc_time") != stats.end()) {
    sfcStats += "SFC Start Time: " + stats["sfc_time"] + "\n";
    sfcStats += "Sidecar In Time: " + stats["sidecar_in_time"] + "\n";
    sfcStats += "Sidecar Out Time: " + stats["sidecar_out_time"] + "\n";
    sfcStats += "Host Name: " + stats["host_name"] + "\n";
  } else {
    sfcStats += "No SFC data available\n";
  }

  context.append(ndn::encoding::makeStringBlock(ndn::tlv::Content, sfcStats));
  context.end();
}

void
SidecarStatsHandler::publishFunctionInfo(const ndn::Name& topPrefix,
                                         const ndn::Interest& interest,
                                         ndn::mgmt::StatusDatasetContext& context)
{
  std::string functionInfo = "Function Information Dataset\n";
  functionInfo += "===============================\n";
  functionInfo += "This dataset provides Service Function information\n";
  functionInfo += "including processing time, load, and usage count.\n";
  functionInfo += "\n";
  functionInfo += "Note: Function information is stored in NameLsa\n";
  functionInfo += "and can be accessed via 'nlsrc status lsdb/names'\n";
  functionInfo += "\n";
  functionInfo += "For detailed statistics, use:\n";
  functionInfo += "- nlsrc status sidecar-stats\n";
  functionInfo += "- nlsrc status service-stats\n";
  functionInfo += "- nlsrc status sfc-stats\n";

  context.append(ndn::encoding::makeStringBlock(ndn::tlv::Content, functionInfo));
  context.end();
}

std::vector<std::map<std::string, std::string>>
SidecarStatsHandler::parseSidecarLog() const
{
  std::vector<std::map<std::string, std::string>> logEntries;
  std::ifstream logFile(m_logPath);
  
  if (!logFile.is_open()) {
    return logEntries;
  }

  std::string line;
  while (std::getline(logFile, line)) {
    try {
      // Simple JSON parsing for sidecar log format
      std::map<std::string, std::string> entry;
      
      // Remove leading/trailing whitespace and braces
      line.erase(0, line.find_first_not_of(" \t"));
      line.erase(line.find_last_not_of(" \t") + 1);
      if (line.front() == '{' && line.back() == '}') {
        line = line.substr(1, line.length() - 2);
      }
      
      // Parse key-value pairs
      std::istringstream iss(line);
      std::string pair;
      while (std::getline(iss, pair, ',')) {
        size_t colonPos = pair.find(':');
        if (colonPos != std::string::npos) {
          std::string key = pair.substr(0, colonPos);
          std::string value = pair.substr(colonPos + 1);
          
          // Remove quotes
          if (key.front() == '"' && key.back() == '"') {
            key = key.substr(1, key.length() - 2);
          }
          if (value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.length() - 2);
          }
          
          entry[key] = value;
        }
      }
      
      if (!entry.empty()) {
        logEntries.push_back(entry);
      }
    } catch (const std::exception& e) {
      // Skip malformed lines
      continue;
    }
  }
  
  return logEntries;
}

std::map<std::string, std::string>
SidecarStatsHandler::getLatestStats() const
{
  auto logEntries = parseSidecarLog();
  
  if (logEntries.empty()) {
    return {{"error", "No log entries found"}};
  }
  
  // Return the latest entry
  return logEntries.back();
}

} // namespace nlsr 