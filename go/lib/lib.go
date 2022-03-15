/** CGo interface for writing node-types in Go
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

package main

// #include <stdint.h>
// #include <villas/nodes/go.h>
// void bridge_go_register_node_factory(_go_register_node_factory_cb cb, _go_plugin_list pl, char *name, char *desc, int flags);
import "C"

import (
	"runtime/cgo"
	"unsafe"

	"git.rwth-aachen.de/acs/public/villas/node/go/pkg/nodes"

	_ "git.rwth-aachen.de/acs/public/villas/node/go/pkg/nodes/example"
	_ "git.rwth-aachen.de/acs/public/villas/node/go/pkg/nodes/loopback"
	_ "git.rwth-aachen.de/acs/public/villas/node/go/pkg/nodes/webrtc"

	"git.rwth-aachen.de/acs/public/villas/node/go/pkg/errors"
)

func main() {}

type NodeType struct {
	nodes.NodeType
}

//export RegisterGoNodeTypes
func RegisterGoNodeTypes(cb C._go_register_node_factory_cb, pl C._go_plugin_list) {
	ntm := nodes.NodeTypes()

	for n, t := range ntm {
		C.bridge_go_register_node_factory(cb, pl, C.CString(n), C.CString(t.Description), C.int(t.Flags))
	}
}

//export NewGoNode
func NewGoNode(t *C.char) C.uintptr_t {
	var nodeTypes = nodes.NodeTypes()

	typ, ok := nodeTypes[C.GoString(t)]
	if !ok {
		return 0
	}

	node := typ.Constructor()
	return C.uintptr_t(cgo.NewHandle(node))
}

//export GoNodeSetLogger
func GoNodeSetLogger(p C.uintptr_t, cb C._go_logger_log_cb, l C._go_logger) {
	h := cgo.Handle(p)
	n := h.Value().(nodes.Node)
	n.SetLogger(NewVillasLogger(cb, l))
}

//export GoNodeClose
func GoNodeClose(p C.uintptr_t) C.int {
	h := cgo.Handle(p)
	n := h.Value().(nodes.Node)
	return C.int(errors.ErrorToInt(n.Close()))
}

//export GoNodePrepare
func GoNodePrepare(p C.uintptr_t) C.int {
	h := cgo.Handle(p)
	n := h.Value().(nodes.Node)
	return C.int(errors.ErrorToInt(n.Prepare()))
}

//export GoNodeParse
func GoNodeParse(p C.uintptr_t, c *C.char) C.int {
	scfg := C.GoString(c)
	bcfg := []byte(scfg)

	h := cgo.Handle(p)
	n := h.Value().(nodes.Node)
	return C.int(errors.ErrorToInt(n.Parse(bcfg)))
}

//export GoNodeCheck
func GoNodeCheck(p C.uintptr_t) C.int {
	h := cgo.Handle(p)
	n := h.Value().(nodes.Node)
	return C.int(errors.ErrorToInt(n.Check()))
}

//export GoNodeStart
func GoNodeStart(p C.uintptr_t) C.int {
	h := cgo.Handle(p)
	n := h.Value().(nodes.Node)
	return C.int(errors.ErrorToInt(n.Start()))
}

//export GoNodeStop
func GoNodeStop(p C.uintptr_t) C.int {
	h := cgo.Handle(p)
	n := h.Value().(nodes.Node)
	return C.int(errors.ErrorToInt(n.Stop()))
}

//export GoNodePause
func GoNodePause(p C.uintptr_t) C.int {
	h := cgo.Handle(p)
	n := h.Value().(nodes.Node)
	return C.int(errors.ErrorToInt(n.Pause()))
}

//export GoNodeResume
func GoNodeResume(p C.uintptr_t) C.int {
	h := cgo.Handle(p)
	n := h.Value().(nodes.Node)
	return C.int(errors.ErrorToInt(n.Resume()))
}

//export GoNodeRestart
func GoNodeRestart(p C.uintptr_t) C.int {
	h := cgo.Handle(p)
	n := h.Value().(nodes.Node)
	return C.int(errors.ErrorToInt(n.Restart()))
}

//export GoNodeRead
func GoNodeRead(p C.uintptr_t, buf *C.char, sz C.int) (C.int, C.int) {
	h := cgo.Handle(p)
	n := h.Value().(nodes.Node)

	src, err := n.Read()
	if err != nil {
		return -1, C.int(errors.ErrorToInt(err))
	}

	// Create slice which is backed by buf
	dst := (*[1 << 30]byte)(unsafe.Pointer(buf))[:sz]
	lsz := copy(dst, src)

	return C.int(lsz), 0
}

//export GoNodeWrite
func GoNodeWrite(p C.uintptr_t, data []byte) C.int {
	h := cgo.Handle(p)
	n := h.Value().(nodes.Node)
	return C.int(errors.ErrorToInt(n.Write(data)))
}

//export GoNodeReverse
func GoNodeReverse(p C.uintptr_t) C.int {
	h := cgo.Handle(p)
	n := h.Value().(nodes.Node)
	return C.int(errors.ErrorToInt(n.Reverse()))
}

//export GoNodeGetPollFDs
func GoNodeGetPollFDs(p C.uintptr_t) ([]int, C.int) {
	h := cgo.Handle(p)
	n := h.Value().(nodes.Node)
	f, err := n.PollFDs()
	if err != nil {
		return nil, C.int(errors.ErrorToInt(err))
	}

	return f, 0
}

//export GoNodeGetNetemFDs
func GoNodeGetNetemFDs(p C.uintptr_t) ([]int, C.int) {
	h := cgo.Handle(p)
	n := h.Value().(nodes.Node)
	f, err := n.NetemFDs()
	if err != nil {
		return nil, C.int(errors.ErrorToInt(err))
	}

	return f, 0
}

//export GoNodeDetails
func GoNodeDetails(p C.uintptr_t) *C.char {
	h := cgo.Handle(p)
	n := h.Value().(nodes.Node)
	d := n.Details()
	return C.CString(d)
}
