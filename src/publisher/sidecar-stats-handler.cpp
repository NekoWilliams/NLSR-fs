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
#include "logger.hpp"
#include "lsdb.hpp"
#include "conf-parameter.hpp"
#include "lsa/name-lsa.hpp"  // For ServiceFunctionInfo definition
#include <ndn-cxx/encoding/block.hpp>
#include <ndn-cxx/encoding/block-helpers.hpp>
#include <ndn-cxx/encoding/encoding-buffer.hpp>
#include <ndn-cxx/util/string-helper.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <functional>
#include <cstring>
#include <ctime>
#include <chrono>

namespace nlsr {

INIT_LOGGER(SidecarStatsHandler);

SidecarStatsHandler::SidecarStatsHandler(ndn::mgmt::Dispatcher& dispatcher,
                                         const std::string& logPath)
  : m_logPath(logPath)
  , m_isRegistered(false)
  , m_lsdb(nullptr)
  , m_confParam(nullptr)
{
  try {
    // Register dataset handlers with explicit logging
    NLSR_LOG_INFO("Registering sidecar-stats dataset handler");
    dispatcher.addStatusDataset(dataset::SIDECAR_STATS_DATASET,
                               ndn::mgmt::makeAcceptAllAuthorization(),
                               std::bind(&SidecarStatsHandler::publishSidecarStats, this, _1, _2, _3));

    NLSR_LOG_INFO("Registering service-stats dataset handler");
    dispatcher.addStatusDataset(dataset::SIDECAR_SERVICE_STATS_DATASET,
                               ndn::mgmt::makeAcceptAllAuthorization(),
                               std::bind(&SidecarStatsHandler::publishServiceStats, this, _1, _2, _3));

    NLSR_LOG_INFO("Registering sfc-stats dataset handler");
    dispatcher.addStatusDataset(dataset::SIDECAR_SFC_STATS_DATASET,
                               ndn::mgmt::makeAcceptAllAuthorization(),
                               std::bind(&SidecarStatsHandler::publishSfcStats, this, _1, _2, _3));

    NLSR_LOG_INFO("Registering function-info dataset handler");
    dispatcher.addStatusDataset(dataset::FUNCTION_INFO_DATASET,
                               ndn::mgmt::makeAcceptAllAuthorization(),
                               std::bind(&SidecarStatsHandler::publishFunctionInfo, this, _1, _2, _3));

    m_isRegistered = true;
    NLSR_LOG_INFO("All sidecar dataset handlers registered successfully");
  }
  catch (const std::exception& e) {
    NLSR_LOG_ERROR("Failed to register sidecar dataset handlers: " + std::string(e.what()));
    m_isRegistered = false;
    throw;
  }
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
  try {
    NLSR_LOG_DEBUG("Received sidecar-stats request from: " + interest.getName().toUri());
    
    auto stats = getLatestStats();
    
    std::string response = "Sidecar Statistics Dataset\n";
    response += "================================\n";
    
    if (stats.find("error") != stats.end()) {
      response += "Error: " + stats["error"] + "\n";
      response += "Log file path: " + m_logPath + "\n";
      response += "Registration status: " + std::string(m_isRegistered ? "Registered" : "Not registered") + "\n";
    } else {
      for (const auto& [key, value] : stats) {
        response += key + ": " + value + "\n";
      }
    }

    context.append(ndn::encoding::makeStringBlock(ndn::tlv::Content, response));
    context.end();
    
    NLSR_LOG_DEBUG("Sidecar-stats response sent successfully");
  }
  catch (const std::exception& e) {
    NLSR_LOG_ERROR("Error in publishSidecarStats: " + std::string(e.what()));
    std::string errorResponse = "Error: " + std::string(e.what()) + "\n";
    context.append(ndn::encoding::makeStringBlock(ndn::tlv::Content, errorResponse));
    context.end();
  }
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
  NLSR_LOG_DEBUG("parseSidecarLog called for: " << m_logPath);
  std::vector<std::map<std::string, std::string>> logEntries;
  
  try {
    std::ifstream logFile(m_logPath);
    
    if (!logFile.is_open()) {
      NLSR_LOG_WARN("Cannot open log file: " + m_logPath);
      return logEntries;
    }
    
    NLSR_LOG_DEBUG("Log file opened successfully");

    std::string line;
    int lineCount = 0;
    while (std::getline(logFile, line)) {
      lineCount++;
      try {
        // Parse nested JSON structure
        // Expected format: {"service_call": {"in_time": "...", "out_time": "..."}, "sidecar": {...}}
        std::map<std::string, std::string> entry;
        
        // Remove leading/trailing whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (line.empty() || line.front() != '{' || line.back() != '}') {
          continue;
        }
        
        // Extract service_call information
        size_t scStart = line.find("\"service_call\"");
        if (scStart != std::string::npos) {
          size_t scBegin = line.find('{', scStart);
          size_t scEnd = line.find('}', scBegin);
          if (scBegin != std::string::npos && scEnd != std::string::npos) {
            std::string serviceCall = line.substr(scBegin, scEnd - scBegin + 1);
            
            // Extract in_time and out_time
            size_t inTimePos = serviceCall.find("\"in_time\"");
            size_t outTimePos = serviceCall.find("\"out_time\"");
            
            if (inTimePos != std::string::npos) {
              size_t colonPos = serviceCall.find(':', inTimePos);
              size_t quoteStart = serviceCall.find('"', colonPos);
              size_t quoteEnd = serviceCall.find('"', quoteStart + 1);
              if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                entry["service_call_in_time"] = serviceCall.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
              }
            }
            
            if (outTimePos != std::string::npos) {
              size_t colonPos = serviceCall.find(':', outTimePos);
              size_t quoteStart = serviceCall.find('"', colonPos);
              size_t quoteEnd = serviceCall.find('"', quoteStart + 1);
              if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                entry["service_call_out_time"] = serviceCall.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
              }
            }
          }
        }
        
        // Extract sidecar information
        size_t sidecarStart = line.find("\"sidecar\"");
        if (sidecarStart != std::string::npos) {
          size_t scBegin = line.find('{', sidecarStart);
          size_t scEnd = line.find('}', scBegin);
          if (scBegin != std::string::npos && scEnd != std::string::npos) {
            std::string sidecar = line.substr(scBegin, scEnd - scBegin + 1);
            
            // Extract in_time and out_time
            size_t inTimePos = sidecar.find("\"in_time\"");
            size_t outTimePos = sidecar.find("\"out_time\"");
            
            if (inTimePos != std::string::npos) {
              size_t colonPos = sidecar.find(':', inTimePos);
              size_t quoteStart = sidecar.find('"', colonPos);
              size_t quoteEnd = sidecar.find('"', quoteStart + 1);
              if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                entry["sidecar_in_time"] = sidecar.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
              }
            }
            
            if (outTimePos != std::string::npos) {
              size_t colonPos = sidecar.find(':', outTimePos);
              size_t quoteStart = sidecar.find('"', colonPos);
              size_t quoteEnd = sidecar.find('"', quoteStart + 1);
              if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                entry["sidecar_out_time"] = sidecar.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
              }
            }
          }
        }
        
        // Extract sfc_time if available
        size_t sfcTimePos = line.find("\"sfc_time\"");
        if (sfcTimePos != std::string::npos) {
          size_t colonPos = line.find(':', sfcTimePos);
          size_t quoteStart = line.find('"', colonPos);
          size_t quoteEnd = line.find('"', quoteStart + 1);
          if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
            entry["sfc_time"] = line.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
          }
        }
        
        if (!entry.empty()) {
          logEntries.push_back(entry);
        }
      } catch (const std::exception& e) {
        NLSR_LOG_WARN("Error parsing line " + std::to_string(lineCount) + ": " + std::string(e.what()));
        continue;
      }
    }
    
    NLSR_LOG_DEBUG("Parsed " + std::to_string(logEntries.size()) + " log entries from " + m_logPath);
    if (logEntries.size() > 0) {
      NLSR_LOG_DEBUG("First entry has " + std::to_string(logEntries[0].size()) + " keys");
      for (const auto& [key, value] : logEntries[0]) {
        NLSR_LOG_DEBUG("  Key: " << key << " = " << value.substr(0, 50));
      }
    }
  }
  catch (const std::exception& e) {
    NLSR_LOG_ERROR("Error reading log file: " + std::string(e.what()));
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

// Extended constructor with LSDB and ConfParameter
SidecarStatsHandler::SidecarStatsHandler(ndn::mgmt::Dispatcher& dispatcher,
                                         Lsdb& lsdb,
                                         ConfParameter& confParam,
                                         const std::string& logPath)
  : m_logPath(logPath)
  , m_isRegistered(false)
  , m_lsdb(&lsdb)
  , m_confParam(&confParam)
{
  try {
    // Register dataset handlers with explicit logging
    NLSR_LOG_INFO("Registering sidecar-stats dataset handler");
    dispatcher.addStatusDataset(dataset::SIDECAR_STATS_DATASET,
                               ndn::mgmt::makeAcceptAllAuthorization(),
                               std::bind(&SidecarStatsHandler::publishSidecarStats, this, _1, _2, _3));

    NLSR_LOG_INFO("Registering service-stats dataset handler");
    dispatcher.addStatusDataset(dataset::SIDECAR_SERVICE_STATS_DATASET,
                               ndn::mgmt::makeAcceptAllAuthorization(),
                               std::bind(&SidecarStatsHandler::publishServiceStats, this, _1, _2, _3));

    NLSR_LOG_INFO("Registering sfc-stats dataset handler");
    dispatcher.addStatusDataset(dataset::SIDECAR_SFC_STATS_DATASET,
                               ndn::mgmt::makeAcceptAllAuthorization(),
                               std::bind(&SidecarStatsHandler::publishSfcStats, this, _1, _2, _3));

    NLSR_LOG_INFO("Registering function-info dataset handler");
    dispatcher.addStatusDataset(dataset::FUNCTION_INFO_DATASET,
                               ndn::mgmt::makeAcceptAllAuthorization(),
                               std::bind(&SidecarStatsHandler::publishFunctionInfo, this, _1, _2, _3));

    m_isRegistered = true;
    NLSR_LOG_INFO("All sidecar dataset handlers registered successfully");
  }
  catch (const std::exception& e) {
    NLSR_LOG_ERROR("Failed to register sidecar dataset handlers: " + std::string(e.what()));
    m_isRegistered = false;
    throw;
  }
}

std::vector<std::map<std::string, std::string>>
SidecarStatsHandler::parseSidecarLogWithTimeWindow(uint32_t windowSeconds) const
{
  NLSR_LOG_DEBUG("parseSidecarLogWithTimeWindow called: windowSeconds=" << windowSeconds);
  
  std::vector<std::map<std::string, std::string>> logEntries;
  
  if (!m_confParam) {
    NLSR_LOG_WARN("ConfParameter not available, falling back to parseSidecarLog()");
    return parseSidecarLog();
  }
  
  try {
    std::ifstream logFile(m_logPath);
    
    if (!logFile.is_open()) {
      NLSR_LOG_WARN("Cannot open log file: " + m_logPath);
      return logEntries;
    }
    
    // Helper function to parse timestamp string to time_point
    auto parseTimestampToTimePoint = [](const std::string& timestamp) -> std::chrono::system_clock::time_point {
      try {
        // Format: "2025-11-12 02:58:50.676086"
        if (timestamp.length() < 19) {
          return std::chrono::system_clock::time_point::min();
        }
        
        int year = std::stoi(timestamp.substr(0, 4));
        int month = std::stoi(timestamp.substr(5, 2));
        int day = std::stoi(timestamp.substr(8, 2));
        int hour = std::stoi(timestamp.substr(11, 2));
        int minute = std::stoi(timestamp.substr(14, 2));
        int second = std::stoi(timestamp.substr(17, 2));
        int microsecond = 0;
        
        if (timestamp.length() > 19) {
          std::string microsecStr = timestamp.substr(20);
          if (!microsecStr.empty() && microsecStr.length() <= 6) {
            // Pad to 6 digits if necessary
            while (microsecStr.length() < 6) {
              microsecStr += "0";
            }
            microsecStr = microsecStr.substr(0, 6);
            microsecond = std::stoi(microsecStr);
          }
        }
        
        std::tm timeinfo = {};
        timeinfo.tm_year = year - 1900;
        timeinfo.tm_mon = month - 1;
        timeinfo.tm_mday = day;
        timeinfo.tm_hour = hour;
        timeinfo.tm_min = minute;
        timeinfo.tm_sec = second;
        
        std::time_t time = std::mktime(&timeinfo);
        auto timePoint = std::chrono::system_clock::from_time_t(time);
        timePoint += std::chrono::microseconds(microsecond);
        
        return timePoint;
      } catch (const std::exception&) {
        return std::chrono::system_clock::time_point::min();
      }
    };
    
    // First pass: Read all entries and find the latest timestamp
    std::vector<std::pair<std::map<std::string, std::string>, std::chrono::system_clock::time_point>> allEntries;
    std::string line;
    int lineCount = 0;
    auto latestTimestamp = std::chrono::system_clock::time_point::min();
    
    // Read all lines and parse timestamps
    while (std::getline(logFile, line)) {
      lineCount++;
      try {
        // Parse the log entry (same logic as parseSidecarLog)
        std::map<std::string, std::string> entry;
        
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (line.empty() || line.front() != '{' || line.back() != '}') {
          continue;
        }
        
        // Extract service_call information
        size_t scStart = line.find("\"service_call\"");
        if (scStart != std::string::npos) {
          size_t scBegin = line.find('{', scStart);
          size_t scEnd = line.find('}', scBegin);
          if (scBegin != std::string::npos && scEnd != std::string::npos) {
            std::string serviceCall = line.substr(scBegin, scEnd - scBegin + 1);
            
            size_t inTimePos = serviceCall.find("\"in_time\"");
            size_t outTimePos = serviceCall.find("\"out_time\"");
            
            if (inTimePos != std::string::npos) {
              size_t colonPos = serviceCall.find(':', inTimePos);
              size_t quoteStart = serviceCall.find('"', colonPos);
              size_t quoteEnd = serviceCall.find('"', quoteStart + 1);
              if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                entry["service_call_in_time"] = serviceCall.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
              }
            }
            
            if (outTimePos != std::string::npos) {
              size_t colonPos = serviceCall.find(':', outTimePos);
              size_t quoteStart = serviceCall.find('"', colonPos);
              size_t quoteEnd = serviceCall.find('"', quoteStart + 1);
              if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                entry["service_call_out_time"] = serviceCall.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
              }
            }
          }
        }
        
        // Extract sidecar information
        size_t sidecarStart = line.find("\"sidecar\"");
        if (sidecarStart != std::string::npos) {
          size_t scBegin = line.find('{', sidecarStart);
          size_t scEnd = line.find('}', scBegin);
          if (scBegin != std::string::npos && scEnd != std::string::npos) {
            std::string sidecar = line.substr(scBegin, scEnd - scBegin + 1);
            
            size_t inTimePos = sidecar.find("\"in_time\"");
            size_t outTimePos = sidecar.find("\"out_time\"");
            
            if (inTimePos != std::string::npos) {
              size_t colonPos = sidecar.find(':', inTimePos);
              size_t quoteStart = sidecar.find('"', colonPos);
              size_t quoteEnd = sidecar.find('"', quoteStart + 1);
              if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                entry["sidecar_in_time"] = sidecar.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
              }
            }
            
            if (outTimePos != std::string::npos) {
              size_t colonPos = sidecar.find(':', outTimePos);
              size_t quoteStart = sidecar.find('"', colonPos);
              size_t quoteEnd = sidecar.find('"', quoteStart + 1);
              if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
                entry["sidecar_out_time"] = sidecar.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
              }
            }
          }
        }
        
        // Extract sfc_time if available
        size_t sfcTimePos = line.find("\"sfc_time\"");
        if (sfcTimePos != std::string::npos) {
          size_t colonPos = line.find(':', sfcTimePos);
          size_t quoteStart = line.find('"', colonPos);
          size_t quoteEnd = line.find('"', quoteStart + 1);
          if (quoteStart != std::string::npos && quoteEnd != std::string::npos) {
            entry["sfc_time"] = line.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
          }
        }
        
        // Store entry with its timestamp
        if (!entry.empty()) {
          // Use service_call_in_time if available, otherwise use sidecar_in_time
          std::string timeStr;
          if (entry.find("service_call_in_time") != entry.end()) {
            timeStr = entry["service_call_in_time"];
          } else if (entry.find("sidecar_in_time") != entry.end()) {
            timeStr = entry["sidecar_in_time"];
          }
          
          if (!timeStr.empty()) {
            auto entryTime = parseTimestampToTimePoint(timeStr);
            if (entryTime != std::chrono::system_clock::time_point::min()) {
              allEntries.push_back({entry, entryTime});
              if (entryTime > latestTimestamp) {
                latestTimestamp = entryTime;
              }
            } else {
              // If timestamp parsing failed, include the entry with min timestamp (will be filtered out)
              allEntries.push_back({entry, std::chrono::system_clock::time_point::min()});
            }
          } else {
            // If no timestamp available, include the entry with min timestamp (will be filtered out)
            allEntries.push_back({entry, std::chrono::system_clock::time_point::min()});
          }
        }
      } catch (const std::exception& e) {
        NLSR_LOG_WARN("Error parsing line " + std::to_string(lineCount) + ": " + std::string(e.what()));
        continue;
      }
    }
    
    // Second pass: Filter entries based on time window relative to latest timestamp
    if (latestTimestamp == std::chrono::system_clock::time_point::min()) {
      NLSR_LOG_DEBUG("No valid timestamps found in log entries");
      return logEntries;
    }
    
    // Calculate time window: from (latestTimestamp - windowSeconds) to latestTimestamp
    auto windowStart = latestTimestamp - std::chrono::seconds(windowSeconds);
    int entriesInWindow = 0;
    
    NLSR_LOG_DEBUG("Latest entry timestamp: " << std::chrono::duration_cast<std::chrono::seconds>(latestTimestamp.time_since_epoch()).count()
                  << ", window start: " << std::chrono::duration_cast<std::chrono::seconds>(windowStart.time_since_epoch()).count()
                  << ", window duration: " << windowSeconds << " seconds");
    
    for (const auto& [entry, entryTime] : allEntries) {
      if (entryTime >= windowStart && entryTime <= latestTimestamp) {
        logEntries.push_back(entry);
        entriesInWindow++;
      }
    }
    
    NLSR_LOG_DEBUG("Parsed " << entriesInWindow << " entries within time window (total lines: " << lineCount 
                  << ", total entries: " << allEntries.size() << ")");
  }
  catch (const std::exception& e) {
    NLSR_LOG_ERROR("Error reading log file: " + std::string(e.what()));
  }
  
  return logEntries;
}

