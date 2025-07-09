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
#include <algorithm>
#include <cmath>
#include <optional>
#include <tuple>
#include <vector>
#include <map>
#include <string>
#include <cstdint> // Required for int types
#include <numeric> // Required for std::accumulate

namespace nlsr {

SidecarStatsHandler::SidecarStatsHandler(ndn::mgmt::Dispatcher& dispatcher,
                                         const std::string& logPath)
  : m_logPath(logPath)
{
  // Register dataset handlers
  dispatcher.addStatusDataset(dataset::SIDECAR_STATS_COMPONENT,
                             ndn::mgmt::makeAuthorization(),
                             std::bind(&SidecarStatsHandler::publishSidecarStats, this, _1, _2, _3));

  dispatcher.addStatusDataset(dataset::SIDECAR_SERVICE_STATS_COMPONENT,
                             ndn::mgmt::makeAuthorization(),
                             std::bind(&SidecarStatsHandler::publishServiceStats, this, _1, _2, _3));

  dispatcher.addStatusDataset(dataset::SIDECAR_SFC_STATS_COMPONENT,
                             ndn::mgmt::makeAuthorization(),
                             std::bind(&SidecarStatsHandler::publishSfcStats, this, _1, _2, _3));
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

std::vector<std::map<std::string, std::string>>
SidecarStatsHandler::parseSidecarLog()
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
SidecarStatsHandler::getLatestStats()
{
  auto logEntries = parseSidecarLog();
  
  if (logEntries.empty()) {
    return {{"error", "No log entries found"}};
  }
  
  // Return the latest entry
  return logEntries.back();
}

std::optional<std::tuple<double, double, double>>
SidecarStatsHandler::getDynamicWeights()
{
  auto logEntries = parseSidecarLog();
  
  if (logEntries.size() < 2) {
    return std::nullopt; // Need at least 2 entries for trend analysis
  }
  
  // Calculate trends from recent entries
  std::vector<double> processingTimes;
  std::vector<double> loads;
  std::vector<int> usageCounts;
  
  // Get last 10 entries for trend analysis
  size_t startIdx = (logEntries.size() > 10) ? logEntries.size() - 10 : 0;
  
  for (size_t i = startIdx; i < logEntries.size(); ++i) {
    const auto& entry = logEntries[i];
    
    if (entry.find("in_datasize") != entry.end() && entry.find("out_datasize") != entry.end()) {
      try {
        int inSize = std::stoi(entry.at("in_datasize"));
        int outSize = std::stoi(entry.at("out_datasize"));
        
        // Calculate processing time (simplified)
        double processingTime = static_cast<double>(outSize - inSize) / 1000.0;
        processingTimes.push_back(std::max(0.0, processingTime));
        
        // Calculate load (based on data size ratio)
        double load = static_cast<double>(inSize) / std::max(1, outSize);
        loads.push_back(std::min(1.0, load));
        
        // Usage count (increment for each entry)
        usageCounts.push_back(static_cast<int>(i - startIdx + 1));
      } catch (const std::exception& e) {
        continue;
      }
    }
  }
  
  if (processingTimes.empty() || loads.empty() || usageCounts.empty()) {
    return std::nullopt;
  }
  
  // Calculate dynamic weights based on trends
  double avgProcessingTime = std::accumulate(processingTimes.begin(), processingTimes.end(), 0.0) / processingTimes.size();
  double avgLoad = std::accumulate(loads.begin(), loads.end(), 0.0) / loads.size();
  double avgUsage = static_cast<double>(std::accumulate(usageCounts.begin(), usageCounts.end(), 0)) / usageCounts.size();
  
  // Normalize and calculate weights
  double total = avgProcessingTime + avgLoad + avgUsage;
  if (total == 0.0) {
    return std::nullopt;
  }
  
  double processingWeight = avgProcessingTime / total;
  double loadWeight = avgLoad / total;
  double usageWeight = avgUsage / total;
  
  return std::make_tuple(processingWeight, loadWeight, usageWeight);
}

} // namespace nlsr 