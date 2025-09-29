#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <iostream>
#include "jansson_wrapper.h"
#include <map>
#include <string>
#include <vector>
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>

namespace DeltaSharing
{

    namespace DeltaSharingProtocol
    {

        using json = JanssonWrapper::json;

        struct DeltaSharingProfile
        {
        public:
            int shareCredentialsVersion;
            std::string endpoint;
            std::string bearerToken;
            boost::optional<std::string> expirationTime;
        };

        inline void from_json(const json &j, DeltaSharingProfile &p)
        {
            if (j.contains("shareCredentialsVersion"))
            {
                p.shareCredentialsVersion = j["shareCredentialsVersion"].integer_value();
            }
            if (j.contains("endpoint"))
            {
                p.endpoint = j["endpoint"].string_value();
            }
            if (j.contains("bearerToken"))
            {
                p.bearerToken = j["bearerToken"].string_value();
            }
            if (j.contains("expirationTime"))
            {
                p.expirationTime = j["expirationTime"].string_value();
            }
        };

        struct Share
        {
        public:
            std::string name = "";
            boost::optional<std::string> id;
        };

        inline void from_json(const json &j, Share &s)
        {
            s.name = j["name"].string_value();
            if (j.contains("id") == false)
            {
                s.id = boost::none;
            }
            else
            {
                s.id = j["id"].string_value();
            }
        };

        struct Schema
        {
        public:
            std::string name;
            std::string share;
        };

        inline void from_json(const json &j, Schema &s)
        {
            s.name = j["name"].string_value();
            if (j.contains("share") == true)
            {
                s.share = j["share"].string_value();
            }
        };

        struct Table
        {
        public:
            std::string name;
            std::string share;
            std::string schema;
        };

        inline void from_json(const json &j, Table &t)
        {
            if (j.contains("name"))
            {
                t.name = j["name"].string_value();
            }
            if (j.contains("share"))
            {
                t.share = j["share"].string_value();
            }
            if (j.contains("schema"))
            {
                t.schema = j["schema"].string_value();
            }
        };

        struct File
        {
        public:
            std::string url;
            boost::optional<std::string> id;
            std::map<std::string, std::string> partitionValues;
            std::size_t size;
            std::string stats;
            boost::optional<std::string> timestamp;
            boost::optional<std::string> version;
        };

        inline void from_json(const json &j, File &f)
        {
            if (j.contains("url"))
            {
                f.url = j["url"].string_value();
            }
            if (j.contains("id"))
            {
                f.id = j["id"].string_value();
            }
            if (j.contains("partitionValues"))
            {
                json arr = j["partitionValues"];
                if (arr.is_array())
                {
                    for (auto it = arr.begin(); it != arr.end(); ++it)
                    {
                        // For jansson, we need to handle the array differently
                        // This assumes partitionValues is an array of objects with key-value pairs
                        json item = *it;
                        if (item.is_object()) {
                            for (auto kv_it = item.begin(); kv_it != item.end(); ++kv_it) {
                                f.partitionValues[kv_it.key()] = (*kv_it).string_value();
                            }
                        }
                    }
                }
            }
            if (j.contains("size"))
            {
                f.size = static_cast<std::size_t>(j["size"].integer_value());
            }
            if (j.contains("stats"))
            {
                f.stats = j["stats"].string_value();
            }
            if (j.contains("timestamp"))
            {
                f.timestamp = j["timestamp"].string_value();
            }
            if (j.contains("version"))
            {
                f.version = j["version"].string_value();
            }
        };

        struct Format {
            std::string provider;
        };

        inline void from_json(const json &j, Format &f)
        {
            if (j.contains("provider"))
            {
                f.provider = j["provider"].string_value();
            }
        }

        struct Metadata
        {
            Format format;
            std::string id;
            std::vector<std::string> partitionColumns;
            std::string schemaString;
        };

        inline void from_json(const json &j, Metadata &m)
        {
            if (j.contains("format"))
            {
                from_json(j["format"], m.format);
            }
            if (j.contains("id"))
            {
                m.id = j["id"].string_value();
            }
            if (j.contains("partitionColumns"))
            {
                json arr = j["partitionColumns"];
                if (arr.is_array())
                {
                    for (auto it = arr.begin(); it != arr.end(); ++it)
                    {
                        m.partitionColumns.push_back((*it).string_value());
                    }
                }
            }
            if (j.contains("schemaString"))
            {
                m.schemaString = j["schemaString"].string_value();
            }
        }

        struct data
        {
            std::vector<std::string> predicateHints;
            int limitHint;
        };

        inline void from_json(const json &j, data &d)
        {
            if (j.contains("predicateHints"))
            {
                json arr = j["predicateHints"];
                if (arr.is_array())
                {
                    for (auto it = arr.begin(); it != arr.end(); ++it)
                    {
                        d.predicateHints.push_back((*it).string_value());
                    }
                }
            }
            if (j.contains("limitHint"))
            {
                d.limitHint = j["limitHint"].integer_value();
            }
        }

        struct format
        {
            std::string provider;
        };

        inline void from_json(const json &j, format &f)
        {
            if (j.contains("provider"))
            {
                f.provider = j["provider"].string_value();
            }
        }

        struct protocol
        {
            int minReaderVersion;
        };

        inline void from_json(const json &j, protocol &p)
        {
            if (j.contains("minReaderVersion"))
            {
                p.minReaderVersion = j["minReaderVersion"].integer_value();
            }
        }

        struct stats
        {
            long long numRecords;
            long minValues;
            long maxValues;
            long nullCount;
        };

        inline void from_json(const json &j, stats &s)
        {
            if (j.contains("numRecords"))
            {
                s.numRecords = j["numRecords"].integer_value();
            }
            if (j.contains("minValues"))
            {
                s.minValues = j["minValues"].integer_value();
            }
            if (j.contains("maxValues"))
            {
                s.maxValues = j["maxValues"].integer_value();
            }
            if (j.contains("nullCount"))
            {
                s.nullCount = j["nullCount"].integer_value();
            }
        }

    };
};

#endif