double
SidecarStatsHandler::calculateUtilization(const std::vector<std::map<std::string, std::string>>& entries,
                                           uint32_t windowSeconds) const
{
  if (entries.empty() || windowSeconds == 0) {
    return 0.0;
  }
  
  // Helper function to parse timestamp string to seconds since epoch
  auto parseTimestamp = [](const std::string& timestamp) -> double {
    try {
      if (timestamp.length() < 19) {
        return 0.0;
      }
      
      int year = std::stoi(timestamp.substr(0, 4));
      int month = std::stoi(timestamp.substr(5, 2));
      int day = std::stoi(timestamp.substr(8, 2));
      int hour = std::stoi(timestamp.substr(11, 2));
      int minute = std::stoi(timestamp.substr(14, 2));
      int second = std::stoi(timestamp.substr(17, 2));
      double microsecond = 0.0;
      
      if (timestamp.length() > 19) {
        std::string microsecStr = timestamp.substr(20);
        if (!microsecStr.empty()) {
          microsecond = std::stod("0." + microsecStr);
        }
      }
      
      std::tm timeinfo = {};
      timeinfo.tm_year = year - 1900;
      timeinfo.tm_mon = month - 1;
      timeinfo.tm_mday = day;
      timeinfo.tm_hour = hour;
      timeinfo.tm_min = minute;
      timeinfo.tm_sec = second;
      
      std::time_t time = std::mktime(&timeinfo);
      return static_cast<double>(time) + microsecond;
    } catch (const std::exception&) {
      return 0.0;
    }
  };
  
  double totalProcessingTime = 0.0;
  int validEntries = 0;
  
  // Calculate total processing time from all entries
  for (const auto& entry : entries) {
    double processingTime = 0.0;
    
    // Priority: 1) service_call times, 2) sidecar times
    if (entry.find("service_call_in_time") != entry.end() && 
        entry.find("service_call_out_time") != entry.end()) {
      try {
        double inTime = parseTimestamp(entry.at("service_call_in_time"));
        double outTime = parseTimestamp(entry.at("service_call_out_time"));
        if (inTime > 0.0 && outTime > 0.0) {
          processingTime = outTime - inTime;
        }
      } catch (const std::exception&) {
        // Skip this entry
        continue;
      }
    } else if (entry.find("sidecar_in_time") != entry.end() && 
               entry.find("sidecar_out_time") != entry.end()) {
      try {
        double inTime = parseTimestamp(entry.at("sidecar_in_time"));
        double outTime = parseTimestamp(entry.at("sidecar_out_time"));
        if (inTime > 0.0 && outTime > 0.0) {
          processingTime = outTime - inTime;
        }
      } catch (const std::exception&) {
        // Skip this entry
        continue;
      }
    }
    
    if (processingTime > 0.0) {
      totalProcessingTime += processingTime;
      validEntries++;
    }
  }
  
  // Calculate utilization: total processing time / time window
  double utilization = totalProcessingTime / static_cast<double>(windowSeconds);
  
  // Clamp to [0.0, 1.0] (utilization cannot exceed 100%)
  if (utilization > 1.0) {
    utilization = 1.0;
  }
  
  NLSR_LOG_DEBUG("Calculated utilization: " << utilization 
                 << " (totalProcessingTime=" << totalProcessingTime 
                 << "s, windowSeconds=" << windowSeconds 
                 << ", validEntries=" << validEntries << ")");
  
  return utilization;
}

