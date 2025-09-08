
#ifndef __FUNCTIONS_H__
#define __FUNCTIONS_H__

#include <vector>
#include <string>
#include <iostream>
#include <arrow/table.h>
#include "DeltaSharingClient.h"
#include <thread>

namespace DeltaSharing {

    const std::vector<std::string> ParseURL(std::string path); 
     std::shared_ptr<DeltaSharingClient> NewDeltaSharingClient(std::string profile, boost::optional<std::string> cacheLocation);
    const std::shared_ptr<arrow::Table> LoadAsArrowTable(std::string path, int fileno);

   
   
};

#endif