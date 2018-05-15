#!/usr/bin/env python3
"""
Author:     Daniel Krebs <github@daniel-krebs.net>
Copyright:  2017, Institute for Automation of Complex Power Systems, EONERC
License:    GNU General Public License (version 3)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
"""

from lxml import etree
import zipfile
import sys
import re
import json

whitelist = [
	[ 'xilinx.com', 'ip', 'axi_timer' ],
	[ 'xilinx.com', 'ip', 'axis_switch' ],
	[ 'xilinx.com', 'ip', 'axi_fifo_mm_s' ],
	[ 'xilinx.com', 'ip', 'axi_dma' ],
	[ 'acs.eonerc.rwth-aachen.de', 'user', 'axi_pcie_intc' ],
	[ 'acs.eonerc.rwth-aachen.de', 'user', 'rtds_axis' ],
	[ 'acs.eonerc.rwth-aachen.de', 'hls' ],
	[ 'acs.eonerc.rwth-aachen.de', 'sysgen' ],
	[ 'xilinx.com', 'ip', 'axi_gpio' ],
	[ 'xilinx.com', 'ip', 'axi_bram_ctrl' ],
	[ 'xilinx.com', 'ip', 'axi_pcie' ]
]

# List of VLNI ids of AXI4-Stream infrastructure IP cores which do not alter data
# see PG085 (AXI4-Stream Infrastructure IP Suite v2.2)
axi_converter_whitelist = [
	[ 'xilinx.com', 'ip', 'axis_subset_converter' ],
	[ 'xilinx.com', 'ip', 'axis_clock_converter' ],
	[ 'xilinx.com', 'ip', 'axis_register_slice' ],
	[ 'xilinx.com', 'ip', 'axis_data_fifo' ],
	[ 'xilinx.com', 'ip', 'axis_dwidth_converter' ],
	[ 'xilinx.com', 'ip', 'axis_register_slice' ]
]

opponent = {
	'MASTER' : ('SLAVE', 'TARGET'),
	'SLAVE'  : ('MASTER', 'INITIATOR'),
	'INITIATOR' : ('TARGET', 'SLAVE'),
	'TARGET' : ('INITIATOR', 'MASTER')
}

def port_trace(root, signame, idx):
	pass

def port_find_driver(root, signame):
	pass

def bus_trace(root, busname, type, whitelist):
	module = root.xpath('.//MODULE[.//BUSINTERFACE[@BUSNAME="{}" and (@TYPE="{}" or @TYPE="{}")]]'.format(busname, type[0], type[1]))

	vlnv = module[0].get('VLNV')
	instance = module[0].get('INSTANCE')

	if vlnv_match(vlnv, whitelist):
		return instance, busname
	elif vlnv_match(vlnv, axi_converter_whitelist):
		next_bus = module[0].xpath('.//BUSINTERFACE[@TYPE="{}" or @TYPE="{}"]'.format(opponent[type[0]][0], opponent[type[0]][1]))
		next_busname = next_bus[0].get('BUSNAME')

		return bus_trace(root, next_busname, type, whitelist)
	else:
		raise TypeError('Unsupported AXI4-Stream IP core: %s (%s)' % (instance, vlnv))

def vlnv_match(vlnv, whitelist):
	c = vlnv.split(':')

	for w in whitelist:
		if c[:len(w)] == w:
			return True

	return False

def remove_prefix(text, prefix):
	return text[text.startswith(prefix) and len(prefix):]

def sanitize_name(name):
	name = remove_prefix(name, 'S_')
	name = remove_prefix(name, 'M_')
	name = remove_prefix(name, 'AXI_')
	name = remove_prefix(name, 'AXIS_')

	return name

if len(sys.argv) < 2:
	print('Usage: {} path/to/*.hwdef'.format(sys.argv[0]))
	print('       {} path/to/*.xml'.format(sys.argv[0]))
	sys.exit(1)

try:
	# read .hwdef which is actually a zip-file
	zip = zipfile.ZipFile(sys.argv[1], 'r')
	hwh = zip.read('top.hwh')
except:
	f = open(sys.argv[1], 'r')
	hwh = f.read()

# parse .hwh file which is actually XML
try:
	root = etree.XML(hwh)
except:
	print('Bad format of "{}"! Did you choose the right file?'.format(sys.argv[1]))
	sys.exit(1)

ips = {}

# find all whitelisted modules
modules = root.find('.//MODULES')
for module in modules:
	instance = module.get('INSTANCE')
	vlnv = module.get('VLNV')

	# Ignroing unkown
	if not vlnv_match(vlnv, whitelist):
		continue

	ips[instance] = {
		'vlnv' : vlnv
	}

	# populate memory view
	mmap = module.find('.//MEMORYMAP')
	if not mmap:
		continue

	mem = ips[instance].setdefault('memory-view', {})
	for mrange in mmap:
		mem_interface = mrange.get('MASTERBUSINTERFACE')
		mem_instance = mrange.get('INSTANCE')
		mem_block = mrange.get('ADDRESSBLOCK')

		_interface = mem.setdefault(mem_interface, {})
		_instance = _interface.setdefault(mem_instance, {})
		_block = _instance.setdefault(mem_block, {})

		_block['baseaddr'] = int(mrange.get('BASEVALUE'), 16)
		_block['highaddr'] = int(mrange.get('HIGHVALUE'), 16)
		_block['size'] = _block['highaddr'] - _block['baseaddr'] + 1


