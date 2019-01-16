/** Node type: IEC 61850-8-1 (MMS)
 *
 * @author Iris Koester <ikoester@eonerc.rwth-aachen.de>
 * @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include <villas/nodes/iec61850_mms.h>
#include <villas/plugin.h>
#include <villas/log.h>


int iec61850_mms_parse_ids(json_t * mms_ids, struct list * domainIDs, struct list * itemIDs)
{
    int ret = 0, totalsize = 0;
    const char * domainID;
    const char * itemID;

    ret = list_init(domainIDs);
    if (ret)
        return ret;

    ret = list_init(itemIDs);
    if (ret)
        return ret;

    size_t i = 0;
    json_t * mms_id;
    json_error_t err;
    json_array_foreach(mms_ids, i, mms_id) {

        ret = json_unpack_ex(mms_id, &err, 0, "{s: s, s: s}",
                "domain_ID", &domainID,
                "item_ID", &itemID
                );

        if (ret) 
            warn("Failed to parse configuration while reading domain and item ids");


        list_push(domainIDs, (void *) domainID);
        list_push(itemIDs, (void *) itemID);

        totalsize++;
    }
    return totalsize;
}

int iec61850_mms_parse(struct node * n, json_t * cfg)
{
    int ret;
    struct iec61850_mms * mms = (struct iec61850_mms *) n->_vd;

    json_error_t err;

    const char * host = NULL;
    json_t * json_in = NULL;
    json_t * json_signals = NULL;
    json_t * json_mms_ids = NULL;

    ret = json_unpack_ex(cfg, &err, 0, "{ s: o, s: s, s: i, s: i, s: s, s: s }",
            "in", &json_in,
            "host", &host,
            "port", &mms->port,
            "rate", &mms->rate
            );

    if (ret)
        jerror (&err, "Failed to parse configuration of node %s", node_name(n));

    if (json_in) {

        ret = json_unpack_ex(json_in, &err, 0, "{ s: o, s: o }",
                "iec_types", &json_signals,
                "mms_ids", &json_mms_ids
                );
        if (ret)
            jerror(&err, "Failed to parse configuration of node %s", node_name(n));

        ret = iec61850_parse_signals(json_signals, &mms->in.iecTypeList, &n->signals);
        if (ret <= 0)
            error("Failed to parse setting 'iecList' of node %s", node_name(n));

        mms->in.totalsize = ret;

        int length_mms_ids = iec61850_mms_parse_ids(json_mms_ids, &mms->in.domain_ids, &mms->in.item_ids);

        if (length_mms_ids == -1)
            error("Configuration error in node '%s': json error while parsing", node_name(n));
        else if (ret != length_mms_ids)  // length of the lists is not the same
            error("Configuration error in node '%s': one set of 'mms_ids' should match one value of 'iecTypes'", node_name(n));
    }

    mms->host = strdup(host);

    return ret;
}

char * iec61850_mms_print(struct node * n)
{
    char * buf;
    struct iec61850_mms * mms = (struct iec61850_mms *) n->_vd;

    buf = strf("host = %s, port = %d, domainID = %s, itemID = %s", mms->host, mms->port, mms->domainID, mms->itemID);

    return buf;
}

// create connection to MMS server
int iec61850_mms_start(struct node * n)
{
    struct iec61850_mms * mms = (struct iec61850_mms *) n->_vd;

    mms->conn = MmsConnection_create();

    /* Connect to MMS Server */
    MmsError err;
    if (!MmsConnection_connect(mms->conn, &err, mms->host, mms->port))
    {
        error("Cannot connect to MMS server: %s:%d", mms->host, mms->port);
    }

    // setup task
    int ret = task_init(&mms->task, mms->rate, CLOCK_MONOTONIC);
    if (ret)
        return ret;

    return 0;
}

// destroy connection to MMS server
int iec61850_mms_stop(struct node * n)
{
    struct iec61850_mms * mms = (struct iec61850_mms *) n->_vd;
    MmsConnection_destroy(mms->conn); // doesn't have return value
    return 0;
}

