/** Node type: IEC60870-5-104 client
 *
 * @author Manuel Pitz <manue.pitz@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <algorithm>

#include <villas/node_compat.hpp>
#include <villas/nodes/iec60870_client.hpp>
#include <villas/utils.hpp>
#include <villas/sample.hpp>
#include <villas/super_node.hpp>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;
using namespace villas::node::iec60870_client;
using namespace std::literals::chrono_literals;


// new value
float val;
bool newValue;
std::mutex new_val_mutex;


/* Connection event handler */
static void
connectionHandler (void* parameter, CS104_Connection connection, CS104_ConnectionEvent event)
{
    switch (event) {
    case CS104_CONNECTION_FAILED:
        printf("Connection failed\n");
        break;
    case CS104_CONNECTION_OPENED:
        printf("Connection established\n");
        break;
    case CS104_CONNECTION_CLOSED:
        printf("Connection closed\n");
        break;
    case CS104_CONNECTION_STARTDT_CON_RECEIVED:
        printf("Received STARTDT_CON\n");
        break;
    case CS104_CONNECTION_STOPDT_CON_RECEIVED:
        printf("Received STOPDT_CON\n");
        break;
    }
}

/*
 * CS101_ASDUReceivedHandler implementation
 *
 * For CS104 the address parameter has to be ignored
 */
static bool
asduReceivedHandler (void* parameter, int address, CS101_ASDU asdu)
{
    printf("RECVD ASDU type: %s(%i) elements: %i\n",
            TypeID_toString(CS101_ASDU_getTypeID(asdu)),
            CS101_ASDU_getTypeID(asdu),
            CS101_ASDU_getNumberOfElements(asdu));

    if (CS101_ASDU_getTypeID(asdu) == M_ME_TE_1) {

        printf("  measured scaled values with CP56Time2a timestamp:\n");

        int i;

        for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {

            MeasuredValueScaledWithCP56Time2a io =
                    (MeasuredValueScaledWithCP56Time2a) CS101_ASDU_getElement(asdu, i);

            printf("    IOA: %i value: %i\n",
                    InformationObject_getObjectAddress((InformationObject) io),
                    MeasuredValueScaled_getValue((MeasuredValueScaled) io)
            );

            MeasuredValueScaledWithCP56Time2a_destroy(io);
        }
    }
    else if (CS101_ASDU_getTypeID(asdu) == M_SP_NA_1) {
        printf("  single point information:\n");

        int i;

        for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {

            SinglePointInformation io =
                    (SinglePointInformation) CS101_ASDU_getElement(asdu, i);

            printf("    IOA: %i value: %i\n",
                    InformationObject_getObjectAddress((InformationObject) io),
                    SinglePointInformation_getValue((SinglePointInformation) io)
            );

            SinglePointInformation_destroy(io);
        }
    }
    else if (CS101_ASDU_getTypeID(asdu) == M_ME_NC_1) {
        printf("  single point information:\n");

        int i;

        for (i = 0; i < CS101_ASDU_getNumberOfElements(asdu); i++) {

            MeasuredValueShort io =
                    (MeasuredValueShort) CS101_ASDU_getElement(asdu, i);

            printf("    IOA: %i value: %f\n",
                    InformationObject_getObjectAddress((InformationObject) io),
                    MeasuredValueShort_getValue((MeasuredValueShort) io)
            );
			new_val_mutex.lock();
			if (!newValue) {
				val = MeasuredValueShort_getValue((MeasuredValueShort) io);
				newValue = true;
			} else
				printf("Missed value");
			new_val_mutex.unlock();			

            MeasuredValueShort_destroy(io);
        }
    }
    return true;
}

int IEC60870_client::_read(struct Sample * smps[], unsigned cnt)
{
	cnt = 0;
	new_val_mutex.lock();
	if (newValue) {

		struct Sample *t = smps[0];

		t->signals = in.signals;
		t->length = 1;

		for (unsigned i = 0; i < t->length; i++) {
			t->data[i].f = val;
		}

		cnt = 1;

		newValue = false;
	}
	new_val_mutex.unlock();


	return cnt;
}


