/* Node type: OPAL-RT Asynchronous Process (libOpalAsyncApi)
 *
 * Author: Steffen Vogel <steffen.vogel@opal-rt.com>
 * SPDX-FileCopyrightText: 2023-2025 OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdlib>
#include <thread>
#include <vector>
#include <unordered_map>

#include <unistd.h>

#include <villas/exceptions.hpp>
#include <villas/log.hpp>
#include <villas/nodes/opal_async.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::node::opal;
using namespace villas::utils;

// This defines the maximum number of signals (doubles) that can be sent
// or received by any individual OpAsyncSend/Recv blocks in the model. This
// only applies to the "model <-> asynchronous process" communication.
#define MAXSENDSIZE 64
#define MAXRECVSIZE 64

// Private static storage

// Shared Memory identifiers and size, provided via argv or config.
static std::string shmemAsyncName;

// Shared Memory identifiers and size, provided via argv or config.
static std::string shmemSystemCtrlName;

// Shared Memory identifiers and size, provided via argv or config.
static size_t shmemAsyncSize;

// A list of available send / receive icon IDs.
static std::vector<unsigned> idsSend, idsRecv;

// An spdlog sink which logs outputs to the OPAL-RT simulation via OpalPrint.
static spdlog::sink_ptr sink;

// String and Float parameters, provided by the RT-LAB OpAsyncGenCtrl block.
static Opal_GenAsyncParam_Ctrl params;

static std::thread awaiter;

static std::mutex lock; // Protecting state and nodeMap
static int state;
static std::unordered_map<int, OpalAsyncNode *> nodeMap;

static void waitForModelState(int state) {
  while (OpalGetAsyncModelState() != state)
    usleep(1000);
}

static void waitForAsyncSendRequests() {
  auto logger = Log::get("node:opal");
  int ret;
  unsigned id;

  waitForModelState(STATE_RUN);

  while (true) {
    ret = OpalWaitForAsyncSendRequest(&id);
    if (ret != EOK) {
      state = OpalGetAsyncModelState();
      if (state == STATE_RESET || state == STATE_STOP)
        break;
    }

    try {
      std::lock_guard<std::mutex> lk(lock);
      nodeMap.at(id)->markReady();
    } catch (std::out_of_range &e) {
      logger->warn("Received send request for unknown id={}", id);
    }
  }

  // Model has been stopped or resetted
  // We stop all OPAL nodes here.
  for (auto const &x : nodeMap)
    x.second->stop();
}

static int dumpGlobal() {
  auto logger = Log::get("node:opal");

  logger->debug("Controller ID: {}", params.controllerID);

  std::stringstream sss, rss;
  for (auto i : idsSend)
    sss << i << " ";

  for (auto i : idsRecv)
    rss << i << " ";

  logger->debug("Send Blocks: {}", sss.str());
  logger->debug("Receive Blocks: {}", rss.str());

  logger->debug("Control Block Parameters:");
  for (int i = 0; i < GENASYNC_NB_FLOAT_PARAM; i++)
    logger->debug("FloatParam[{}] = {}", i, (double)params.FloatParam[i]);

  for (int i = 0; i < GENASYNC_NB_STRING_PARAM; i++)
    logger->debug("StringParam[{}] = {}", i, params.StringParam[i]);

  return 0;
}

LogSink::LogSink(const std::string &shmemName)
    : shmemSystemCtrlName(shmemName) {
  // Enable the OpalPrint function. This prints to the OpalDisplay.
  int ret =
      OpalSystemCtrl_Register(const_cast<char *>(shmemSystemCtrlName.c_str()));
  if (ret != EOK)
    throw RuntimeError("OpalPrint() access not available ({})", ret);
}

LogSink::~LogSink() {
  OpalSystemCtrl_UnRegister((char *)shmemSystemCtrlName.c_str());
}

void LogSink::sink_it_(const spdlog::details::log_msg &msg) {
  spdlog::memory_buf_t buf;

  buf.clear();
  formatter_->format(msg, buf);

  auto bufPtr = buf.data();
  bufPtr[buf.size()] = '\0';

  OpalPrint(bufPtr);
}

int OpalAsyncNode::parse(json_t *json) {
  int ret, rply = -1, id = -1;

  ret = Node::parse(json);
  if (ret)
    return ret;

  json_error_t err;
  ret = json_unpack_ex(json, &err, 0, "{ s: i, s?: { s?: b } }", "id", &id,
                       "in", "reply", &rply);
  if (ret)
    throw ConfigError(json, err, "node-config-node-opal-async");

  if (id >= 0) {
    in.id = id;
    out.id = id;
  }

  if (rply != -1) {
    in.reply = rply;
  }

  return 0;
}

const std::string &OpalAsyncNode::getDetails() {
  // @todo Print send_params, recv_params

  if (details.empty())
    details = fmt::format("out.id={}, in.id={}, in.reply={}, in.mode={}",
                          out.id, in.id, in.reply, in.mode);

  return details;
}

int OpalAsyncNode::start() {
  int ret, szSend, szRecv;

  // Check if this instance has OpAsyncSend/Recv blocks in the RT-LAB model
  for (auto i : idsSend)
    in.present |= i == in.id;

  for (auto i : idsRecv)
    out.present |= i == out.id;

  // Get some more informations and parameters from OPAL-RT
  if (in.present) {
    ret = OpalGetAsyncSendParameters(&in.params, sizeof(Opal_SendAsyncParam),
                                     in.id);
    if (ret != EOK)
      throw RuntimeError("Failed to get parameters of send icon {}", in.id);

    ret = OpalGetAsyncSendIconMode(&in.mode, in.id);
    if (ret != EOK)
      throw RuntimeError("Failed to get send mode of icon {}", in.id);

    ret = OpalGetAsyncSendIconDataLength(&szSend, in.id);
    if (ret != EOK)
      throw RuntimeError("Failed to get send length for icon {}", in.id);

    in.length = szSend / sizeof(double);

    Node::in.signals =
        std::make_shared<SignalList>(in.length, SignalType::FLOAT);
  }

  if (out.present) {
    ret = OpalGetAsyncRecvParameters(&out.params, sizeof(Opal_RecvAsyncParam),
                                     out.id);
    if (ret != EOK)
      throw RuntimeError(
          "Failed to get parameters of OpAsyncRecv block with id={}", out.id);

    ret = OpalGetAsyncRecvIconDataLength(&szRecv, out.id);
    if (ret != EOK)
      throw RuntimeError("Failed to get send length for icon {}", out.id);

    out.length = szRecv / sizeof(double);
  }

  // Register node ID in node map
  std::unique_lock<std::mutex> lk(lock);
  nodeMap[in.id] = this;

  return Node::start();
}

int OpalAsyncNode::stop() {
  int ret = Node::stop();
  if (ret)
    return ret;

  std::unique_lock<std::mutex> lk(lock);

  nodeMap.erase(in.id);

  return 0;
}

int OpalAsyncNode::_read(struct Sample *smps[], unsigned cnt) {
  int ret;

  struct Sample *smp = smps[0];

  double data[smp->capacity];

  assert(cnt == 1);

  if (!in.present)
    throw RuntimeError("RT-LAB has no OpAsyncSend block with id={}", in.id);

  // Block until we are ready to receive new data
  waitReady();

  // No errors encountered yet
  ret = OpalSetAsyncSendIconError(0, in.id);
  if (ret != EOK)
    throw RuntimeError("Failed to clear send icon error");

  smp->flags = (int)SampleFlags::HAS_DATA;
  smp->signals = Node::in.signals;
  smp->length = in.length > smp->capacity ? smp->capacity : in.length;

  // Read data from the model
  ret = OpalGetAsyncSendIconData(data, smp->length * sizeof(double), in.id);
  if (ret != EOK)
    throw RuntimeError("Failed to get data of OpAsyncSend block with id={}",
                       in.id);

  for (unsigned i = 0; i < smp->length; i++)
    smp->data[i].f = data[i];

  // This next call allows the execution of the "asynchronous" process
  // to actually be synchronous with the model. To achieve this, you
  // should set the "Sending Mode" in the OpAsyncSend block to
  // NEED_REPLY_BEFORE_NEXT_SEND or NEED_REPLY_NOW. This will force
  // the model to wait for this process to call this
  // OpalAsyncSendRequestDone function before continuing.
  if (in.reply &&
      (in.mode == NEED_REPLY_BEFORE_NEXT_SEND || in.mode == NEED_REPLY_NOW)) {
    ret = OpalAsyncSendRequestDone(in.id);
    if (ret != EOK)
      throw RuntimeError("Failed to complete send request of icon {}", in.id);
  }

  return 1;
}

int OpalAsyncNode::_write(struct Sample *smps[], unsigned cnt) {
  assert(cnt == 1);

  struct Sample *smp = smps[0];

  int ret;
  unsigned realLen;
  double data[smp->length];

  if (!out.present)
    throw RuntimeError("RT-LAB has no OpAsyncRecv block with id={}", out.id);

  ret = OpalSetAsyncRecvIconStatus(smp->sequence,
                                   out.id); // Set the status to the message ID
  if (ret != EOK)
    throw RuntimeError(
        "Failed to update status of OpAsyncRecv block with id={}", out.id);

  ret = OpalSetAsyncRecvIconError(0, out.id); // Clear the error
  if (ret != EOK)
    throw RuntimeError("Failed to update error of OpAsyncRecv block with id={}",
                       out.id);

  // Get the number of signals to send back to the model
  if (out.length > smp->length)
    logger->warn(
        "RT-LAB model is expecting more signals ({}) than in message ({})",
        out.length, smp->length);

  if (out.length < smp->length) {
    logger->warn("RT-LAB model can only accept {} of {} signals", out.length,
                 smp->length);
    realLen = out.length;
  } else
    realLen = smp->length;

  for (unsigned i = 0; i < MIN(realLen, smp->signals->size()); i++) {
    auto sig = smp->signals->getByIndex(i);
    data[i] = smp->data[i].cast(sig->type, SignalType::FLOAT).f;
  }

  ret = OpalSetAsyncRecvIconData(data, realLen * sizeof(double), out.id);
  if (ret != EOK)
    throw RuntimeError("Failed to set receive data of icon {}", out.id);

  return 1;
}

void OpalAsyncNode::markReady() {
  std::unique_lock<std::mutex> lk(readyLock);

  ready = true;
  readyCv.notify_all();
}

void OpalAsyncNode::waitReady() {
  std::unique_lock<std::mutex> lk(readyLock);

  readyCv.wait(lk, [=] { return this->ready; });
  ready = false;
}

int OpalAsyncNodeFactory::start(SuperNode *sn) {
  int ret, idsRecvLen, idsSendLen;

  auto *shmemAsyncNamePtr = std::getenv("OPAL_ASYNC_SHMEM_NAME");
  auto *shmemAsyncSizePtr = std::getenv("OPAL_ASYNC_SHMEM_SIZE");
  auto *shmemSystemCtrlNamePtr = std::getenv("OPAL_PRINT_SHMEM_NAME");

  if (!shmemAsyncNamePtr || !shmemAsyncSizePtr || !shmemSystemCtrlNamePtr)
    throw RuntimeError("Missing OPAL_ environment variables");

  shmemAsyncName = shmemAsyncNamePtr;
  shmemAsyncSize = atoi(shmemAsyncSizePtr);
  shmemSystemCtrlName = shmemSystemCtrlNamePtr;

  // Register OpalPrint sink for spdlog
  sink = std::make_shared<LogSink>(shmemSystemCtrlName);
  Log::getInstance().replaceStdSink(sink);

  // Open shared memory region created by the RT-LAB model.
  ret = OpalOpenAsyncMem(shmemAsyncSize,
                         const_cast<char *>(shmemAsyncName.c_str()));
  if (ret != EOK)
    throw RuntimeError("Model shared memory not available ({})", ret);

  // Get list of SendIDs and RecvIDs
  ret = OpalGetNbAsyncSendIcon(&idsSendLen);
  if (ret != EOK)
    throw RuntimeError("Failed to get number of OpAsyncSend blocks ({})", ret);

  ret = OpalGetNbAsyncRecvIcon(&idsRecvLen);
  if (ret != EOK)
    throw RuntimeError("Failed to get number of OpAsyncRecv blocks ({})", ret);

  idsSend.resize(idsSendLen);
  idsRecv.resize(idsRecvLen);

  ret = OpalGetAsyncSendIDList(idsSend.data(), idsSendLen * sizeof(int));
  if (ret != EOK)
    throw RuntimeError("Failed to get list of send icon ids ({})", ret);

  ret = OpalGetAsyncRecvIDList(idsRecv.data(), idsRecvLen * sizeof(int));
  if (ret != EOK)
    throw RuntimeError("Failed to get list of receive icon ids ({})", ret);

  logger->info("Started VILLASnode as asynchronous process");

  dumpGlobal();

  awaiter = std::thread(waitForAsyncSendRequests);

  return NodeFactory::start(sn);
}

int OpalAsyncNodeFactory::stop() {
  int ret;

  logger->debug("Stopping waiter thread");

  awaiter.join();

  logger->debug("Closing OPAL-RT shared memory mapping");

  ret = OpalCloseAsyncMem(shmemAsyncSize, shmemAsyncName.c_str());
  if (ret != EOK)
    throw RuntimeError("Failed to close shared memory region ({})", ret);

  return NodeFactory::stop();
}

static OpalAsyncNodeFactory p;