ServiceFunctionInfo
SidecarStatsHandler::convertStatsToServiceFunctionInfo() const
{
  ServiceFunctionInfo info;
  
  // Initialize with default values
  info.processingTime = 0.0;  // Now stores utilization (0.0 ~ 1.0)
  info.load = 0.0;
  info.usageCount = 0;
  info.lastUpdateTime = ndn::time::system_clock::now();
  info.processingWeight = 0.4;  // Default weight
  info.loadWeight = 0.4;       // Default weight
  info.usageWeight = 0.2;       // Default weight
  
  if (!m_confParam) {
    NLSR_LOG_WARN("ConfParameter not available, cannot calculate utilization");
    return info;
  }
  
  // Get weight information from configuration file
  info.processingWeight = m_confParam->getProcessingWeight();
  info.loadWeight = m_confParam->getLoadWeight();
  info.usageWeight = m_confParam->getUsageWeight();
  
  // Get time window from configuration
  uint32_t windowSeconds = m_confParam->getUtilizationWindowSeconds();
  NLSR_LOG_DEBUG("Calculating utilization with time window: " << windowSeconds << " seconds");
  
  // Parse log entries within time window (based on entry timestamps)
  auto entries = parseSidecarLogWithTimeWindow(windowSeconds);
  
  if (entries.empty()) {
    NLSR_LOG_DEBUG("No log entries found within time window, returning default values");
    return info;
  }
  
  // Find the latest entry timestamp to set lastUpdateTime
  // This is done by parsing all entries again to find the latest timestamp
  auto parseTimestampToTimePoint = [](const std::string& timestamp) -> std::chrono::system_clock::time_point {
    try {
      if (timestamp.length() < 19) {
        return std::chrono::system_clock::time_point::min();
      }
      
      int year = std::stoi(timestamp.substr(0, 4));
      int month = std::stoi(timestamp.substr(5, 2));
      int day = std::stoi(timestamp.substr(8, 2));
      int hour = std::stoi(timestamp.substr(11, 2));
      int minute = std::stoi(timestamp.substr(14, 2));
      int second = std::stoi(timestamp.substr(17, 2));
      int microsecond = 0;
      
      if (timestamp.length() > 19) {
        std::string microsecStr = timestamp.substr(20);
        if (!microsecStr.empty() && microsecStr.length() <= 6) {
          while (microsecStr.length() < 6) {
            microsecStr += "0";
          }
          microsecStr = microsecStr.substr(0, 6);
          microsecond = std::stoi(microsecStr);
        }
      }
      
      std::tm timeinfo = {};
      timeinfo.tm_year = year - 1900;
      timeinfo.tm_mon = month - 1;
      timeinfo.tm_mday = day;
      timeinfo.tm_hour = hour;
      timeinfo.tm_min = minute;
      timeinfo.tm_sec = second;
      
      std::time_t time = std::mktime(&timeinfo);
      auto timePoint = std::chrono::system_clock::from_time_t(time);
      timePoint += std::chrono::microseconds(microsecond);
      
      return timePoint;
    } catch (const std::exception&) {
      return std::chrono::system_clock::time_point::min();
    }
  };
  
  auto latestTimestamp = std::chrono::system_clock::time_point::min();
  for (const auto& entry : entries) {
    std::string timeStr;
    if (entry.find("service_call_in_time") != entry.end()) {
      timeStr = entry.at("service_call_in_time");
    } else if (entry.find("sidecar_in_time") != entry.end()) {
      timeStr = entry.at("sidecar_in_time");
    }
    
    if (!timeStr.empty()) {
      auto entryTime = parseTimestampToTimePoint(timeStr);
      if (entryTime != std::chrono::system_clock::time_point::min() && entryTime > latestTimestamp) {
        latestTimestamp = entryTime;
      }
    }
  }
  
  // Set lastUpdateTime to the latest entry timestamp
  if (latestTimestamp != std::chrono::system_clock::time_point::min()) {
    info.lastUpdateTime = latestTimestamp;
  }
  
  // Check if the latest entry is too old (more than windowSeconds * 2 ago)
  // If so, set processingTime to 0
  auto now = std::chrono::system_clock::now();
  auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::seconds>(now - latestTimestamp).count();
  uint32_t staleThreshold = windowSeconds * 2;  // Consider stale if older than 2x window
  
  if (timeSinceLastUpdate > static_cast<int64_t>(staleThreshold)) {
    NLSR_LOG_DEBUG("Latest entry is too old (" << timeSinceLastUpdate << "s ago, threshold: " << staleThreshold 
                  << "s), setting processingTime to 0");
    info.processingTime = 0.0;
    return info;
  }
  
  // Calculate utilization
  info.processingTime = calculateUtilization(entries, windowSeconds);
  
  NLSR_LOG_DEBUG("ServiceFunctionInfo: utilization=" << info.processingTime 
                 << " (calculated from " << entries.size() << " entries, lastUpdateTime: " 
                 << std::chrono::duration_cast<std::chrono::seconds>(latestTimestamp.time_since_epoch()).count() << ")");
  
  return info;
}