# find AXI-Stream switch port mapping
switch = root.find('.//MODULE[@MODTYPE="axis_switch"]')
busifs = switch.find('.//BUSINTERFACES')
switch_ports = 0
for busif in busifs:
	if busif.get('VLNV') != 'xilinx.com:interface:axis:1.0':
		continue

	switch_ports += 1

	busname = busif.get('BUSNAME')
	name = busif.get('NAME')
	type = busif.get('TYPE')

	r = re.compile('(M|S)([0-9]+)_AXIS')
	m = r.search(name)

	port = int(m.group(2))

	ep, busname_ep = bus_trace(root, busname, opponent[type], whitelist)
	if ep in ips:

		ports = ips[ep].setdefault('ports', [])
		ports.append({
			'role': opponent[type][0].lower(),
			'target': '{}:{}'.format(switch.get('INSTANCE'), port)
		})

		module_ep = root.find('.//MODULE[@INSTANCE="{}"]'.format(ep))
		busif_ep = module_ep.find('.//BUSINTERFACE[@BUSNAME="{}"]'.format(busname_ep))
		if busif_ep:
			ports[-1]['name'] = sanitize_name(busif_ep.get('NAME'))

# set number of master/slave port pairs for switch
ips[switch.get('INSTANCE')]['num_ports'] = int(switch_ports / 2)


# find Interrupt assignments
intc = root.find('.//MODULE[@MODTYPE="axi_pcie_intc"]')
intr = intc.xpath('.//PORT[@NAME="intr" and @DIR="I"]')[0]
concat = root.xpath('.//MODULE[@MODTYPE="xlconcat" and .//PORT[@SIGNAME="{}" and @DIR="O"]]'.format(intr.get('SIGNAME')))[0]
ports = concat.xpath('.//PORT[@DIR="I"]')

for port in ports:
	name = port.get('NAME')
	signame = port.get('SIGNAME')

	# Skip unconnected IRQs
	if not signame:
		continue

	r = re.compile('In([0-9+])')
	m = r.search(name)

	irq = int(m.group(1))
	ip = root.xpath('.//MODULE[.//PORT[@SIGNAME="{}" and @DIR="O"]]'.format(signame))[0]

	instance = ip.get('INSTANCE')
	vlnv = ip.get('VLNV')
	modtype = ip.get('MODTYPE')

	originators = []

	# follow one level of OR gates merging interrupts (may be generalized later)
	if modtype == 'util_vector_logic':
		logic_op = ip.xpath('.//PARAMETER[@NAME="C_OPERATION"]')[0]
		if logic_op.get('VALUE') == 'or':
			# hardware interrupts sharing the same IRQ at the controller
			ports = ip.xpath('.//PORT[@DIR="I"]')
			for port in ports:
				signame = port.get('SIGNAME')
				ip = root.xpath('.//MODULE[.//PORT[@SIGNAME="{}" and @DIR="O"]]'.format(signame))[0]
				instance = ip.get('INSTANCE')
				originators.append((instance, signame))
	else:
		# consider this instance as originator
		originators.append((instance, signame))


	for instance, signame in originators:
		ip = root.xpath('.//MODULE[.//PORT[@SIGNAME="{}" and @DIR="O"]]'.format(signame))[0]
		port = ip.xpath('.//PORT[@SIGNAME="{}" and @DIR="O"]'.format(signame))[0]
		irqname = port.get('NAME')

		if instance in ips:
			irqs = ips[instance].setdefault('irqs', {})
			irqs[irqname] = '{}:{}'.format(intc.get('INSTANCE'), irq)

# Find BRAM storage depths (size)
brams = root.xpath('.//MODULE[@MODTYPE="axi_bram_ctrl"]')
for bram in brams:
	instance = bram.get('INSTANCE')

	width = bram.find('.//PARAMETER[@NAME="DATA_WIDTH"]').get('VALUE')
	depth = bram.find('.//PARAMETER[@NAME="MEM_DEPTH"]').get('VALUE')
	
	size = int(width) * int(depth) / 8
	
	if instance in ips:
		ips[instance]['size'] = int(size)

pcies = root.xpath('.//MODULE[@MODTYPE="axi_pcie"]')
for pcie in pcies:
	instance = pcie.get('INSTANCE')
	axi_bars = ips[instance].setdefault('axi_bars', {})
	pcie_bars = ips[instance].setdefault('pcie_bars', {})

	for from_bar, to_bar, from_bars in (('AXIBAR', 'PCIEBAR', axi_bars), ('PCIEBAR', 'AXIBAR', pcie_bars)):
		from_bar_num = int(pcie.find('.//PARAMETER[@NAME="C_{}_NUM"]'.format(from_bar)).get('VALUE'))

		for i in range(0, from_bar_num):
			from_bar_to_bar_offset = int(pcie.find('.//PARAMETER[@NAME="C_{}2{}_{}"]'.format(from_bar, to_bar, i)).get('VALUE'), 16)
			from_bars['BAR{}'.format(i)] = { 'translation': from_bar_to_bar_offset }

			if from_bar == 'AXIBAR':
				axi_bar_lo = int(pcie.find('.//PARAMETER[@NAME="C_{}_{}"]'.format(from_bar, i)).get('VALUE'), 16)
				axi_bar_hi = int(pcie.find('.//PARAMETER[@NAME="C_{}_HIGHADDR_{}"]'.format(from_bar, i)).get('VALUE'), 16)
				axi_bar_size = axi_bar_hi - axi_bar_lo + 1

				axi_bar = from_bars['BAR{}'.format(i)]
				axi_bar['baseaddr'] = axi_bar_lo
				axi_bar['highaddr'] = axi_bar_hi
				axi_bar['size'] = axi_bar_size

print(json.dumps(ips, indent=2))
