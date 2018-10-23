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


int iec61850_mms_parse(struct node * n, json_t * cfg)
{
    int ret;
    struct iec61850_mms * mms = (struct iec61850_mms *) n->_vd;

    json_error_t err;

    const char * host = NULL;
    const char * port = NULL;
    const char * domainID = NULL;
    const char * itemID = NULL;

    ret = json_unpack_ex(json, &err, 0, "{ s: s, s?: i, s?: s, s?: s }",
            "host", &host,
            "port", &mms->port,
            "domain_ID", &domainID,
            "item_ID", &itemID
            );

    if (ret) {
        jerror (&err, "Failed to parse configuration of node %si", node_name(n));
    }

    mms->host = strdup(host);
    mms->domainID = strdup(domainID);
    mms->itemID = strdup(itemID);   

}

char * iec61850_mms_print(struct node * n)
{
    char * buf;
    struct iec61850_mms * mms = (struct iec61850 *) n->_vd;

    buf = strf("host = %s, port = %d, domainID = %s, itemID = %s", mms->host, mms->port, mms->domainID, mms->itemID);

    return buf;
}

int iec61850_mms_start(struct node * n)
{
    int ret;
    struct iec61850_mms * mms = (struct iec61850 *) n->_vd;



    return ret;
}

int iec61850_mms_fd(struct node * n)
{
    struct iec61850_mms * mms = (struct iec61850 *) n->_vd;

    return queue_signalled_fd(&mms->queue);
}


static struct plugin p = {
    .name           = "iec61850-8-1",
    .description    = "IEC 61850-8-1 (MMS)",
    .type           = PLUGIN_TYPE_NODE,
    .node           = {
        .vectorize      = 0, /* unlimited */
        .size           = sizeof(struct iec61850_mms),
        .type.start     = iec61850_mms_typestart,   // TODO
        .type.stop      = iec61850_mms_typestop,    // TODO
        .parse          = iec61850_mms_parse,       /* done */
        .print          = iec61850_mms_print,       /* done */
        .start          = iec61850_mms_start,       // TODO
        .stop           = iec61850_mms_stop,        // TODO
        .destroy        = iec61850_mms_destroy,     // TODO
        .read           = iec61850_mms_read,        // TODO
        .write          = iec61850_mms_write,       // TODO
        .fd             = iec61850_mms_fd           // TODO
    }
};

REGISTER_PLUGIN(&p);
