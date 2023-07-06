/** Node-type for smu data aquisition.
 *
 * @file
 * @author Manuel Pitz <manuel.pitz@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <list>
#include <cmath>
#include <cstring>
#include <byteswap.h>

#include <villas/exceptions.hpp>
#include <villas/nodes/smu.hpp>
#include <villas/utils.hpp>


using namespace villas;
using namespace villas::node;
using namespace villas::utils;

size_t SMUNode::mem_pos = 0;
pthread_mutex_t SMUNode::mutex;
pthread_cond_t SMUNode::cv;

SMUNode::SMUNode(const std::string &name) :
    mode(MODE_FREERUN),
    sample_rate(FS_10kSPS),
    fps(FB_10FPS),
    sync(SYNC_PPS),
	daq_cfg({sample_rate, fps, mode, sync})
{
    return;
}

int SMUNode::prepare()
{
	assert(state == State::CHECKED);
	return Node::prepare();
}

int SMUNode::parse(json_t *json, const uuid_t sn_uuid)
{
	int ret = Node::parse(json, sn_uuid);
	if (ret)
		return ret;

	json_error_t err;


    json_t *jsonDumpers = nullptr;

    char *modeIn = nullptr;
    char *syncIn = nullptr;
    char *fpsIn = nullptr;
    char *sample_rateIn = nullptr;

	ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: s, s?: s, s?: s, s?: o }",
		"mode", &modeIn,
        "sync", &syncIn,
        "fps", &fpsIn,
        "sample_rate", &sample_rateIn,
        "dumper", &jsonDumpers
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-smu");

    if (jsonDumpers) {
        auto signals = getInputSignals();

        size_t i;
        json_t *jsonDumper;
        ret = json_unpack_ex(jsonDumpers, &err, 0, "{ o }",
            &jsonDumper
        );
        json_array_foreach(jsonDumpers, i, jsonDumper) {
            char *nameIn = nullptr;
            char *pathIn = nullptr;

            ret = json_unpack_ex(jsonDumper, &err, 0, "{ s: s, s: s }",
                "name", &nameIn,
                "path", &pathIn
            );
            if (ret)
                throw ConfigError(json, err, "node-config-node-smu", "Failed to parse dumper configuration");

            int idx = signals->getIndexByName(nameIn);
            if (idx == -1)
                throw ConfigError(json, err, "node-config-node-smu", "Could not find signal {} for dumper", nameIn);

            dumpers[idx].setPath(pathIn);
            dumpers[idx].openSocket();
            dumpers[idx].setActive();

        }
  
    }

    if(!modeIn || strcmp(modeIn, "MODE_FREERUN")==0)
        mode = MODE_FREERUN;
    else if (strcmp(modeIn, "MODE_ONESHOT")==0)
        mode = MODE_ONESHOT;
    else if (strcmp(modeIn, "MODE_VIRTUAL")==0)
        mode = MODE_VIRTUAL;
    else if (strcmp(modeIn, "MODE_MAX")==0)
        mode = MODE_MAX;

    if(!syncIn || strcmp(syncIn, "SYNC_PPS")==0)
        sync = SYNC_PPS;
    else if (strcmp(syncIn, "SYNC_NONE")==0)
        sync = SYNC_NONE;
    else if (strcmp(syncIn, "SYNC_NTP")==0)
        sync = SYNC_NTP;
    else if (strcmp(syncIn, "SYNC_MAX")==0)
        sync = SYNC_MAX;

    if(!fpsIn || strcmp(fpsIn, "FB_10FPS")==0)
        fps = FB_10FPS;
    else if (strcmp(fpsIn, "FB_20FPS")==0)
        fps = FB_20FPS;
    else if (strcmp(fpsIn, "FB_50FPS")==0)
        fps = FB_50FPS;
    else if (strcmp(fpsIn, "FB_100FPS")==0)
        fps = FB_100FPS;
    else if (strcmp(fpsIn, "FB_200FPS")==0)
        fps = FB_200FPS;

    if (!sample_rateIn || strcmp(sample_rateIn, "FS_10kSPS")==0)
        sample_rate = FS_10kSPS;
    else if (strcmp(sample_rateIn, "FS_1kSPS")==0)
        sample_rate = FS_1kSPS;
    else if (strcmp(sample_rateIn, "FS_2kSPS")==0)
        sample_rate = FS_2kSPS;
    else if (strcmp(sample_rateIn, "FS_5kSPS")==0)
        sample_rate = FS_5kSPS;
    else if (strcmp(sample_rateIn, "FS_20kSPS")==0)
        sample_rate = FS_20kSPS;
    else if (strcmp(sample_rateIn, "FS_25kSPS")==0)
        sample_rate = FS_25kSPS;
    else if (strcmp(sample_rateIn, "FS_50kSPS")==0)
        sample_rate = FS_50kSPS;
    else if (strcmp(sample_rateIn, "FS_100kSPS")==0)
        sample_rate = FS_100kSPS;
    else if (strcmp(sample_rateIn, "FS_200kSPS")==0) {
        sample_rate = FS_200kSPS;
    }
    
    buffer_size = sample_rate * 1000 / fps;

    if (in.vectorize!=buffer_size) {
        throw ConfigError(json, "node-config-node-smu", "Vectorize has to be equal to buffer size {}", buffer_size);
    }

	return 0;
}

int SMUNode::start()
{
	assert(state == State::PREPARED ||
	       state == State::PAUSED);

	int ret = Node::start();


    daq_cfg.rate = sample_rate;
    daq_cfg.buff = fps;
    daq_cfg.mode = mode;
    daq_cfg.sync = sync;  


    fd = ::open(SMU_DEV, O_RDWR);
    if (fd < 0)
        logger->warn("Fail to open the device driver");

    // Associate kernel-to-user Task ID
    if (ioctl(fd, SMU_IOC_REG_TASK, 0)){
        logger->warn("Fail to register the driver");
        ::close(fd);
        // return;
    }

    // Map kernel-to-user shared memory
    void *sh_mem = mmap(nullptr, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    shared_mem = (uint8_t*)sh_mem;
    if (shared_mem == MAP_FAILED)
        logger->warn("Fail to map driver memory");

    // Install PPS signal handler
    sigemptyset(&act.sa_mask);
    act.sa_flags = (SA_SIGINFO | SA_RESTART);
    act.sa_sigaction = sync_signal;
    if (sigaction(SMU_SIG_SYNC, &act, nullptr))
        logger->warn("Fail to install PPS handler");

    // Install ADC signal handler
    sigemptyset(&act.sa_mask);
    act.sa_flags = (SA_SIGINFO | SA_RESTART);
    act.sa_sigaction = data_available_signal;
    if (sigaction(SMU_SIG_DATA, &act, nullptr))
        logger->warn("Fail to install ADC handler");
	
    // Update DAQ memory configuration
    shared_mem_pos = 0;
    shared_mem_inc = sizeof(smu_mcsc_t);
    shared_mem_dim = daq_cfg.rate;

    // Stop DAQ driver
    if (ioctl(fd, SMU_IOC_STOP, 0))
        logger->warn("Fail to stop the driver");

    // Configure DAQ driver
    if (::write(fd, &daq_cfg, sizeof(daq_cfg)))
        logger->warn("Fail to configure the driver");

    // Start DAQ driver
    if (ioctl(fd, SMU_IOC_START, 0)) {
        logger->warn("Fail to start the driver");
    }

	if (!ret)
		state = State::STARTED;

	return ret;
}

int SMUNode::stop()
{
    // Stop DAQ driver
    if(!ioctl(fd, SMU_IOC_STOP, 0)){
        // Unmap kernel-to-user shared memory
        munmap(shared_mem, MMAP_SIZE);
        while(close(fd) != 0){}
    }

    return 1;
}

void SMUNode::sync_signal(int, siginfo_t *info, void*)
{

	//mem_pos = (info->si_value.sival_int);
	/* Signal uldaq_read() about new data */
	//pthread_cond_signal(&u->in.cv);
}

