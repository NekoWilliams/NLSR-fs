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

ServiceFunctionInfo
SidecarStatsHandler::convertStatsToServiceFunctionInfo(const std::map<std::string, std::string>& stats) const
{
  ServiceFunctionInfo info;
  
  // Initialize with default values
  info.processingTime = 0.0;
  info.load = 0.0;
  info.usageCount = 0;
  info.lastUpdateTime = ndn::time::system_clock::now();
  
  // Helper function to parse timestamp string to seconds since epoch
  auto parseTimestamp = [](const std::string& timestamp) -> double {
    try {
      // Format: "2025-11-12 02:58:50.676086"
      // Parse year, month, day, hour, minute, second, microsecond
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
        // Extract microseconds
        std::string microsecStr = timestamp.substr(20);
        if (!microsecStr.empty()) {
          microsecond = std::stod("0." + microsecStr);
        }
      }
      
      // Convert to seconds since epoch (simplified calculation)
      // Note: This is a simplified implementation. For production, use proper date/time library
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
  
  // Extract processing time
  // Priority: 1) service_call times, 2) sidecar times, 3) direct processing_time
  if (stats.find("service_call_in_time") != stats.end() && 
      stats.find("service_call_out_time") != stats.end()) {
    try {
      double inTime = parseTimestamp(stats.at("service_call_in_time"));
      double outTime = parseTimestamp(stats.at("service_call_out_time"));
      if (inTime > 0.0 && outTime > 0.0) {
        info.processingTime = outTime - inTime;
      }
    } catch (const std::exception& e) {
      NLSR_LOG_WARN("Failed to calculate processing_time from service_call times: " + std::string(e.what()));
    }
  } else if (stats.find("sidecar_in_time") != stats.end() && 
             stats.find("sidecar_out_time") != stats.end()) {
    try {
      double inTime = parseTimestamp(stats.at("sidecar_in_time"));
      double outTime = parseTimestamp(stats.at("sidecar_out_time"));
      if (inTime > 0.0 && outTime > 0.0) {
        info.processingTime = outTime - inTime;
      }
    } catch (const std::exception& e) {
      NLSR_LOG_WARN("Failed to calculate processing_time from sidecar times: " + std::string(e.what()));
    }
  } else if (stats.find("processing_time") != stats.end()) {
    try {
      info.processingTime = std::stod(stats.at("processing_time"));
    } catch (const std::exception& e) {
      NLSR_LOG_WARN("Failed to parse processing_time: " + std::string(e.what()));
    }
  }
  
  // Extract load (if available in stats)
  if (stats.find("load") != stats.end()) {
    try {
      info.load = std::stod(stats.at("load"));
    } catch (const std::exception& e) {
      NLSR_LOG_WARN("Failed to parse load: " + std::string(e.what()));
    }
  }
  
  // Extract usage count
  if (stats.find("usage_count") != stats.end()) {
    try {
      info.usageCount = static_cast<uint32_t>(std::stoul(stats.at("usage_count")));
    } catch (const std::exception& e) {
      NLSR_LOG_WARN("Failed to parse usage_count: " + std::string(e.what()));
    }
  }
  
  return info;
}

ndn::Name
SidecarStatsHandler::getServiceFunctionPrefix() const
{
  // Default prefix for service function
  // This can be customized based on configuration
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
    NLSR_LOG_DEBUG("Getting latest stats from log file...");
    auto stats = getLatestStats();
    
    NLSR_LOG_DEBUG("Retrieved " << stats.size() << " stat entries");
    
    if (stats.find("error") != stats.end()) {
      NLSR_LOG_DEBUG("No valid statistics available, skipping NameLSA update: " << stats.at("error"));
      return;
    }
    
    // Convert statistics to ServiceFunctionInfo
    NLSR_LOG_DEBUG("Converting stats to ServiceFunctionInfo...");
    ServiceFunctionInfo sfInfo = convertStatsToServiceFunctionInfo(stats);
    
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