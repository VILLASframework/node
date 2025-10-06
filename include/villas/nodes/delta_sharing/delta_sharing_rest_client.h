/* Node type: Delta Share.
 *
 * Author: Ritesh Karki <ritesh.karki@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <iostream>
#include <villas/nodes/delta_sharing/jansson_wrapper.h>
#include <list>
#include <villas/nodes/delta_sharing/Protocol.h>
#include <restclient-cpp/restclient.h>
#include <restclient-cpp/connection.h>


namespace DeltaSharing
{
    using json = JanssonWrapper::json;
    struct DeltaSharingRestClient
    {
    public:
        DeltaSharingRestClient(std::string filename);
        ~DeltaSharingRestClient();
        const std::shared_ptr<std::vector<DeltaSharingProtocol::Share>> ListShares(int maxResult, std::string pageToken) const;
        const std::shared_ptr<std::vector<DeltaSharingProtocol::Schema>> ListSchemas(const DeltaSharingProtocol::Share &share, int maxResult, std::string pageToken) const;
        const std::shared_ptr<std::vector<DeltaSharingProtocol::Table>> ListTables(const DeltaSharingProtocol::Schema &schema, int maxResult, std::string pageToken) const;
        const std::shared_ptr<std::vector<DeltaSharingProtocol::Table>> ListAllTables(const DeltaSharingProtocol::Share &share, int maxResult, std::string pageToken) const;
        const std::shared_ptr<std::vector<DeltaSharingProtocol::File>> ListFilesInTable(const DeltaSharingProtocol::Table) const;
        const DeltaSharingProtocol::Metadata QueryTableMetadata(const DeltaSharingProtocol::Table &table) const;
        const DeltaSharingProtocol::DeltaSharingProfile &GetProfile() const;
        RestClient::Response get(std::string url);
        void PopulateCache(std::string url, std::string cacheLocation);
        const bool shouldRetry(RestClient::Response &response) const;

    protected:
        json ReadFromFile(std::string filename);

    private:
        DeltaSharingProtocol::DeltaSharingProfile profile;
        static const std::string user_agent;
    };
};
