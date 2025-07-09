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

#ifndef NLSR_PUBLISHER_SIDECAR_STATS_HANDLER_HPP
#define NLSR_PUBLISHER_SIDECAR_STATS_HANDLER_HPP

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/mgmt/dispatcher.hpp>
#include <boost/noncopyable.hpp>
#include <string>
#include <vector>
#include <map>

namespace nlsr {

namespace dataset {
inline const ndn::PartialName SIDECAR_STATS_DATASET{"sidecar-stats"};
inline const ndn::PartialName SIDECAR_SERVICE_STATS_DATASET{"service-stats"};
inline const ndn::PartialName SIDECAR_SFC_STATS_DATASET{"sfc-stats"};
inline const ndn::PartialName FUNCTION_INFO_DATASET{"function-info"};
} // namespace dataset

/*!
   \brief Class to publish sidecar statistics dataset
   \sa https://redmine.named-data.net/projects/nlsr/wiki/Sidecar_Stats_DataSet
 */
class SidecarStatsHandler : boost::noncopyable
{
public:
  class Error : std::runtime_error
  {
  public:
    using std::runtime_error::runtime_error;
  };

  SidecarStatsHandler(ndn::mgmt::Dispatcher& dispatcher,
                      const std::string& logPath = "/var/log/sidecar/service.log");

public:
  /*! \brief Get current sidecar statistics for external access
  */
  std::map<std::string, std::string>
  getCurrentStats() const;

private:
  /*! \brief provide sidecar statistics dataset
  */
  void
  publishSidecarStats(const ndn::Name& topPrefix, const ndn::Interest& interest,
                      ndn::mgmt::StatusDatasetContext& context);

  /*! \brief provide service statistics dataset
  */
  void
  publishServiceStats(const ndn::Name& topPrefix, const ndn::Interest& interest,
                      ndn::mgmt::StatusDatasetContext& context);

  /*! \brief provide SFC statistics dataset
  */
  void
  publishSfcStats(const ndn::Name& topPrefix, const ndn::Interest& interest,
                  ndn::mgmt::StatusDatasetContext& context);

  /*! \brief provide function information dataset
  */
  void
  publishFunctionInfo(const ndn::Name& topPrefix, const ndn::Interest& interest,
                      ndn::mgmt::StatusDatasetContext& context);

  /*! \brief parse sidecar log file
  */
  std::vector<std::map<std::string, std::string>>
  parseSidecarLog() const;

  /*! \brief get latest statistics
  */
  std::map<std::string, std::string>
  getLatestStats() const;

private:
  std::string m_logPath;
};

} // namespace nlsr

#endif // NLSR_PUBLISHER_SIDECAR_STATS_HANDLER_HPP 