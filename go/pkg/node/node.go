/** Wrapper for using libvillas in Go applications.
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

package node

// #cgo LDFLAGS: -lvillas
// #include <villas/node.h>
import "C"
import (
	"encoding/json"
	"fmt"
	"log"
	"unsafe"

	"github.com/google/uuid"

	"git.rwth-aachen.de/acs/public/villas/node/go/pkg"
	"git.rwth-aachen.de/acs/public/villas/node/go/pkg/errors"
)

const MAX_SIGNALS = 64
const NUM_HUGEPAGES = 100

type Node struct {
	inst *C.vnode
}

func IsValidNodeNname(name string) bool {
	return bool(C.node_is_valid_name(C.CString(name)))
}

func NewNode(cfg interface{}, sn_uuid uuid.UUID) (*Node, error) {
	if js, err := json.Marshal(cfg); err != nil {
		return nil, fmt.Errorf("failed to serialize config: %w", err)
	} else {
		log.Printf("Config = %s\n", js)
		n := C.node_new(C.CString(string(js)), C.CString(sn_uuid.String()))
		return &Node{n}, nil
	}
}

func (n *Node) Prepare() error {
	return errors.IntToError(int(C.node_prepare(n.inst)))
}

func (n *Node) Check() error {
	return errors.IntToError(int(C.node_check(n.inst)))
}

func (n *Node) Start() error {
	return errors.IntToError(int(C.node_start(n.inst)))
}

func (n *Node) Stop() error {
	return errors.IntToError(int(C.node_stop(n.inst)))
}

func (n *Node) Pause() error {
	return errors.IntToError(int(C.node_pause(n.inst)))
}

func (n *Node) Resume() error {
	return errors.IntToError(int(C.node_resume(n.inst)))
}

func (n *Node) Restart() error {
	return errors.IntToError(int(C.node_restart(n.inst)))
}

func (n *Node) Close() error {
	return errors.IntToError(int(C.node_destroy(n.inst)))
}

func (n *Node) Reverse() error {
	return errors.IntToError(int(C.node_reverse(n.inst)))
}

func (n *Node) Read(cnt int) []Sample {
	var csmps = make([]*C.vsample, cnt)

	for i := 0; i < cnt; i++ {
		csmps[i] = C.sample_alloc(MAX_SIGNALS)
	}

	read := int(C.node_read(n.inst, (**C.vsample)(unsafe.Pointer(&csmps[0])), C.uint(cnt)))

	var smps = make([]Sample, read)
	for i := 0; i < read; i++ {
		smps[i].FromC(csmps[i])
		C.sample_decref(csmps[i])
	}

	return smps
}

func (n *Node) Write(smps []Sample) int {
	cnt := len(smps)
	var csmps = make([]*C.vsample, cnt)

	for i := 0; i < cnt; i++ {
		csmps[i] = smps[i].ToC()
	}

	return int(C.node_write(n.inst, (**C.vsample)(unsafe.Pointer(&csmps[0])), C.uint(cnt)))
}

func (n *Node) PollFDs() []int {
	var cfds [16]C.int
	cnt := int(C.node_poll_fds(n.inst, (*C.int)(unsafe.Pointer(&cfds))))

	var fds = make([]int, cnt)
	for i := 0; i < cnt; i++ {
		fds[i] = int(cfds[i])
	}

	return fds
}

func (n *Node) NetemFDs() []int {
	var cfds [16]C.int
	cnt := int(C.node_netem_fds(n.inst, (*C.int)(unsafe.Pointer(&cfds))))

	var fds = make([]int, cnt)
	for i := 0; i < cnt; i++ {
		fds[i] = int(cfds[i])
	}

	return fds
}

func (n *Node) IsEnabled() bool {
	return bool(C.node_is_enabled(n.inst))
}

func (n *Node) Name() string {
	return C.GoString(C.node_name(n.inst))
}

func (n *Node) NameShort() string {
	return C.GoString(C.node_name_short(n.inst))
}

func (n *Node) NameFull() string {
	return C.GoString(C.node_name_full(n.inst))
}

func (n *Node) Details() string {
	return C.GoString(C.node_details(n.inst))
}

func (n *Node) InputSignalsMaxCount() uint {
	return uint(C.node_input_signals_max_cnt(n.inst))
}

func (n *Node) OutputSignalsMaxCount() uint {
	return uint(C.node_output_signals_max_cnt(n.inst))
}

func (n *Node) ToJSON() string {
	json_str := C.node_to_json_str(n.inst)
	return C.GoString(json_str)
}

func init() {
	C.memory_init(NUM_HUGEPAGES)
}

type Sample pkg.Sample

func (s *Sample) ToC() *C.vsample {
	return C.sample_pack(
		C.uint(s.Sequence),
		&C.struct_timespec{
			tv_sec:  C.long(s.Timestamps.Origin[0]),
			tv_nsec: C.long(s.Timestamps.Origin[1]),
		},
		&C.struct_timespec{
			tv_sec:  C.long(s.Timestamps.Received[0]),
			tv_nsec: C.long(s.Timestamps.Received[1]),
		},
		C.uint(len(s.Data)),
		(*C.double)(unsafe.Pointer(&s.Data[0])),
	)
}

func (s *Sample) FromC(c *C.vsample) {
	var tsOrigin C.struct_timespec
	var tsReceived C.struct_timespec

	len := C.sample_length(c)

	s.Data = make([]float64, uint(len))

	C.sample_unpack(c,
		(*C.uint)(unsafe.Pointer(&s.Sequence)),
		&tsOrigin,
		&tsReceived,
		(*C.int)(unsafe.Pointer(&s.Flags)),
		&len,
		(*C.double)(unsafe.Pointer(&s.Data[0])),
	)

	s.Timestamps.Origin = pkg.Timestamp{int64(tsOrigin.tv_sec), int64(tsOrigin.tv_nsec)}
	s.Timestamps.Received = pkg.Timestamp{int64(tsReceived.tv_sec), int64(tsReceived.tv_nsec)}
}
