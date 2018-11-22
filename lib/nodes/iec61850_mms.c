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


int iec61850_mms_parse(struct node * n, json_t * cfg)
{
    int ret;
    struct iec61850_mms * mms = (struct iec61850_mms *) n->_vd;

    json_error_t err;

    const char * host = NULL;
    const char * domainID = NULL;
    const char * itemID = NULL;

    ret = json_unpack_ex(cfg, &err, 0, "{ s: s, s?: i, s?: s, s?: s }",
            "host", &host,
            "port", &mms->port,
            "domain_ID", &domainID,
            "item_ID", &itemID
            );

    if (ret) {
        jerror (&err, "Failed to parse configuration of node %s", node_name(n));
    }

    mms->host = strdup(host);
    mms->domainID = strdup(domainID);
    mms->itemID = strdup(itemID);

    return ret;
}

char * iec61850_mms_print(struct node * n)
{
    char * buf;
    struct iec61850_mms * mms = (struct iec61850_mms *) n->_vd;

    buf = strf("host = %s, port = %d, domainID = %s, itemID = %s", mms->host, mms->port, mms->domainID, mms->itemID);

    return buf;
}

int iec61850_mms_start(struct node * n)
{
    int ret = 0;
    struct iec61850_mms * mms = (struct iec61850_mms *) n->_vd;

    mms->conn = MmsConnection_create();

    /* Connect to MMS Server */
    MmsError error;
    if (!MmsConnection_connect(mms->conn, &error, mms->host, mms->port))
    {
        return -1;
    }

    //    /* Initialize pool and queue to pass samples between threads */
    //    ret = pool_init(&mms->pool, 1024, SAMPLE_LENGTH(list_length(n->signals)),
    //    if (ret) { return ret; }
    //
    //    ret = queue_signalled_init(&mms->queue, 1024, &memory_hugepage, 0);
    //    if (ret) { return ret; }

    return ret;
}

int iec61850_mms_stop(struct node * n)
{
    int ret = 0;
    //struct iec61850_mms * mms = (struct iec61850_mms *) n->_vd;


    return ret;
}

int iec61850_mms_read(struct node * n, struct sample * smps[], unsigned cnt, unsigned * release)
{
    int pulled;
    struct iec61850_mms * mms = (struct iec61850_mms *) n->_vd;
    struct sample *smpt[cnt];

    pulled = queue_signalled_pull_many(&mms->queue, (void **) smpt, cnt);

    sample_copy_many(smps, smpt, pulled);
    sample_decref_many(smpt, pulled);

    return pulled;
}

int iec61850_mms_write(struct node * n, struct sample * smps[], unsigned cnt, unsigned * release)
{
    return 0;
}

int iec61850_mms_destroy(struct node * n)
{
    int ret;
    struct iec61850_mms * mms = (struct iec61850_mms *) n->_vd;

    MmsConnection_destroy(mms->conn);

    ret = pool_destroy(&mms->pool);
    if (ret) { return ret; }

    ret = queue_signalled_destroy(&mms->queue);
    if (ret) { return ret; }


    free(mms->host);
    free(mms->domainID);
    free(mms->itemID);

    return 0;
}

int iec61850_mms_fd(struct node * n)
{
    struct iec61850_mms * mms = (struct iec61850_mms *) n->_vd;

    return queue_signalled_fd(&mms->queue);
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
        .fd             = iec61850_mms_fd           /* done */
    }
};

REGISTER_PLUGIN(&p);
LIST_INIT_STATIC(&p.node.instances);

