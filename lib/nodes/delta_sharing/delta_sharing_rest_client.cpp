/* Node type: delta_sharing.
 *
 * Author: Ritesh Karki <ritesh.karki@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/nodes/delta_sharing/delta_sharing_rest_client.hpp>
#include <villas/nodes/delta_sharing/protocol.hpp>
#include <fstream>
#include <sstream>
#include <restclient-cpp/restclient.h>
#include <restclient-cpp/connection.h>
#include <filesystem>
#include <exception>
#include <chrono>
#include <thread>

namespace DeltaSharing
{

    const std::string DeltaSharingRestClient::user_agent = "delta-sharing-CPP/0.0.1";

    DeltaSharingRestClient::DeltaSharingRestClient(std::string filename)
    {
        json j = ReadFromFile(filename);
        if(j.empty()) {
            return;
        }
        this->profile = j;
        RestClient::init();
    };

    DeltaSharingRestClient::~DeltaSharingRestClient()
    {
        std::cerr << "DeltaSharingRestClient destructed" << std::endl;
    };

    const std::shared_ptr<std::vector<DeltaSharingProtocol::Share>> DeltaSharingRestClient::ListShares(int maxResult, std::string pageToken) const
    {
        std::unique_ptr<RestClient::Connection> c = std::unique_ptr<RestClient::Connection>(new RestClient::Connection(this->profile.endpoint));
        c->SetUserAgent(this->user_agent);
        RestClient::Response r = c->get("/shares");
        json j = json::parse(r.body);
        auto items = j["items"];
        std::shared_ptr<std::vector<DeltaSharingProtocol::Share>> p;
        p = std::make_shared<std::vector<DeltaSharingProtocol::Share>>();
        for (auto it = items.begin(); it < items.end(); it++)
        {
            DeltaSharingProtocol::Share s = it.value();
            p->push_back(s);
        }
        return p;
    };

    const std::shared_ptr<std::vector<DeltaSharingProtocol::Schema>> DeltaSharingRestClient::ListSchemas(const DeltaSharingProtocol::Share &share, int maxResult, std::string pageToken) const
    {
        std::unique_ptr<RestClient::Connection> c = std::unique_ptr<RestClient::Connection>(new RestClient::Connection(this->profile.endpoint));
        c->SetUserAgent(this->user_agent);
        std::string path = "/shares/" + share.name + "/schemas";
        RestClient::Response r = c->get(path);
        json j = json::parse(r.body);
        auto items = j["items"];
        std::shared_ptr<std::vector<DeltaSharingProtocol::Schema>> p;
        p = std::make_shared<std::vector<DeltaSharingProtocol::Schema>>();
        for (auto it = items.begin(); it < items.end(); it++)
        {
            DeltaSharingProtocol::Schema s = it.value().get<DeltaSharingProtocol::Schema>();
            p->push_back(s);
        }
        return p;
    };

    const std::shared_ptr<std::vector<DeltaSharingProtocol::Table>> DeltaSharingRestClient::ListTables(const DeltaSharingProtocol::Schema &schema, int maxResult, std::string pageToken) const
    {
        std::unique_ptr<RestClient::Connection> c = std::unique_ptr<RestClient::Connection>(new RestClient::Connection(this->profile.endpoint));
        c->SetUserAgent(this->user_agent);
        std::string path = "/shares/" + schema.share + "/schemas/" + schema.name + "/tables";
        RestClient::Response r = c->get(path);
        json j = json::parse(r.body);
        auto items = j["items"];
        std::shared_ptr<std::vector<DeltaSharingProtocol::Table>> t;
        t = std::make_shared<std::vector<DeltaSharingProtocol::Table>>();
        for (auto it = items.begin(); it < items.end(); it++)
        {
            DeltaSharingProtocol::Table s = it.value();
            t->push_back(s);
        }
        return t;
    };

    const std::shared_ptr<std::vector<DeltaSharingProtocol::Table>> DeltaSharingRestClient::ListAllTables(const DeltaSharingProtocol::Share &share, int maxResult, std::string pageToken) const
    {
        std::unique_ptr<RestClient::Connection> c = std::unique_ptr<RestClient::Connection>(new RestClient::Connection(this->profile.endpoint));
        c->SetUserAgent(this->user_agent);
        std::string path = "/shares/" + share.name + "/all-tables";
        RestClient::Response r = c->get(path);
        json j = json::parse(r.body);
        auto items = j["items"];
        std::shared_ptr<std::vector<DeltaSharingProtocol::Table>> t;
        t = std::make_shared<std::vector<DeltaSharingProtocol::Table>>();
        for (auto it = items.begin(); it < items.end(); it++)
        {
            DeltaSharingProtocol::Table s = it.value();
            t->push_back(s);
        }
        return t;
    };

    const DeltaSharingProtocol::Metadata DeltaSharingRestClient::QueryTableMetadata(const DeltaSharingProtocol::Table &table) const {
        std::unique_ptr<RestClient::Connection> c = std::unique_ptr<RestClient::Connection>(new RestClient::Connection(this->profile.endpoint));
        c->SetUserAgent(this->user_agent);
        std::string path = "/shares/" + table.share + "/schemas/" + table.schema + "/tables/" + table.name + "/metadata";
        RestClient::Response r = c->get(path);
        std::istringstream input;
        input.str(r.body);
        json j;
        DeltaSharingProtocol::Metadata m;
        int cnt = 0;
        for (std::string line; std::getline(input, line); cnt++)
        {
            if (cnt == 1)
            {
                j = json::parse(line);
                m = j["metaData"];
            }
        }

        return m;
    };

    json DeltaSharingRestClient::ReadFromFile(std::string filename)
    {
        std::ifstream is;
        try {
            is.open(filename, std::ifstream::in);
        }
        catch (std::exception *e) {
            json r = {};
            return r;
        }

        json j;
        is >> j;
        is.close();
        return j;
    };

    const DeltaSharingProtocol::DeltaSharingProfile &DeltaSharingRestClient::GetProfile() const
    {
        return this->profile;
    }

    void DeltaSharingRestClient::PopulateCache(std::string url, std::string cacheLocation) {
        int protocolLength = 0;
        if ((url.find("http://")) != std::string::npos)
        {
            protocolLength = 7;
        }

        if ((url.find("https://")) != std::string::npos)
        {
            protocolLength = 8;
        }
        auto pos = url.find_first_of('?', protocolLength);
        auto path = url.substr(protocolLength, pos - protocolLength); // Removing "https://"

        std::vector<std::string> urlparts;
        while ((pos = path.find("/")) != std::string::npos)
        {
            urlparts.push_back(path.substr(0, pos));
            path.erase(0, pos + 1);
        }
        if (urlparts.size() != 3)
        {
            std::cerr << "Invalid URL:" << url << std::endl;
            return;
        }
        std::string tbl = urlparts.back();
        urlparts.pop_back();
        std::string schema = urlparts.back();
        urlparts.pop_back();
        std::string share = urlparts.back();

        auto completePath = cacheLocation + "/" + share + "/" + schema + "/" + tbl;

        if(!std::filesystem::exists(completePath + "/" + path))
        {
            std::cerr << completePath+ "/" + path << " does not exist in cache" << std::endl;
            std::filesystem::create_directories(completePath);
            auto r = this->get(url);
            int cnt = 0;
            std::cerr << url << " code: " << r.code << std::endl;

            while (this->shouldRetry(r))
            {
                cnt++;
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (cnt > 4)
                {
                    std::cerr << "Failed to retrieve file using url: ( Response code: " << r.code << ") Message: " << r.body << std::endl;
                    return;
                }
                r = this->get(url);
            }

            if (r.code != 200)
            {
                std::cerr << "Could not read file: " << r.code << " Message: " << r.body << std::endl;
                return;
            }

            std::fstream f;
            f.open(completePath + "/" + path, std::ios::trunc | std::ios::out | std::ios::binary);
            f.write(r.body.c_str(), r.body.size());
            f.flush();
            f.close();
        }
    };

    const std::shared_ptr<std::vector<DeltaSharingProtocol::File>> DeltaSharingRestClient::ListFilesInTable(DeltaSharingProtocol::Table table) const
    {
        std::unique_ptr<RestClient::Connection> c = std::unique_ptr<RestClient::Connection>(new RestClient::Connection(this->profile.endpoint));
        c->SetUserAgent(this->user_agent);
        std::string path = "/shares/" + table.share + "/schemas/" + table.schema + "/tables/" + table.name + "/query";
        RestClient::HeaderFields h;
        h.insert({"Content-Type", "application/json; charset=UTF-8"});
        h.insert({"Authorization", "Bearer: " + this->profile.bearerToken});
        c->SetHeaders(h);
        DeltaSharingProtocol::data d;
        json j = d;
        RestClient::Response r = c->post(path, j.dump());
        int cnt = 0;
        std::istringstream input;
        input.str(r.body);
        std::shared_ptr<std::vector<DeltaSharingProtocol::File>> t;
        t = std::make_shared<std::vector<DeltaSharingProtocol::File>>();
        for (std::string line; std::getline(input, line); cnt++)
        {
            if (cnt > 1)
            {
                json jf = json::parse(line);
                json jf2 = jf["file"];
                DeltaSharingProtocol::File f = jf2;
                t->push_back(f);
            }
        }

        return t;
    };
    RestClient::Response DeltaSharingRestClient::get(std::string url)
    {
        RestClient::Response r = RestClient::get(url);
        return r;
    };


    const bool DeltaSharingRestClient::shouldRetry(RestClient::Response &r) const {
    if(r.code == 200)
        return false;
	  if (r.code == 429) {
		std::cerr << "Retry operation due to status code: 429" << std::endl;
		return true;
	} else if (r.code >= 500 && r.code < 600 ) {
		std::cerr << "Retry operation due to status code: " << r.code << std::endl;
		return true;
	} else
		return false;

    };

};
