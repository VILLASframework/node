/** Moving window / Recursive DFT implementation based on HLS
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Steffen Vogel
 * @license GNU General Public License (version 3)
 *
 * VILLASfpga
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

#include "log.h"
#include "log_config.h"
#include "plugin.h"

#include "fpga/ip.h"
#include "fpga/card.h"
#include "fpga/ips/dft.h"

int dft_parse(struct fpga_ip *c, json_t *cfg)
{
	struct dft *dft = (struct dft *) c->_vd;

	int ret;

	json_t *json_harms;
	json_error_t err;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: i, s: i, s: o }",
		"period", &dft->period,
		"decimation", &dft->decimation,
		"harmonics", &json_harms
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of FPGA IP '%s'", c->name);

	if (!json_is_array(json_harms))
		error("DFT IP core requires 'harmonics' to be an array of integers!");

	dft->num_harmonics = json_array_size(json_harms);
	if (dft->num_harmonics <= 0)
		error("DFT IP core requires 'harmonics' to contain at least 1 value!");

	dft->fharmonics = alloc(sizeof(float) * dft->num_harmonics);

	size_t index;
	json_t *json_harm;
	json_array_foreach(json_harms, index, json_harm) {
		if (!json_is_real(json_harm))
			error("DFT IP core requires all 'harmonics' values to be of floating point type");

		dft->fharmonics[index] = (float) json_number_value(json_harm) / dft->period;
	}

	return 0;
}

int dft_start(struct fpga_ip *c)
{
	int ret;

	struct fpga_card *f = c->card;
	struct dft *dft = (struct dft *) c->_vd;

	XHls_dft *xdft = &dft->inst;
	XHls_dft_Config xdft_cfg = {
		.Ctrl_BaseAddress = (uintptr_t) f->map + c->baseaddr
	};

	ret = XHls_dft_CfgInitialize(xdft, &xdft_cfg);
	if (ret != XST_SUCCESS)
		return ret;

	int max_harmonics = XHls_dft_Get_fharmonics_TotalBytes(xdft) / sizeof(dft->fharmonics[0]);

	if (dft->num_harmonics > max_harmonics)
		error("DFT IP core supports a maximum of %u harmonics", max_harmonics);

	XHls_dft_Set_num_harmonics_V(xdft, dft->num_harmonics);

	XHls_dft_Set_decimation_V(xdft, dft->decimation);

	memcpy((void *) (uintptr_t) XHls_dft_Get_fharmonics_BaseAddress(xdft), dft->fharmonics, dft->num_harmonics * sizeof(dft->fharmonics[0]));

	XHls_dft_EnableAutoRestart(xdft);
	XHls_dft_Start(xdft);

	return 0;
}

int dft_stop(struct fpga_ip *c)
{
	struct dft *dft = (struct dft *) c->_vd;

	XHls_dft *xdft = &dft->inst;

	XHls_dft_DisableAutoRestart(xdft);

	return 0;
}

int dft_destroy(struct fpga_ip *c)
{
	struct dft *dft = (struct dft *) c->_vd;

	if (dft->fharmonics) {
		free(dft->fharmonics);
		dft->fharmonics = NULL;
	}

	return 0;
}

static struct plugin p = {
	.name		= "Discrete Fourier Transform",
	.description	= "Perfom Discrete Fourier Transforms with variable number of harmonics on the FPGA",
	.type		= PLUGIN_TYPE_FPGA_IP,
	.ip		= {
		.vlnv	= { "acs.eonerc.rwth-aachen.de", "hls", "hls_dft", NULL },
		.type	= FPGA_IP_TYPE_MATH,
		.start	= dft_start,
		.stop	= dft_stop,
		.destroy = dft_destroy,
		.parse	= dft_parse,
		.size	= sizeof(struct dft)
	}
};

REGISTER_PLUGIN(&p)
