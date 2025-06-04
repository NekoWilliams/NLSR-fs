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
#include "tlv-nlsr.hpp"
#include <ndn-cxx/encoding/block-helpers.hpp>

namespace nlsr {

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
  auto it = m_serviceFunctionInfo.find(name);
  return (it != m_serviceFunctionInfo.end()) ? it->second : ServiceFunctionInfo{};
}

namespace {

using namespace ndn::encoding;

template<ndn::encoding::Tag TAG>
size_t
nlsrPrependDoubleBlock(ndn::encoding::EncodingImpl<TAG>& encoder, uint32_t type, double value)
{
  size_t totalLength = encoder.prependByteArray(
    reinterpret_cast<const uint8_t*>(&value), sizeof(double));
  totalLength += encoder.prependVarNumber(sizeof(double));
  totalLength += encoder.prependVarNumber(type);
  return totalLength;
}

double
extractDoubleFromBlock(const ndn::Block& block)
{
  double value = 0.0;
  const uint8_t* content = block.value();
  memcpy(&value, content, sizeof(double));
  return value;
}

} // anonymous namespace

template<ndn::encoding::Tag TAG>
size_t
NameLsa::wireEncode(ndn::EncodingImpl<TAG>& block) const
{
  size_t totalLength = 0;

  // エンコードService Function情報
  for (const auto& [name, info] : m_serviceFunctionInfo) {
    size_t sfInfoLength = 0;
    
    // Service Function情報をエンコード
    sfInfoLength += nlsrPrependDoubleBlock<TAG>(block, nlsr::tlv::ProcessingTime, info.processingTime);
    sfInfoLength += nlsrPrependDoubleBlock<TAG>(block, nlsr::tlv::Load, info.load);
    sfInfoLength += block.prependVarNumber(info.usageCount);
    sfInfoLength += block.prependVarNumber(nlsr::tlv::UsageCount);
    
    // 名前をエンコード
    sfInfoLength += name.wireEncode(block);
    
    // Service Function情報全体の長さを追加
    totalLength += sfInfoLength;
    totalLength += block.prependVarNumber(sfInfoLength);
    totalLength += block.prependVarNumber(nlsr::tlv::ServiceFunction);
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
  for (; val != m_wire.elements_end(); ++val) {
    if (val->type() == nlsr::tlv::PrefixInfo) {
      //TODO: Implement this structure as a type instead and add decoding
      npl.insert(PrefixInfo(*val));
    }
    else {
      NDN_THROW(Error("Name", val->type()));
    }
  }
  m_npl = npl;
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

  // Obtain the set difference of the current and the incoming
  // name prefix sets, and add those.

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

  // Also remove any names that are no longer being advertised.
  std::list<ndn::Name> nameRefToRemove;
  std::list<PrefixInfo> namesToRemove;
  std::set_difference(oldNames.begin(), oldNames.end(), newNames.begin(), newNames.end(),
                      std::inserter(nameRefToRemove, nameRefToRemove.begin()));
  for (const auto& name : nameRefToRemove) {
    namesToRemove.push_back(m_npl.getPrefixInfoForName(name));
    removeName(m_npl.getPrefixInfoForName(name));

    updated = true;
  }
  return {updated, namesToAdd, namesToRemove};
}

} // namespace nlsr
