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
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NLSR, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "name-lsa.hpp"
#include "logger.hpp"
#include "tlv-nlsr.hpp"
#include <ndn-cxx/encoding/block-helpers.hpp>
#include <ndn-cxx/encoding/encoding-buffer.hpp>
#include <ndn-cxx/encoding/tlv.hpp>
#include <ndn-cxx/util/span.hpp>
#include <vector>
#include <algorithm>

namespace nlsr {

INIT_LOGGER(lsa.NameLsa);

namespace {

template<ndn::encoding::Tag TAG>
size_t
nlsrPrependDoubleBlock(ndn::encoding::EncodingImpl<TAG>& encoder, uint32_t type, double value)
{
  size_t totalLength = 0;
  uint8_t buffer[sizeof(double)];
  memcpy(buffer, &value, sizeof(double));
  
  ndn::span<const uint8_t> bytes(buffer, sizeof(double));
  totalLength += encoder.prependBytes(bytes);
  totalLength += encoder.prependVarNumber(sizeof(double));
  totalLength += encoder.prependVarNumber(type);
  
  return totalLength;
}

// Removed unused function extractDoubleFromBlock

} // anonymous namespace

NameLsa::NameLsa(const ndn::Name& originRouter, uint64_t seqNo,
                 const ndn::time::system_clock::time_point& timepoint,
                 const NamePrefixList& npl)
  : Lsa(originRouter, seqNo, timepoint)
{
  for (const auto& name : npl.getPrefixInfo()) {
    addName(name);
  }
}

NameLsa::NameLsa(const ndn::Block& block)
{
  wireDecode(block);
}

void
NameLsa::setServiceFunctionInfo(const ndn::Name& name, const ServiceFunctionInfo& info)
{
  m_wire.reset();  // 既存のワイヤーエンコーディングを無効化
  m_serviceFunctionInfo[name] = info;
}

ServiceFunctionInfo
NameLsa::getServiceFunctionInfo(const ndn::Name& name) const
{
  NLSR_LOG_DEBUG("getServiceFunctionInfo called: name=" << name);
  NLSR_LOG_DEBUG("m_serviceFunctionInfo map size: " << m_serviceFunctionInfo.size());
  
  // Debug: Print all entries in the map
  for (const auto& [mapName, info] : m_serviceFunctionInfo) {
    NLSR_LOG_DEBUG("  Map entry: " << mapName << " -> processingTime=" << info.processingTime
                  << ", load=" << info.load << ", usageCount=" << info.usageCount
                  << ", processingWeight=" << info.processingWeight
                  << ", loadWeight=" << info.loadWeight
                  << ", usageWeight=" << info.usageWeight);
  }
  
  auto it = m_serviceFunctionInfo.find(name);
  if (it != m_serviceFunctionInfo.end()) {
    NLSR_LOG_DEBUG("Service Function info found for " << name << ": processingTime=" << it->second.processingTime
                  << ", load=" << it->second.load << ", usageCount=" << it->second.usageCount
                  << ", processingWeight=" << it->second.processingWeight
                  << ", loadWeight=" << it->second.loadWeight
                  << ", usageWeight=" << it->second.usageWeight);
    return it->second;
  } else {
    NLSR_LOG_DEBUG("Service Function info NOT found for " << name << " in map");
    return ServiceFunctionInfo{};
  }
}

template<ndn::encoding::Tag TAG>
size_t
NameLsa::wireEncode(ndn::EncodingImpl<TAG>& block) const
{
  size_t totalLength = 0;

  // エンコードService Function情報
  NLSR_LOG_DEBUG("wireEncode: Encoding " << m_serviceFunctionInfo.size() << " Service Function info entries");
  for (const auto& [name, info] : m_serviceFunctionInfo) {
    NLSR_LOG_DEBUG("wireEncode: Encoding Service Function: " << name 
                  << ", processingTime=" << info.processingTime
                  << ", load=" << info.load << ", usageCount=" << info.usageCount
                  << ", processingWeight=" << info.processingWeight
                  << ", loadWeight=" << info.loadWeight
                  << ", usageWeight=" << info.usageWeight);
    
    size_t sfInfoLength = 0;
    
    // Service Function情報をエンコード（weight情報を含む）
    sfInfoLength += nlsrPrependDoubleBlock(block, nlsr::tlv::UsageWeight, info.usageWeight);
    sfInfoLength += nlsrPrependDoubleBlock(block, nlsr::tlv::LoadWeight, info.loadWeight);
    sfInfoLength += nlsrPrependDoubleBlock(block, nlsr::tlv::ProcessingWeight, info.processingWeight);
    sfInfoLength += nlsrPrependDoubleBlock(block, nlsr::tlv::ProcessingTime, info.processingTime);
    sfInfoLength += nlsrPrependDoubleBlock(block, nlsr::tlv::Load, info.load);
    sfInfoLength += block.prependVarNumber(info.usageCount);
    sfInfoLength += block.prependVarNumber(nlsr::tlv::UsageCount);
    
    // 名前をエンコード
    sfInfoLength += name.wireEncode(block);
    
    // Service Function情報全体の長さを追加
    totalLength += sfInfoLength;
    totalLength += block.prependVarNumber(sfInfoLength);
    totalLength += block.prependVarNumber(nlsr::tlv::ServiceFunction);
    
    NLSR_LOG_DEBUG("wireEncode: Encoded Service Function: " << name << ", length=" << sfInfoLength);
  }

  // 名前プレフィックスリストをエンコード
  for (const auto& name : m_npl.getPrefixInfo()) {
    totalLength += name.wireEncode(block);
  }

  // LSA共通部分をエンコード
  totalLength += Lsa::wireEncode(block);

  totalLength += block.prependVarNumber(totalLength);
  totalLength += block.prependVarNumber(nlsr::tlv::NameLsa);

  return totalLength;
}

NDN_CXX_DEFINE_WIRE_ENCODE_INSTANTIATIONS(NameLsa);

const ndn::Block&
NameLsa::wireEncode() const
{
  if (m_wire.hasWire()) {
    return m_wire;
  }

  ndn::EncodingEstimator estimator;
  size_t estimatedSize = wireEncode(estimator);

  ndn::EncodingBuffer buffer(estimatedSize, 0);
  wireEncode(buffer);

  m_wire = buffer.block();

  return m_wire;
}

void
NameLsa::wireDecode(const ndn::Block& wire)
{
  m_wire = wire;

  if (m_wire.type() != nlsr::tlv::NameLsa) {
    NDN_THROW(Error("NameLsa", m_wire.type()));
  }

  m_wire.parse();

  auto val = m_wire.elements_begin();

  if (val != m_wire.elements_end() && val->type() == nlsr::tlv::Lsa) {
    Lsa::wireDecode(*val);
    ++val;
  }
  else {
    NDN_THROW(Error("Missing required Lsa field"));
  }

  NamePrefixList npl;
  m_serviceFunctionInfo.clear();  // Clear existing Service Function info
  
  NLSR_LOG_DEBUG("wireDecode: Starting decode, will look for Service Function TLV elements");
  int serviceFunctionCount = 0;
  
  for (; val != m_wire.elements_end(); ++val) {
    if (val->type() == nlsr::tlv::PrefixInfo) {
      npl.insert(PrefixInfo(*val));
    }
    else if (val->type() == nlsr::tlv::ServiceFunction) {
      // Decode Service Function information
      NLSR_LOG_DEBUG("wireDecode: Found ServiceFunction TLV element #" << (serviceFunctionCount + 1));
      val->parse();
      
      ndn::Name serviceName;
      ServiceFunctionInfo sfInfo;
      
      // Initialize with default values
      sfInfo.processingTime = 0.0;
      sfInfo.load = 0.0;
      sfInfo.usageCount = 0;
      sfInfo.lastUpdateTime = ndn::time::system_clock::now();
      sfInfo.processingWeight = 0.4;  // Default weight
      sfInfo.loadWeight = 0.4;         // Default weight
      sfInfo.usageWeight = 0.2;        // Default weight
      
      // Parse Service Function TLV elements
      // Note: Due to prepend encoding, elements are in reverse order
      // We need to iterate in reverse or collect all elements first
      std::vector<ndn::Block> elements;
      for (auto sfVal = val->elements_begin(); sfVal != val->elements_end(); ++sfVal) {
        elements.push_back(*sfVal);
      }
      
      NLSR_LOG_DEBUG("wireDecode: ServiceFunction TLV has " << elements.size() << " sub-elements");
      
      // Process elements in reverse order (to match encoding order)
      for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
        if (it->type() == ndn::tlv::Name) {
          // Service function name
          serviceName.wireDecode(*it);
          NLSR_LOG_DEBUG("wireDecode: Decoded Service Function name: " << serviceName);
        }
        else if (it->type() == nlsr::tlv::ProcessingTime) {
          // Processing time (double) - 8 bytes
          if (it->value_size() == sizeof(double)) {
            std::memcpy(&sfInfo.processingTime, it->value(), sizeof(double));
            NLSR_LOG_DEBUG("wireDecode: Decoded ProcessingTime: " << sfInfo.processingTime 
                          << " (value_size=" << it->value_size() << ")");
          } else {
            NLSR_LOG_WARN("wireDecode: ProcessingTime value_size mismatch: expected " 
                         << sizeof(double) << ", got " << it->value_size());
          }
        }
        else if (it->type() == nlsr::tlv::Load) {
          // Load (double) - 8 bytes
          if (it->value_size() == sizeof(double)) {
            std::memcpy(&sfInfo.load, it->value(), sizeof(double));
            NLSR_LOG_DEBUG("wireDecode: Decoded Load: " << sfInfo.load 
                          << " (value_size=" << it->value_size() << ")");
          } else {
            NLSR_LOG_WARN("wireDecode: Load value_size mismatch: expected " 
                         << sizeof(double) << ", got " << it->value_size());
          }
        }
        else if (it->type() == nlsr::tlv::UsageCount) {
          // Usage count - encoded as var number
          // The value is stored directly in the block's value
          try {
            // Read the var number from the block's value
            auto begin = it->value_begin();
            auto end = it->value_end();
            sfInfo.usageCount = static_cast<uint32_t>(ndn::tlv::readVarNumber(begin, end));
            NLSR_LOG_DEBUG("wireDecode: Decoded UsageCount: " << sfInfo.usageCount);
          } catch (...) {
            // If parsing fails, try reading as direct value (fallback)
            if (it->value_size() <= 4) {
              uint32_t count = 0;
              std::memcpy(&count, it->value(), std::min(it->value_size(), sizeof(uint32_t)));
              sfInfo.usageCount = count;
              NLSR_LOG_DEBUG("wireDecode: Decoded UsageCount (fallback): " << sfInfo.usageCount);
            }
          }
        }
        else if (it->type() == nlsr::tlv::ProcessingWeight) {
          // Processing weight (double) - 8 bytes
          if (it->value_size() == sizeof(double)) {
            std::memcpy(&sfInfo.processingWeight, it->value(), sizeof(double));
            NLSR_LOG_DEBUG("wireDecode: Decoded ProcessingWeight: " << sfInfo.processingWeight);
          } else {
            NLSR_LOG_WARN("wireDecode: ProcessingWeight value_size mismatch: expected " 
                         << sizeof(double) << ", got " << it->value_size());
          }
        }
        else if (it->type() == nlsr::tlv::LoadWeight) {
          // Load weight (double) - 8 bytes
          if (it->value_size() == sizeof(double)) {
            std::memcpy(&sfInfo.loadWeight, it->value(), sizeof(double));
            NLSR_LOG_DEBUG("wireDecode: Decoded LoadWeight: " << sfInfo.loadWeight);
          } else {
            NLSR_LOG_WARN("wireDecode: LoadWeight value_size mismatch: expected " 
                         << sizeof(double) << ", got " << it->value_size());
          }
        }
        else if (it->type() == nlsr::tlv::UsageWeight) {
          // Usage weight (double) - 8 bytes
          if (it->value_size() == sizeof(double)) {
            std::memcpy(&sfInfo.usageWeight, it->value(), sizeof(double));
            NLSR_LOG_DEBUG("wireDecode: Decoded UsageWeight: " << sfInfo.usageWeight);
          } else {
            NLSR_LOG_WARN("wireDecode: UsageWeight value_size mismatch: expected " 
                         << sizeof(double) << ", got " << it->value_size());
          }
        } else {
          NLSR_LOG_DEBUG("wireDecode: Unknown Service Function sub-element type: " << it->type());
        }
      }
      
      // Store Service Function information if we have a valid name
      if (!serviceName.empty()) {
        m_serviceFunctionInfo[serviceName] = sfInfo;
        serviceFunctionCount++;
        NLSR_LOG_DEBUG("wireDecode: Stored Service Function info: " << serviceName 
                      << " -> processingTime=" << sfInfo.processingTime
                      << ", load=" << sfInfo.load << ", usageCount=" << sfInfo.usageCount
                      << ", processingWeight=" << sfInfo.processingWeight
                      << ", loadWeight=" << sfInfo.loadWeight
                      << ", usageWeight=" << sfInfo.usageWeight);
      } else {
        NLSR_LOG_WARN("wireDecode: Service Function name is empty, skipping");
      }
    }
    else {
      // Unknown element type - skip it (for forward compatibility)
      // NDN_THROW(Error("Name", val->type()));
    }
  }
  m_npl = npl;
  
