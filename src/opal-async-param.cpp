/* Retrieve parameters from OPAL-RTs AsyncApi
 *
 * Author: Steffen Vogel <steffen.vogel@opal-rt.com>
 * SPDX-FileCopyrightText: 2023-2025 OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 *

#include <cstring>
#include <iostream>

#include <AsyncApi.h>
#include <OpalGenAsyncParamCtrl.h>

int main(int argc, char *argv[]) {
  Opal_GenAsyncParam_Ctrl params;

  if (argc != 4) {
    std::cerr << "Usage: AsyncShmemName AsyncShmemSize PrintShmemName"
              << std::endl;
    return -1;
  }

  auto err = OpalOpenAsyncMem(atoi(argv[2]), argv[1]);
  if (err != EOK) {
    std::cerr << "Model shared memory not available (" << err << ")"
              << std::endl;
    return -1;
  }

  memset(&params, 0, sizeof(params));

  err = OpalGetAsyncCtrlParameters(&params, sizeof(params));
  if (err != EOK) {
    std::cerr << "Could not get controller parameters (" << err << ")"
              << std::endl;
    return -1;
  }

  std::cout << params.StringParam[0] << std::endl;

  return 0;
}