ndn::Name
SidecarStatsHandler::getServiceFunctionPrefix() const
{
  // Get service function prefix from configuration
  if (m_confParam) {
    return m_confParam->getServiceFunctionPrefix();
  }
  // Fallback to default if ConfParameter is not available
  return ndn::Name("/relay");
}

void
SidecarStatsHandler::updateNameLsaWithStats()
{
  NLSR_LOG_DEBUG("updateNameLsaWithStats called");
  
  if (!m_lsdb || !m_confParam) {
    NLSR_LOG_WARN("LSDB or ConfParameter not available, skipping NameLSA update");
    return;
  }
  
  try {
    // Convert statistics to ServiceFunctionInfo (now calculates utilization)
    NLSR_LOG_DEBUG("Converting stats to ServiceFunctionInfo (utilization-based)...");
    ServiceFunctionInfo sfInfo = convertStatsToServiceFunctionInfo();
    
    NLSR_LOG_DEBUG("ServiceFunctionInfo: processingTime=" << sfInfo.processingTime 
                   << ", load=" << sfInfo.load 
                   << ", usageCount=" << sfInfo.usageCount);
    
    // Get the router's own NameLSA
    const ndn::Name& routerPrefix = m_confParam->getRouterPrefix();
    NLSR_LOG_DEBUG("Looking for NameLSA for router: " << routerPrefix);
    auto nameLsa = m_lsdb->findLsa<NameLsa>(routerPrefix);
    
    if (!nameLsa) {
      NLSR_LOG_WARN("Own NameLSA not found, cannot update Service Function info");
      return;
    }
    
    // Get service function prefix
    ndn::Name servicePrefix = getServiceFunctionPrefix();
    NLSR_LOG_DEBUG("Service function prefix: " << servicePrefix);
    
    // Update Service Function information
    nameLsa->setServiceFunctionInfo(servicePrefix, sfInfo);
    NLSR_LOG_DEBUG("Service Function info set in NameLSA");
    
    // Rebuild and install the updated NameLSA
    // This will increment the sequence number and trigger sync
    NLSR_LOG_DEBUG("Rebuilding and installing NameLSA...");
    m_lsdb->buildAndInstallOwnNameLsa();
    
    NLSR_LOG_INFO("Updated NameLSA with Service Function info: prefix=" << servicePrefix
                  << ", processingTime=" << sfInfo.processingTime
                  << ", load=" << sfInfo.load
                  << ", usageCount=" << sfInfo.usageCount);
  }
  catch (const std::exception& e) {
    NLSR_LOG_ERROR("Error updating NameLSA with stats: " + std::string(e.what()));
  }
}