int IEC60870_client::_write(Sample *samples[], unsigned sample_count)
{
	for ( unsigned i = 0; i < sample_count; i++) {
		InformationObject sc = (InformationObject)
				SetpointCommandShort_create(NULL, 8000, samples[i]->data[0].f, false, 0);
		CS104_Connection_sendProcessCommandEx(server.conHandle, CS101_COT_ACTIVATION, server.common_address, sc);
	}

	return sample_count;
}

IEC60870_client::IEC60870_client(const std::string &name) :
	Node(name)
{
	newValue = false;
	val = 0;
}

IEC60870_client::~IEC60870_client()
{

}

int IEC60870_client::parse(json_t *json, const uuid_t sn_uuid)
{
	int ret = Node::parse(json, sn_uuid);
	if (ret)
		return ret;

	json_error_t err;
	auto signals = getOutputSignals();

	// json_t *out_json = nullptr;
	char const *address = nullptr;

	ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: i, s?: i }",
		"address", &address,
		"port", &server.port,
		"ca", &server.common_address
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-iec60870-5-104");

	if (address)
		server.address = address;

	// json_t *signals_json = nullptr;
	// int duplicate_ioa_is_sequence = false;

	// if (signals_json) {
	// 	json_t *signal_json;
	// 	size_t i;
	// 	std::optional<ASDUData> last_data = std::nullopt;

	// 	json_array_foreach(signals_json, i, signal_json) {
	// 		auto signal = signals ? signals->getByIndex(i) : Signal::Ptr{};
	// 		auto asdu_data = ASDUData::parse(signal_json, last_data, duplicate_ioa_is_sequence);
	// 		last_data = asdu_data;
	// 		SignalData initial_value;

	// 		if (signal) {
	// 			if (signal->type != asdu_data.signalType()) {
	// 				throw RuntimeError("Type mismatch! Expected type {} for signal {}, but found {}",
	// 					signalTypeToString(asdu_data.signalType()),
	// 					signal->name,
	// 					signalTypeToString(signal->type)
	// 				);
	// 			}

	// 			switch (signal->type) {
	// 				case SignalType::BOOLEAN:
	// 					initial_value.b = false;
	// 					break;

	// 				case SignalType::INTEGER:
	// 					initial_value.i = 0;
	// 					break;

	// 				case SignalType::FLOAT:
	// 					initial_value.f = 0;
	// 					break;

	// 				default:
	// 					assert(!"unreachable");
	// 			}
	// 		} else
	// 			initial_value.f = 0.0;

	// 		output.mapping.push_back(asdu_data);
	// 		output.last_values.push_back(initial_value);
	// 	}
	// }

	// for (auto const &asdu_data : output.mapping) {
	// 	if (std::find(begin(output.asdu_types), end(output.asdu_types), asdu_data.type()) == end(output.asdu_types))
	// 		output.asdu_types.push_back(asdu_data.type());
	// }

	return 0;
}

int IEC60870_client::start()
{
    server.conHandle = CS104_Connection_create(server.address.c_str(), server.port);

    CS104_Connection_setConnectionHandler(server.conHandle, connectionHandler, NULL);
    CS104_Connection_setASDUReceivedHandler(server.conHandle, asduReceivedHandler, NULL);

    /* uncomment to log messages */
    //CS104_Connection_setRawMessageHandler(con, rawMessageHandler, NULL);

    if (CS104_Connection_connect(server.conHandle)) {
        printf("Connected!\n");

        CS104_Connection_sendStartDT(server.conHandle);

        sleep(1);

        CS104_Connection_sendInterrogationCommand(server.conHandle, CS101_COT_ACTIVATION, 1, IEC60870_QOI_STATION);

        sleep(1);
	}

	return Node::start();
}

int IEC60870_client::stop()
{

	CS104_Connection_destroy(server.conHandle);

	return Node::stop();
}

static char name[] = "iec60870-5-104-client";
static char description[] = "Cleint side for IEC60870-104";
static NodePlugin<IEC60870_client, name, description, (int) NodeFactory::Flags::SUPPORTS_WRITE, 1> p;
