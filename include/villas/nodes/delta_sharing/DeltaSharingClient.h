
#ifndef __DELTASHARINGCLIENT_H__
#define __DELTASHARINGCLIENT_H__

#include <iostream>
#include "DeltaSharingRestClient.h"
#include <arrow/table.h>

namespace DeltaSharing
{
    
    struct DeltaSharingClient 
    {
    public:
        DeltaSharingClient(std::string filename, boost::optional<std::string> cacheLocation);
        std::shared_ptr<arrow::Table> LoadAsArrowTable(std::string &url);
        std::shared_ptr<arrow::Table> ReadTableFromCache(std::string &url);
        const std::shared_ptr<std::vector<DeltaSharingProtocol::Share>> ListShares(int maxResult, std::string pageToken) const;
        const std::shared_ptr<std::vector<DeltaSharingProtocol::Schema>> ListSchemas(const DeltaSharingProtocol::Share &share, int maxResult, std::string pageToken) const;
        const std::shared_ptr<std::vector<DeltaSharingProtocol::Table>> ListTables(const DeltaSharingProtocol::Schema &schema, int maxResult, std::string pageToken) const;
        const std::shared_ptr<std::vector<DeltaSharingProtocol::Table>> ListAllTables(const DeltaSharingProtocol::Share &share, int maxResult, std::string pageToken) const;
        const std::shared_ptr<std::vector<DeltaSharingProtocol::File>> ListFilesInTable(const DeltaSharingProtocol::Table) const;
        const DeltaSharingProtocol::Metadata QueryTableMetadata(const DeltaSharingProtocol::Table &table) const;
        const int GetNumberOfThreads() { return this->maxThreads; };
        void PopulateCache(std::string url) { this->restClient.PopulateCache(url,this->cacheLocation); };
    protected:
    private:
        DeltaSharingRestClient restClient;
        std::string cacheLocation;
        int maxThreads;
    };
};

#endif