void
SidecarStatsHandler::startLogMonitoring(ndn::Scheduler& scheduler, uint32_t intervalMs)
{
  NLSR_LOG_INFO("startLogMonitoring called with interval: " << intervalMs << "ms, logPath: " << m_logPath);
  
  if (!m_lsdb || !m_confParam) {
    NLSR_LOG_WARN("LSDB or ConfParameter not available, cannot start log monitoring");
    return;
  }
  
  // Calculate hash of current log file for change detection
  try {
    std::ifstream logFile(m_logPath);
    if (!logFile.is_open()) {
      NLSR_LOG_WARN("Cannot open log file for initial hash calculation: " + m_logPath);
      m_lastLogHash = "";
    } else {
      std::string content((std::istreambuf_iterator<char>(logFile)),
                          std::istreambuf_iterator<char>());
      std::hash<std::string> hasher;
      m_lastLogHash = std::to_string(hasher(content));
      NLSR_LOG_DEBUG("Initial log file hash calculated: " << m_lastLogHash.substr(0, std::min(16UL, m_lastLogHash.size())) << "... (size: " << content.size() << " bytes)");
    }
  } catch (const std::exception& e) {
    NLSR_LOG_ERROR("Error reading log file for initial hash: " + std::string(e.what()));
    m_lastLogHash = "";
  }
  
  // Schedule periodic monitoring
  // Use shared_ptr to allow recursive scheduling of the same function
  auto monitorFunc = std::make_shared<std::function<void()>>();
  *monitorFunc = [this, &scheduler, intervalMs, monitorFunc]() {
    NLSR_LOG_DEBUG("Log monitoring check triggered");
    try {
      // Read current log file
      std::ifstream logFile(m_logPath);
      if (!logFile.is_open()) {
        NLSR_LOG_DEBUG("Cannot open log file for monitoring: " + m_logPath);
        // Schedule next check using the same function
        scheduler.schedule(ndn::time::milliseconds(intervalMs), *monitorFunc);
        return;
      }
      
      std::string content((std::istreambuf_iterator<char>(logFile)),
                          std::istreambuf_iterator<char>());
      std::hash<std::string> hasher;
      std::string currentHash = std::to_string(hasher(content));
      
      std::string lastHashPreview = m_lastLogHash.empty() ? "(empty)" : m_lastLogHash.substr(0, std::min(16UL, m_lastLogHash.size()));
      NLSR_LOG_DEBUG("Current log file hash: " << currentHash.substr(0, std::min(16UL, currentHash.size())) << "... (size: " << content.size() << " bytes), last hash: " << lastHashPreview << "...");
      
      // Check if log file has changed
      if (m_lastLogHash.empty() || currentHash != m_lastLogHash) {
        std::string oldHashPreview = m_lastLogHash.empty() ? "(empty)" : m_lastLogHash.substr(0, std::min(16UL, m_lastLogHash.size()));
        NLSR_LOG_INFO("Log file changed, updating NameLSA (old hash: " << oldHashPreview << "..., new hash: " << currentHash.substr(0, std::min(16UL, currentHash.size())) << "...)");
        m_lastLogHash = currentHash;
        updateNameLsaWithStats();
      } else {
        NLSR_LOG_DEBUG("Log file unchanged, skipping update");
      }
      
      // Schedule next check using the same function
      scheduler.schedule(ndn::time::milliseconds(intervalMs), *monitorFunc);
    }
    catch (const std::exception& e) {
      NLSR_LOG_ERROR("Error in log monitoring: " + std::string(e.what()));
      // Continue monitoring even on error
      scheduler.schedule(ndn::time::milliseconds(intervalMs), *monitorFunc);
    }
  };
  
  scheduler.schedule(ndn::time::milliseconds(intervalMs), *monitorFunc);
  NLSR_LOG_INFO("Started log file monitoring with interval: " << intervalMs << "ms, logPath: " << m_logPath);
}

} // namespace nlsr 