void SMUNode::data_available_signal(int, siginfo_t *info, void*)
{
    
	mem_pos = (info->si_value.sival_int);
	/* Signal uldaq_read() about new data */
	pthread_cond_signal(&cv);
}


int SMUNode::_read(struct Sample *smps[], unsigned cnt)
{
	struct timespec ts;
	ts.tv_sec = time(nullptr);
	ts.tv_nsec = 0;

    pthread_mutex_lock(&mutex);

	pthread_cond_wait(&cv, &mutex);

    smu_mcsc_t* p = (smu_mcsc_t*)shared_mem;

    for (unsigned j = 0; j < cnt; j++) {
        struct Sample *t = smps[j];

        for (unsigned i = 0; i < 8; i++) {
            int16_t data = p[mem_pos].ch[i];
            t->data[i].f = ((double)data);
            if (dumpers[i].isActive())
                dumpers[i].writeDataBinary(1, &(t->data[i].f));
        }

        mem_pos++;

        t->flags = (int) SampleFlags::HAS_TS_ORIGIN | (int) SampleFlags::HAS_DATA | (int) SampleFlags::HAS_SEQUENCE;
        t->ts.origin = ts;
        t->sequence = sequence++;
        t->length = 8;
        t->signals = in.signals;
    }

	pthread_mutex_unlock(&mutex);

	return cnt;
}

static char n[] = "smu";
static char d[] = "SMU data acquisition";
static NodePlugin<SMUNode, n , d, (int) NodeFactory::Flags::SUPPORTS_READ | (int) NodeFactory::Flags::SUPPORTS_POLL> p;
