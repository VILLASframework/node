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

#include <villas/exceptions.hpp>
#include <villas/nodes/smu.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

SMUNode::SMUNode(const std::string &name) :
	mem_pos(0),
	daq_cfg_default({FS_10kSPS, FB_10FPS, MODE_FREERUN, SYNC_PPS})
{
    return;
}

int SMUNode::prepare()
{
	assert(state == State::CHECKED);

	return Node::prepare();
}

int SMUNode::start()
{
	assert(state == State::PREPARED ||
	       state == State::PAUSED);

	int ret = Node::start();


    fd = open(SMU_DEV, O_RDWR);
    if (fd < 0){
    //     /*SMU_LOGE(TAG,"[%1] fail to open the device driver");
    //     smuCFG->error.daq = SMU_ERR_DRV_NOT_FOUND;*/
    //     return;
        logger->warn("fail to open the device driver");
    }

    // Associate kernel-to-user Task ID
    if (ioctl(fd, SMU_IOC_REG_TASK, 0)){
        /*SMU_LOGE(TAG,"[%1] fail to register the driver");
        smuCFG->error.daq = SMU_ERR_DRV_NOT_ASSOC;*/
        logger->warn("fail to register the driver");
        close(fd);
        // return;
    }

    // Map kernel-to-user shared memory
    void *sh_mem = mmap(nullptr, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    shared_mem = (uint8_t*)sh_mem;
    if (shared_mem == MAP_FAILED){
    //     /*SMU_LOGE(TAG,"[%1] fail to map driver memory");
    //     smuCFG->error.daq = SMU_ERR_DRV_MMAP_FAIL;*/
    //     // return;
        logger->warn("fail to map driver memory");

    }

    // Install PPS signal handler
    sigemptyset(&act.sa_mask);
    act.sa_flags = (SA_SIGINFO | SA_RESTART);
    act.sa_sigaction = sync_signal;
    if (sigaction(SMU_SIG_SYNC, &act, nullptr)){
    //     /*SMU_LOGE(TAG,"[%1] fail to install PPS handler");
    //     smuCFG->error.daq = SMU_ERR_DRV_NO_SIGNAL;*/
    //     // return;
        logger->warn("fail to install PPS handler");

    }

    // Install ADC signal handler
    sigemptyset(&act.sa_mask);
    act.sa_flags = (SA_SIGINFO | SA_RESTART);
    act.sa_sigaction = data_available_signal;
    if (sigaction(SMU_SIG_DATA, &act, nullptr)){
        /*SMU_LOGE(TAG,"[%1] fail to install ADC handler");
        smuCFG->error.daq = SMU_ERR_DRV_NO_SIGNAL;*/
        // return;
        logger->warn("fail to install ADC handler");

    }

    // Check status
    /*if (smuCFG->error.daq){
        SMU_LOGW(TAG,"[%1] cannot configure the module");
        return;
    }*/
	
    // Update DAQ memory configuration
    shared_mem_pos = 0;
    shared_mem_inc = sizeof(smu_mcsc_t);
    shared_mem_dim = daq_cfg_default.rate;


    // Stop DAQ driver
    if (ioctl(fd, SMU_IOC_STOP, 0)){
        /*SMU_LOGE(TAG,"[%1] fail to stop the driver");
        smuCFG->error.daq = SMU_ERR_DRV_NO_STOP;*/
        logger->warn("fail to stop the driver");
    }

    // Configure DAQ driver
    if (::write(fd, &daq_cfg_default, sizeof(daq_cfg_default))){
        /*smuCFG->error.daq = SMU_ERR_DRV_NOT_CONF;
        SMU_LOGE(TAG,"[%1] fail to configure the driver");*/
        // return;
        logger->warn("fail to configure the driver");

    }

    // Start DAQ driver
    if (ioctl(fd, SMU_IOC_START, 0)){
        /*SMU_LOGE(TAG,"[%1] fail to start the driver");
        smuCFG->error.daq = SMU_ERR_DRV_NO_START;*/
    //     return;
        logger->warn("fail to start the driver");

    }
	
    // DAQ module configured
    //SMU_LOGI(TAG,"[%1] module configured");

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
    /*} else {
        SMU_LOGE(TAG,"[%1] fail to stop the driver");
        smuCFG->error.daq = SMU_ERR_DRV_NO_STOP;*/
    }

    // Stop Thread
    /*SMU_LOGI(TAG,"[%1] module stopped");
    smuCFG->error.daq = SMU_IDLE;
    emit stopped();*/
    //set_cpu_governor("ondemand");
    return 1;
}

void SMUNode::sync_signal(int, siginfo_t *info, void*)
{

	//mem_pos = (info->si_value.sival_int);
    int test = 5;
    test ++;

	/* Signal uldaq_read() about new data */
	//pthread_cond_signal(&u->in.cv);
}

void SMUNode::data_available_signal(int, siginfo_t *info, void*)
{

	//mem_pos = (info->si_value.sival_int);
    int test = 5;
    test ++;

	/* Signal uldaq_read() about new data */
	//pthread_cond_signal(&u->in.cv);
}


int SMUNode::_read(struct Sample *smps[], unsigned cnt)
{
	struct Sample *t = smps[0];

	struct timespec ts;
	ts.tv_sec = time(nullptr);
	ts.tv_nsec = 0;

	assert(cnt == 1);

	t->flags = (int) SampleFlags::HAS_TS_ORIGIN | (int) SampleFlags::HAS_DATA | (int) SampleFlags::HAS_SEQUENCE;
	t->ts.origin = ts;
	t->sequence = sequence++;
	t->length = 1;
	t->signals = in.signals;
	sleep(10);
	for (unsigned i = 0; i < t->length; i++) {
		t->data[i].f = 1.0;

	}

	return 1;
}

static char n[] = "smu";
static char d[] = "SMU data acquisition";
static NodePlugin<SMUNode, n , d, (int) NodeFactory::Flags::SUPPORTS_READ | (int) NodeFactory::Flags::SUPPORTS_POLL> p;