// read from MMS Server
int iec61850_mms_read(struct node * n, struct sample * smps[], unsigned cnt, unsigned * release)
{
    struct iec61850_mms * mms = (struct iec61850_mms *) n->_vd;
    struct sample * smp = smps[0];
    //struct timespec time;


    // read value from MMS server
    MmsError error;
    MmsValue * mms_val;

    task_wait(&mms->task);

    // TODO: timestamp von MMS server?
    //time = time_now();


    smp->flags = SAMPLE_HAS_DATA | SAMPLE_HAS_SEQUENCE;
    smp->length = 0;


    const char * domainID;
    const char * itemID;

    for (size_t j = 0; j < list_length(&mms->in.iecTypeList); j++)
    {
        // get MMS Value from server
        domainID = (const char *) list_at(&mms->in.domain_ids, j);
        itemID = (const char *) list_at(&mms->in.item_ids, j);

        mms_val = MmsConnection_readVariable(mms->conn, &error, domainID, itemID);

        if (mms_val == NULL) {
            warn("Reading MMS value from server failed"); // TODO: elaborate
        }

        // convert result according data type
        struct iec61850_type_descriptor * td = (struct iec61850_type_descriptor *) list_at(&mms->in.iecTypeList, j);

        switch (td->type) {
            case IEC61850_TYPE_INT32:   smp->data[j].i = MmsValue_toInt32(mms_val); break;
            case IEC61850_TYPE_INT32U:  smp->data[j].i = MmsValue_toUint32(mms_val); break;
            case IEC61850_TYPE_FLOAT32: smp->data[j].f = MmsValue_toFloat(mms_val); break;
            case IEC61850_TYPE_FLOAT64: smp->data[j].f = MmsValue_toDouble(mms_val); break;
            default: { }
        }

        smp->length++;
    }




    MmsValue_delete(mms_val);

    return 1;
}


/*
// read from MMS Server
int iec61850_mms_read(struct node * n, struct sample * smps[], unsigned cnt, unsigned * release)
{
struct iec61850_mms * mms = (struct iec61850_mms *) n->_vd;
//struct sample *smpt[cnt];


// read value from MMS server
MmsError error;
MmsValue * mms_val = MmsConnection_readVariable(mms->conn, &error, mms->domainID, mms->itemID);

if (mms_val == NULL) {
warn("Reading MMS value from server failed");
return -1; // TODO: improve this
}


// create sample and write MMS value into sample
struct sample * smp;
smp = sample_alloc_mem(1); // allocate sample array of capacity 1 and length 0
if (!smp) {
warn("Could not allocate sample in %s", node_name(n));
return -1;
}

smp->flags = SAMPLE_HAS_DATA;

if ( strcmp(mms->iecType, "int32") ) {
smp->data[0].i = MmsValue_toInt32(mms_val);
} else { // TODO
}

 *smps = smp;

 MmsValue_delete(mms_val);

 return 1;
 } */

// not neccessary for now
int iec61850_mms_write(struct node * n, struct sample * smps[], unsigned cnt, unsigned * release)
{
    struct iec61850_mms * mms = (struct iec61850_mms *) n->_vd;

    if(!mms->out.enabled) { return 0; }


    // TODO: check mqtt




    return 0;
}

int iec61850_mms_destroy(struct node * n)
{
    int ret;
    struct iec61850_mms * mms = (struct iec61850_mms *) n->_vd;

    MmsConnection_destroy(mms->conn);

    //ret = pool_destroy(&mms->pool);
    //if (ret) { return ret; }

    ret = queue_signalled_destroy(&mms->in.queue);
    if (ret) { return ret; }


    free(mms->host);
    free(mms->domainID);
    free(mms->itemID);

    return 0;
}

int iec61850_mms_fd(struct node * n)
{
    struct iec61850_mms * mms = (struct iec61850_mms *) n->_vd;

    return task_fd(&mms->task);
}


static struct plugin p = {
    .name           = "iec61850-8-1",
    .description    = "IEC 61850-8-1 (MMS)",
    .type           = PLUGIN_TYPE_NODE,
    .node           = {
        .vectorize      = 0, /* unlimited */
        .size           = sizeof(struct iec61850_mms),
        .type.start     = iec61850_type_start,   // TODO
        .type.stop      = iec61850_type_stop,    // TODO
        .parse          = iec61850_mms_parse,       /* done */
        .print          = iec61850_mms_print,       /* done */
        .start          = iec61850_mms_start,       /* done */
        .stop           = iec61850_mms_stop,        // TODO
        .destroy        = iec61850_mms_destroy,     /* done */
        .read           = iec61850_mms_read,        /* done */
        .write          = iec61850_mms_write,       // TODO: Rumpf erstellt
        .fd             = iec61850_mms_fd
    }
};

REGISTER_PLUGIN(&p);
LIST_INIT_STATIC(&p.node.instances);