  NLSR_LOG_DEBUG("wireDecode: Completed decode. Service Function info entries: " << m_serviceFunctionInfo.size());
  for (const auto& [name, info] : m_serviceFunctionInfo) {
    NLSR_LOG_DEBUG("wireDecode: Final Service Function info: " << name 
                  << " -> processingTime=" << info.processingTime
                  << ", load=" << info.load << ", usageCount=" << info.usageCount
                  << ", processingWeight=" << info.processingWeight
                  << ", loadWeight=" << info.loadWeight
                  << ", usageWeight=" << info.usageWeight);
  }
}

void
NameLsa::print(std::ostream& os) const
{
  os << "      Names:\n";
  int i = 0;
  for (const auto& name : m_npl.getPrefixInfo()) {
    os << "        Name " << i << ": " << name.getName()
       << " | Cost: " << name.getCost() << "\n";
    i++;
  }
}

std::tuple<bool, std::list<PrefixInfo>, std::list<PrefixInfo>>
NameLsa::update(const std::shared_ptr<Lsa>& lsa)
{
  auto nlsa = std::static_pointer_cast<NameLsa>(lsa);
  bool updated = false;

  std::list<ndn::Name> newNames = nlsa->getNpl().getNames();
  std::list<ndn::Name> oldNames = m_npl.getNames();
  std::list<ndn::Name> nameRefToAdd;
  std::list<PrefixInfo> namesToAdd;

  std::set_difference(newNames.begin(), newNames.end(), oldNames.begin(), oldNames.end(),
                      std::inserter(nameRefToAdd, nameRefToAdd.begin()));
  for (const auto& name : nameRefToAdd) {
    namesToAdd.push_back(nlsa->getNpl().getPrefixInfoForName(name));
    addName(nlsa->getNpl().getPrefixInfoForName(name));
    updated = true;
  }

  std::list<ndn::Name> nameRefToRemove;
  std::list<PrefixInfo> namesToRemove;
  std::set_difference(oldNames.begin(), oldNames.end(), newNames.begin(), newNames.end(),
                      std::inserter(nameRefToRemove, nameRefToRemove.begin()));
  for (const auto& name : nameRefToRemove) {
    namesToRemove.push_back(m_npl.getPrefixInfoForName(name));
    removeName(m_npl.getPrefixInfoForName(name));
    updated = true;
  }

  // Check for Service Function information changes
  NLSR_LOG_DEBUG("NameLsa::update: Existing m_serviceFunctionInfo size: " << m_serviceFunctionInfo.size()
                << ", New nlsa->m_serviceFunctionInfo size: " << nlsa->m_serviceFunctionInfo.size());
  
  for (const auto& [serviceName, newSfInfo] : nlsa->m_serviceFunctionInfo) {
    NLSR_LOG_DEBUG("NameLsa::update: Processing Service Function info for " << serviceName.toUri()
                  << ": processingTime=" << newSfInfo.processingTime
                  << ", processingWeight=" << newSfInfo.processingWeight);
    auto it = m_serviceFunctionInfo.find(serviceName);
    if (it == m_serviceFunctionInfo.end()) {
      // New Service Function information
      m_serviceFunctionInfo[serviceName] = newSfInfo;
      updated = true;
      NLSR_LOG_DEBUG("Service Function info added for " << serviceName.toUri());
    } else {
      // Check if Service Function information has changed
      const auto& oldSfInfo = it->second;
      if (oldSfInfo.processingTime != newSfInfo.processingTime ||
          oldSfInfo.load != newSfInfo.load ||
          oldSfInfo.usageCount != newSfInfo.usageCount ||
          oldSfInfo.processingWeight != newSfInfo.processingWeight ||
          oldSfInfo.loadWeight != newSfInfo.loadWeight ||
          oldSfInfo.usageWeight != newSfInfo.usageWeight) {
        m_serviceFunctionInfo[serviceName] = newSfInfo;
        updated = true;
        NLSR_LOG_DEBUG("Service Function info updated for " << serviceName.toUri()
                      << ": processingTime=" << newSfInfo.processingTime
                      << ", load=" << newSfInfo.load
                      << ", usageCount=" << newSfInfo.usageCount
                      << ", processingWeight=" << newSfInfo.processingWeight);
      }
    }
  }

  // Check for removed Service Function information
  // Only remove if the new NameLSA explicitly does not contain the Service Function info
  // If the new NameLSA's m_serviceFunctionInfo is empty, we should preserve existing info
  // (This can happen if wireDecode failed to decode Service Function info, but we don't want to lose existing info)
  if (!nlsa->m_serviceFunctionInfo.empty()) {
    for (auto it = m_serviceFunctionInfo.begin(); it != m_serviceFunctionInfo.end();) {
      if (nlsa->m_serviceFunctionInfo.find(it->first) == nlsa->m_serviceFunctionInfo.end()) {
        NLSR_LOG_DEBUG("Service Function info removed for " << it->first.toUri());
        it = m_serviceFunctionInfo.erase(it);
        updated = true;
      } else {
        ++it;
      }
    }
  } else {
    NLSR_LOG_DEBUG("NameLsa::update: New NameLSA has empty m_serviceFunctionInfo, preserving existing info");
  }

  NLSR_LOG_DEBUG("NameLsa::update: Final m_serviceFunctionInfo size: " << m_serviceFunctionInfo.size());
  return {updated, namesToAdd, namesToRemove};
}

size_t
NameLsa::getServiceFunctionInfoMapSize() const
{
  return m_serviceFunctionInfo.size();
}

const std::map<ndn::Name, ServiceFunctionInfo>&
NameLsa::getAllServiceFunctionInfo() const
{
  return m_serviceFunctionInfo;
}

} // namespace nlsr
