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

#include "name-prefix-table.hpp"

#include "logger.hpp"
#include "nlsr.hpp"
#include "routing-table.hpp"
#include "lsa/name-lsa.hpp"
#include "conf-parameter.hpp"

#include <algorithm>
#include <list>
#include <utility>
#include <map>

namespace nlsr {

INIT_LOGGER(route.NamePrefixTable);

NamePrefixTable::NamePrefixTable(const ndn::Name& ownRouterName, Fib& fib,
                                 RoutingTable& routingTable,
                                 AfterRoutingChange& afterRoutingChangeSignal,
                                 Lsdb::AfterLsdbModified& afterLsdbModifiedSignal,
                                 Lsdb& lsdb, ConfParameter& confParam)
  : m_ownRouterName(ownRouterName)
  , m_fib(fib)
  , m_routingTable(routingTable)
  , m_lsdb(lsdb)
  , m_confParam(confParam)
{
  m_afterRoutingChangeConnection = afterRoutingChangeSignal.connect(
    [this] (const std::list<RoutingTableEntry>& entries) {
      updateWithNewRoute(entries);
    });

  m_afterLsdbModified = afterLsdbModifiedSignal.connect(
    [this] (std::shared_ptr<Lsa> lsa, LsdbUpdate updateType,
            const auto& namesToAdd, const auto& namesToRemove) {
      updateFromLsdb(lsa, updateType, namesToAdd, namesToRemove);
    }
  );
}

NamePrefixTable::~NamePrefixTable()
{
  m_afterRoutingChangeConnection.disconnect();
  m_afterLsdbModified.disconnect();
}

void
NamePrefixTable::updateFromLsdb(std::shared_ptr<Lsa> lsa, LsdbUpdate updateType,
                                const std::list<nlsr::PrefixInfo>& namesToAdd,
                                const std::list<nlsr::PrefixInfo>& namesToRemove)
{
  if (m_ownRouterName == lsa->getOriginRouter()) {
    NLSR_LOG_DEBUG("updateFromLsdb: Skipping own router's LSA");
    return;
  }
  NLSR_LOG_DEBUG("updateFromLsdb called: router=" << lsa->getOriginRouter() 
                << ", type=" << static_cast<int>(lsa->getType())
                << ", updateType=" << static_cast<int>(updateType)
                << ", namesToAdd=" << namesToAdd.size()
                << ", namesToRemove=" << namesToRemove.size());
  NLSR_LOG_TRACE("Got update from Lsdb for router: " << lsa->getOriginRouter());

  if (updateType == LsdbUpdate::INSTALLED) {
    NLSR_LOG_DEBUG("updateFromLsdb: LSA INSTALLED, adding router entry");
    addEntry(lsa->getOriginRouter(), lsa->getOriginRouter());

    if (lsa->getType() == Lsa::Type::NAME) {
      auto nlsa = std::static_pointer_cast<NameLsa>(lsa);
      NLSR_LOG_DEBUG("updateFromLsdb: Processing NAME LSA with " << nlsa->getNpl().getPrefixInfo().size() << " prefixes");
      for (const auto &prefix : nlsa->getNpl().getPrefixInfo()) {
        if (prefix.getName() != m_ownRouterName) {
          m_nexthopCost[DestNameKey(lsa->getOriginRouter(), prefix.getName())] = prefix.getCost();
          addEntry(prefix.getName(), lsa->getOriginRouter());
        }
      }
    }
  }
  else if (updateType == LsdbUpdate::UPDATED) {
    if (lsa->getType() != Lsa::Type::NAME) {
      NLSR_LOG_DEBUG("updateFromLsdb: LSA UPDATED but not NAME type, skipping");
      return;
    }

    // Get the actual updated NameLSA from LSDB (not from lsa parameter)
    // The lsa parameter may be the new LSA instance, but we need the updated one from LSDB
    auto nlsa = m_lsdb.findLsa<NameLsa>(lsa->getOriginRouter());
    if (!nlsa) {
      NLSR_LOG_DEBUG("updateFromLsdb: NameLSA not found in LSDB for " << lsa->getOriginRouter());
      return;
    }
    
    NLSR_LOG_DEBUG("updateFromLsdb: NAME LSA UPDATED, checking for Service Function info changes");
    NLSR_LOG_DEBUG("updateFromLsdb: NameLSA from LSDB has m_serviceFunctionInfo size: " 
                  << nlsa->getServiceFunctionInfoMapSize());
    
    // Check if Service Function information exists in the updated NameLSA
    // If Service Function info exists, we need to update the FIB for existing entries
    // Note: We use the actual NameLSA from LSDB, which has been updated by the update() method
    const auto& allSfInfo = nlsa->getAllServiceFunctionInfo();
    bool hasSfInfo = false;
    for (const auto& [serviceName, sfInfo] : allSfInfo) {
      if (sfInfo.processingTime > 0.0 || sfInfo.load > 0.0 || sfInfo.usageCount > 0) {
        hasSfInfo = true;
        NLSR_LOG_DEBUG("updateFromLsdb: Service Function info found for " << serviceName.toUri()
                      << ": processingTime=" << sfInfo.processingTime
                      << ", load=" << sfInfo.load
                      << ", usageCount=" << sfInfo.usageCount);
      }
    }
    
    // If Service Function info exists, update existing entries to recalculate FunctionCost
    // This ensures that FunctionCost is recalculated whenever NameLSA is updated with Service Function info
    if (hasSfInfo || !allSfInfo.empty()) {
      NLSR_LOG_DEBUG("updateFromLsdb: Service Function info present, updating existing entries for router=" << lsa->getOriginRouter());
      // Find all entries for this router and update them
      for (auto& entry : m_table) {
        auto rtpeList = entry->getRteList();
        for (const auto& rtpe : rtpeList) {
          if (rtpe->getDestination() == lsa->getOriginRouter()) {
            NLSR_LOG_DEBUG("updateFromLsdb: Updating entry for prefix=" << entry->getNamePrefix()
                         << ", router=" << lsa->getOriginRouter());
            entry->generateNhlfromRteList();
            if (entry->getNexthopList().size() > 0) {
              m_fib.update(entry->getNamePrefix(), 
                          adjustNexthopCosts(entry->getNexthopList(), entry->getNamePrefix(), *entry));
            }
          }
        }
      }
    }

    for (const auto &prefix : namesToAdd) {
      if (prefix.getName() != m_ownRouterName) {
        m_nexthopCost[DestNameKey(lsa->getOriginRouter(), prefix.getName())] = prefix.getCost();
        addEntry(prefix.getName(), lsa->getOriginRouter());
      }
    }

    for (const auto &prefix : namesToRemove) {
      if (prefix.getName() != m_ownRouterName) {
        m_nexthopCost.erase(m_nexthopCost.find(DestNameKey(lsa->getOriginRouter(), prefix.getName())));
        removeEntry(prefix.getName(), lsa->getOriginRouter());
      }
    }
  }
  else {
    removeEntry(lsa->getOriginRouter(), lsa->getOriginRouter());
    if (lsa->getType() == Lsa::Type::NAME) {
      auto nlsa = std::static_pointer_cast<NameLsa>(lsa);
      for (const auto& name : nlsa->getNpl().getNames()) {
        if (name != m_ownRouterName) {
          m_nexthopCost.erase(m_nexthopCost.find(DestNameKey(lsa->getOriginRouter(), name)));
          removeEntry(name, lsa->getOriginRouter());
        }
      }
    }
  }
}

NexthopList
NamePrefixTable::adjustNexthopCosts(const NexthopList& nhlist, const ndn::Name& nameToCheck, const NamePrefixTableEntry& npte)
{
  // サービスファンクションかどうかを判定
  // NameLSAからPrefixInfoを取得して、isServiceFunctionフラグを確認
  bool isServiceFunction = false;
  
  for (const auto& rtpe : npte.getRteList()) {
    const ndn::Name& destRouterName = rtpe->getDestination();
    auto nameLsa = m_lsdb.findLsa<NameLsa>(destRouterName);
    if (nameLsa) {
      const auto& prefixInfoList = nameLsa->getNpl().getPrefixInfo();
      for (const auto& prefixInfo : prefixInfoList) {
        if (prefixInfo.getName() == nameToCheck) {
          isServiceFunction = prefixInfo.isServiceFunction();
          NLSR_LOG_DEBUG("Found prefix " << nameToCheck << " in NameLSA from " << destRouterName
                        << ", isServiceFunction=" << isServiceFunction);
          break;
        }
      }
      if (isServiceFunction) {
        break;
      }
    }
  }
  
  if (!isServiceFunction) {
    NLSR_LOG_DEBUG("adjustNexthopCosts: " << nameToCheck << " is not a service function, returning original NextHopList");
    return nhlist;
  }
  
  NLSR_LOG_DEBUG("adjustNexthopCosts called: nameToCheck=" << nameToCheck << " (service function)");
  
  // 同じFaceUriでも異なるdestRouterNameのNextHopを区別するため、
  // FaceUriとdestRouterNameのペアをキーとして使用
  // (FaceUri, destRouterName) -> (NextHop, functionCost)
  std::map<std::pair<ndn::FaceUri, ndn::Name>, std::pair<NextHop, double>> nextHopMap;
  
  // 各RoutingTablePoolEntryに対して個別にFunctionCostを計算
  // これにより、各NextHopがどのdestRouterNameに対応するかを正確に判断できる
  for (const auto& rtpe : npte.getRteList()) {
    const ndn::Name& destRouterName = rtpe->getDestination();
    NLSR_LOG_DEBUG("Processing RoutingTablePoolEntry for destRouterName=" << destRouterName);
    
    // destRouterNameのNameLSAからFunctionCostを計算
    double functionCost = 0.0;
    auto nameLsa = m_lsdb.findLsa<NameLsa>(destRouterName);
    if (nameLsa) {
      NLSR_LOG_DEBUG("NameLSA found for " << destRouterName);
      NLSR_LOG_DEBUG("NameLSA from LSDB has m_serviceFunctionInfo size: " 
                    << nameLsa->getServiceFunctionInfoMapSize());
      
      // Log all Service Function info in the NameLSA for debugging
      const auto& allSfInfo = nameLsa->getAllServiceFunctionInfo();
      for (const auto& [sfName, sfInfo] : allSfInfo) {
        NLSR_LOG_DEBUG("NameLSA contains Service Function: " << sfName.toUri()
                      << " -> processingTime=" << sfInfo.processingTime
                      << ", processingWeight=" << sfInfo.processingWeight);
      }
      
      ServiceFunctionInfo sfInfo = nameLsa->getServiceFunctionInfo(nameToCheck);
      
      NLSR_LOG_DEBUG("ServiceFunctionInfo for " << nameToCheck << " (destRouterName=" << destRouterName 
                    << "): processingTime=" << sfInfo.processingTime
                    << ", load=" << sfInfo.load << ", usageCount=" << sfInfo.usageCount
                    << ", processingWeight=" << sfInfo.processingWeight
                    << ", loadWeight=" << sfInfo.loadWeight
                    << ", usageWeight=" << sfInfo.usageWeight);
      
      // Check if Service Function info is stale (lastUpdateTime is too old)
      // If stale, set functionCost to 0
      bool isStale = false;
      if (sfInfo.lastUpdateTime != ndn::time::system_clock::time_point::min()) {
        auto now = ndn::time::system_clock::now();
        auto timeSinceLastUpdate = boost::chrono::duration_cast<boost::chrono::seconds>(now - sfInfo.lastUpdateTime).count();
        uint32_t staleThreshold = m_confParam.getUtilizationWindowSeconds() * 2;  // 2x window duration
        
        if (timeSinceLastUpdate > static_cast<int64_t>(staleThreshold)) {
          isStale = true;
          NLSR_LOG_DEBUG("Service Function info is stale (lastUpdateTime: " 
                        << boost::chrono::duration_cast<boost::chrono::seconds>(sfInfo.lastUpdateTime.time_since_epoch()).count()
                        << ", timeSinceLastUpdate: " << timeSinceLastUpdate << "s, threshold: " << staleThreshold << "s)");
        }
      }
      
      // Calculate function cost if Service Function info is available and not stale
      // NameLSAにService Function情報が存在する場合、そのプレフィックスはサービスファンクションと判断
      // weight情報は、サービスファンクションを持つノード（destRouterName）の設定ファイルから取得
      // NameLSAに含まれるweight情報を使用する
      if (!isStale && (sfInfo.processingTime > 0.0 || sfInfo.load > 0.0 || sfInfo.usageCount > 0)) {
        NLSR_LOG_DEBUG("Service Function info found in NameLSA for " << nameToCheck 
                      << " (destRouterName=" << destRouterName << "), calculating FunctionCost");
        
        // NameLSAから取得したweight情報を使用（サービスファンクションを持つノードの設定ファイルの値）
        double processingWeight = sfInfo.processingWeight;
        double loadWeight = sfInfo.loadWeight;
        double usageWeight = sfInfo.usageWeight;
        
        NLSR_LOG_DEBUG("Weights from NameLSA (node " << destRouterName << "): processing=" << processingWeight 
                      << ", load=" << loadWeight << ", usage=" << usageWeight);
        
        functionCost = sfInfo.processingTime * processingWeight +
                      sfInfo.load * loadWeight +
                      (sfInfo.usageCount / 100.0) * usageWeight;
        
        NLSR_LOG_DEBUG("FunctionCost calculated for " << nameToCheck << " prefix to " << destRouterName 
                      << ": processingTime=" << sfInfo.processingTime 
                      << ", functionCost=" << functionCost);
      } else if (isStale) {
        NLSR_LOG_DEBUG("Service Function info is stale for " << nameToCheck 
                      << " (destRouterName=" << destRouterName << "), functionCost=0");
        functionCost = 0.0;
      } else {
        NLSR_LOG_DEBUG("No Service Function info in NameLSA for " << nameToCheck 
                      << " (destRouterName=" << destRouterName << ", all values are zero)");
      }
    } else {
      NLSR_LOG_DEBUG("NameLSA not found for " << destRouterName);
    }
    
    // このRoutingTablePoolEntryのNextHopに対してFunctionCostを適用
    for (const auto& nh : rtpe->getNexthopList().getNextHops()) {
      double originalCost = nh.getRouteCost();
      double nexthopCost = m_nexthopCost[DestNameKey(destRouterName, nameToCheck)];
      double adjustedCost = originalCost + nexthopCost + functionCost;
      
      NLSR_LOG_DEBUG("Adjusting cost for " << nameToCheck << " to " << destRouterName
                    << ": originalCost=" << originalCost 
                    << ", nexthopCost=" << nexthopCost
                    << ", functionCost=" << functionCost
                    << ", adjustedCost=" << adjustedCost);
      
      const NextHop newNextHop = NextHop(nh.getConnectingFaceUri(), adjustedCost);
      const ndn::FaceUri& faceUri = nh.getConnectingFaceUri();
      
      // (FaceUri, destRouterName)のペアをキーとして使用
      auto key = std::make_pair(faceUri, destRouterName);
      auto it = nextHopMap.find(key);
      
      if (it == nextHopMap.end()) {
        // 新しいNextHopを追加（同じFaceUriでも異なるdestRouterNameなら別エントリとして保持）
        nextHopMap[key] = std::make_pair(newNextHop, functionCost);
        NLSR_LOG_DEBUG("Added new NextHop: " << faceUri << " via " << destRouterName 
                      << " with adjustedCost=" << adjustedCost 
                      << ", functionCost=" << functionCost);
      } else {
        // 同じ(FaceUri, destRouterName)のペアが既に存在する場合、更新する
        // これは、同じdestRouterNameからのNextHopが更新された場合を意味する
        double existingFunctionCost = it->second.second;
        double existingCost = it->second.first.getRouteCost();
        
        NLSR_LOG_DEBUG("Updating existing NextHop: " << faceUri << " via " << destRouterName 
                      << ", existing: cost=" << existingCost << ", functionCost=" << existingFunctionCost
                      << ", new: cost=" << adjustedCost << ", functionCost=" << functionCost);
        
        // 同じdestRouterNameからのNextHopなので、常に最新の値で更新
        nextHopMap[key] = std::make_pair(newNextHop, functionCost);
        NLSR_LOG_DEBUG("Updated NextHop: " << faceUri << " via " << destRouterName 
                      << " with adjustedCost=" << adjustedCost);
      }
    }
  }
  
  // マップからNexthopListを構築
  // 同じFaceUriでも異なるdestRouterNameのNextHopは両方とも保持される
  NexthopList new_nhList;
  for (const auto& [key, pair] : nextHopMap) {
    const auto& [faceUri, destRouterName] = key;
    new_nhList.addNextHop(pair.first);
    NLSR_LOG_DEBUG("Final NextHop added to list: " << faceUri 
                  << " via " << destRouterName
                  << " with cost=" << pair.first.getRouteCost() 
                  << ", functionCost=" << pair.second);
  }
  
  NLSR_LOG_DEBUG("adjustNexthopCosts returning NexthopList with " << new_nhList.size() << " entries");
  
  return new_nhList;
}

void
NamePrefixTable::addEntry(const ndn::Name& name, const ndn::Name& destRouter)
{
  NLSR_LOG_DEBUG("NamePrefixTable::addEntry called: name=" << name << ", destRouter=" << destRouter);
  // Check if the advertised name prefix is in the table already.
  auto nameItr = std::find_if(m_table.begin(), m_table.end(),
                              [&] (const auto& entry) { return name == entry->getNamePrefix(); });

  // Attempt to find a routing table pool entry (RTPE) we can use.
  auto rtpeItr = m_rtpool.find(destRouter);

  // These declarations just to make the compiler happy...
  RoutingTablePoolEntry rtpe;
  std::shared_ptr<RoutingTablePoolEntry> rtpePtr(nullptr);

  // There isn't currently a routing table entry in the pool for this name
  if (rtpeItr == m_rtpool.end()) {
    // See if there is a routing table entry available we could use
    RoutingTableEntry* routeEntryPtr = m_routingTable.findRoutingTableEntry(destRouter);

    // We have to create a new routing table entry
    if (routeEntryPtr == nullptr) {
      rtpe = RoutingTablePoolEntry(destRouter, 0);
    }
    // There was already a usable one in the routing table
    else {
      rtpe = RoutingTablePoolEntry(*routeEntryPtr, 0);
    }

    // Add the new pool object to the pool.
    rtpePtr = addRtpeToPool(rtpe);
  }
  // There was one already, so just fetch that one.
  else {
    rtpePtr = (*rtpeItr).second;
  }

  std::shared_ptr<NamePrefixTableEntry> npte;
  // Either we have to make a new NPT entry or there already was one.
  if (nameItr == m_table.end()) {
    NLSR_LOG_DEBUG("Adding origin: " << rtpePtr->getDestination()
                   << " to a new name prefix: " << name);
    npte = std::make_shared<NamePrefixTableEntry>(name);
    npte->addRoutingTableEntry(rtpePtr);
    npte->generateNhlfromRteList();
    m_table.push_back(npte);

    // If this entry has next hops, we need to inform the FIB
    if (npte->getNexthopList().size() > 0) {
      NLSR_LOG_TRACE("Updating FIB with next hops for " << npte->getNamePrefix());
      m_fib.update(name, adjustNexthopCosts(npte->getNexthopList(), name, *npte));
    }
    // The routing table may recalculate and add a routing table entry
    // with no next hops to replace an existing routing table entry. In
    // this case, the name prefix is no longer reachable through a next
    // hop and should be removed from the FIB. But, the prefix should
    // remain in the Name Prefix Table as a future routing table
    // calculation may add next hops.
    else {
      NLSR_LOG_TRACE(npte->getNamePrefix() << " has no next hops; removing from FIB");
      m_fib.remove(name);
    }
  }
  else {
    npte = *nameItr;
    NLSR_LOG_TRACE("Adding origin: " << rtpePtr->getDestination() <<
                   " to existing prefix: " << **nameItr);
    (*nameItr)->addRoutingTableEntry(rtpePtr);
    (*nameItr)->generateNhlfromRteList();

    if ((*nameItr)->getNexthopList().size() > 0) {
      NLSR_LOG_TRACE("Updating FIB with next hops for " << (**nameItr));
      m_fib.update(name, adjustNexthopCosts((*nameItr)->getNexthopList(), name, **nameItr));
    }
    else {
      NLSR_LOG_TRACE(npte->getNamePrefix() << " has no next hops; removing from FIB");
      m_fib.remove(name);
    }
  }

  // Add the reference to this NPT to the RTPE.
  rtpePtr->namePrefixTableEntries.try_emplace(npte->getNamePrefix(),
                                              std::weak_ptr<NamePrefixTableEntry>(npte));
}

void
NamePrefixTable::removeEntry(const ndn::Name& name, const ndn::Name& destRouter)
{
  NLSR_LOG_DEBUG("Removing origin: " << destRouter << " from " << name);

  // Fetch an iterator to the appropriate pair object in the pool.
  auto rtpeItr = m_rtpool.find(destRouter);

  // Simple error checking to prevent any unusual behavior in the case
  // that we try to remove an entry that isn't there.
  if (rtpeItr == m_rtpool.end()) {
    NLSR_LOG_DEBUG("No entry for origin: " << destRouter
                   << " found, so it cannot be removed from prefix: " << name);
    return;
  }
  std::shared_ptr<RoutingTablePoolEntry> rtpePtr = rtpeItr->second;

  // Ensure that the entry exists
  auto nameItr = std::find_if(m_table.begin(), m_table.end(),
                              [&] (const auto& entry) { return entry->getNamePrefix() == name; });
  if (nameItr != m_table.end()) {
    NLSR_LOG_TRACE("Removing origin: " << rtpePtr->getDestination()
                   << " from prefix: " << **nameItr);

    // Rather than iterating through the whole list periodically, just
    // delete them here if they have no references.
    if ((*nameItr)->removeRoutingTableEntry(rtpePtr) == 0) {
      deleteRtpeFromPool(rtpePtr);
    }

    // If the prefix is a router prefix and it does not have any other
    // routing table entries, the Adjacency/Coordinate LSA associated
    // with that origin router has been removed from the LSDB and so
    // the router prefix should be removed from the Name Prefix Table.
    //
    // If the prefix is an advertised name prefix: If another router
    //   advertises this name prefix, the RteList should have another
    //   entry for that router; the next hops should be recalculated
    //   and installed in the FIB.
    //
    //   If no other router advertises this name prefix, the RteList
    //   should be empty and the prefix can be removed from the Name
    //   Prefix Table. Once a new Name LSA advertises this prefix, a
    //   new entry for the prefix will be created.
    //
    if ((*nameItr)->getRteListSize() == 0) {
      NLSR_LOG_TRACE(**nameItr << " has no routing table entries;"
                     << " removing from table and FIB");
      m_table.erase(nameItr);
      m_fib.remove(name);
    }
    else {
      NLSR_LOG_TRACE(**nameItr << " has other routing table entries;"
                     << " updating FIB with next hops");
      (*nameItr)->generateNhlfromRteList();
      m_fib.update(name, adjustNexthopCosts((*nameItr)->getNexthopList(), name, **nameItr));
    }
  }
  else {
    NLSR_LOG_DEBUG("Attempted to remove origin: " << rtpePtr->getDestination()
                   << " from non-existent prefix: " << name);
  }
}

void
NamePrefixTable::updateWithNewRoute(const std::list<RoutingTableEntry>& entries)
{
  NLSR_LOG_DEBUG("Updating table with newly calculated routes");

  // Iterate over each pool entry we have
  for (auto&& poolEntryPair : m_rtpool) {
    auto&& poolEntry = poolEntryPair.second;
    auto sourceEntry = std::find_if(entries.begin(), entries.end(),
                                    [&poolEntry] (const RoutingTableEntry& entry) {
                                      return poolEntry->getDestination() == entry.getDestination();
                                    });
    // If this pool entry has a corresponding entry in the routing table now
    if (sourceEntry != entries.end()
        && poolEntry->getNexthopList() != sourceEntry->getNexthopList()) {
      NLSR_LOG_DEBUG("Routing entry: " << poolEntry->getDestination() << " has changed next-hops.");
      poolEntry->setNexthopList(sourceEntry->getNexthopList());
      for (const auto& nameEntry : poolEntry->namePrefixTableEntries) {
        auto nameEntryFullPtr = nameEntry.second.lock();
        addEntry(nameEntryFullPtr->getNamePrefix(), poolEntry->getDestination());
      }
    }
    else if (sourceEntry == entries.end()) {
      NLSR_LOG_DEBUG("Routing entry: " << poolEntry->getDestination() << " now has no next-hops.");
      poolEntry->getNexthopList().clear();
      for (const auto& nameEntry : poolEntry->namePrefixTableEntries) {
        auto nameEntryFullPtr = nameEntry.second.lock();
        addEntry(nameEntryFullPtr->getNamePrefix(), poolEntry->getDestination());
      }
    }
    else {
      NLSR_LOG_TRACE("No change in routing entry:" << poolEntry->getDestination()
                 << ", no action necessary.");
    }
  }
}

// Inserts the routing table pool entry into the NPT's RTE storage
// pool.  This cannot fail, so the pool is guaranteed to contain the
// item after this occurs.
std::shared_ptr<RoutingTablePoolEntry>
NamePrefixTable::addRtpeToPool(RoutingTablePoolEntry& rtpe)
{
  auto poolIt = m_rtpool.try_emplace(rtpe.getDestination(),
                                     std::make_shared<RoutingTablePoolEntry>(rtpe)).first;
  return poolIt->second;
}

// Removes the routing table pool entry from the storage pool. The
// postconditions of this function are guaranteed to include that
// the storage pool does not contain such an item. Additionally,
// this function cannot fail, but nonetheless debug information is
// given in the case that this function is called with an entry that
// isn't in the pool.
void
NamePrefixTable::deleteRtpeFromPool(std::shared_ptr<RoutingTablePoolEntry> rtpePtr)
{
  if (m_rtpool.erase(rtpePtr->getDestination()) != 1) {
    NLSR_LOG_DEBUG("Attempted to delete non-existent origin: "
                   << rtpePtr->getDestination()
                   << " from NPT routing table entry storage pool.");
  }
}

void
NamePrefixTable::writeLog()
{
  NLSR_LOG_DEBUG(*this);
}

std::ostream&
operator<<(std::ostream& os, const NamePrefixTable& table)
{
  os << "----------------NPT----------------------\n";

  for (const auto& entryPtr : table) {
    os << *entryPtr << std::endl;
  }

  return os;
}

} // namespace nlsr
