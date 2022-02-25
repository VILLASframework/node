package main

/*
 */
import "C"

import (
	"unsafe"

	node "git.rwth-aachen.de/acs/public/villas/node/pkg"
	gopointer "github.com/mattn/go-pointer"

	_ "git.rwth-aachen.de/acs/public/villas/node/pkg/nodes/loopback"
	_ "git.rwth-aachen.de/acs/public/villas/node/pkg/nodes/webrtc"
)

func main() {}

//export GoNodeTypes
func GoNodeTypes() (int, **C.char) {
	var nodeTypes = node.NodeTypes()
	var n = len(nodeTypes)

	types := (**C.char)(C.malloc(C.size_t(n) * C.size_t(unsafe.Sizeof(uintptr(0)))))
	slice := (*[1 << 31]*C.char)(unsafe.Pointer(types))[:n:n]

	i := 0
	for typ, _ := range nodeTypes {
		slice[i] = C.CString(typ)
		i++
	}

	return n, types
}

//export NewGoNode
func NewGoNode(t *C.char) unsafe.Pointer {
	var nodeTypes = node.NodeTypes()

	typ := C.GoString(t)
	ctor, ok := nodeTypes[typ]
	if !ok {
		return unsafe.Pointer(nil)
	}

	node := ctor()

	ptr := gopointer.Save(node)
	return ptr
}

//export GoNodeClose
func GoNodeClose(p unsafe.Pointer) C.int {
	n := gopointer.Restore(p).(node.Node)
	return errorToInt(n.Close())
}

//export GoNodePrepare
func GoNodePrepare(p unsafe.Pointer) C.int {
	n := gopointer.Restore(p).(node.Node)
	return errorToInt(n.Prepare())
}

//export GoNodeParse
func GoNodeParse(p unsafe.Pointer, c *C.char) C.int {
	scfg := C.GoString(c)
	bcfg := []byte(scfg)

	n := gopointer.Restore(p).(node.Node)
	return errorToInt(n.Parse(bcfg))
}

//export GoNodeCheck
func GoNodeCheck(p unsafe.Pointer) C.int {
	n := gopointer.Restore(p).(node.Node)
	return errorToInt(n.Check())
}

//export GoNodeStart
func GoNodeStart(p unsafe.Pointer) C.int {
	n := gopointer.Restore(p).(node.Node)
	return errorToInt(n.Start())
}

//export GoNodeStop
func GoNodeStop(p unsafe.Pointer) C.int {
	n := gopointer.Restore(p).(node.Node)
	return errorToInt(n.Stop())
}

//export GoNodePause
func GoNodePause(p unsafe.Pointer) C.int {
	n := gopointer.Restore(p).(node.Node)
	return errorToInt(n.Pause())
}

//export GoNodeResume
func GoNodeResume(p unsafe.Pointer) C.int {
	n := gopointer.Restore(p).(node.Node)
	return errorToInt(n.Resume())
}

//export GoNodeRestart
func GoNodeRestart(p unsafe.Pointer) C.int {
	n := gopointer.Restore(p).(node.Node)
	return errorToInt(n.Restart())
}

//export GoNodeRead
func GoNodeRead(p unsafe.Pointer) ([]byte, C.int) {
	n := gopointer.Restore(p).(node.Node)

	d, err := n.Read()
	if err != nil {
		return nil, errorToInt(err)
	}

	return d, 0
}

//export GoNodeWrite
func GoNodeWrite(p unsafe.Pointer, data []byte) C.int {
	n := gopointer.Restore(p).(node.Node)
	return errorToInt(n.Write(data))
}

//export GoNodeReverse
func GoNodeReverse(p unsafe.Pointer) C.int {
	n := gopointer.Restore(p).(node.Node)
	return errorToInt(n.Reverse())
}

//export GoNodeGetPollFDs
func GoNodeGetPollFDs(p unsafe.Pointer) ([]int, C.int) {
	n := gopointer.Restore(p).(node.Node)
	f, err := n.GetPollFDs()
	if err != nil {
		return nil, errorToInt(err)
	}

	return f, 0
}

//export GoNodeGetNetemFDs
func GoNodeGetNetemFDs(p unsafe.Pointer) ([]int, C.int) {
	n := gopointer.Restore(p).(node.Node)
	f, err := n.GetNetemFDs()
	if err != nil {
		return nil, errorToInt(err)
	}

	return f, 0
}

//export GoNodeDetails
func GoNodeDetails(p unsafe.Pointer) *C.char {
	n := gopointer.Restore(p).(node.Node)
	d := n.Details()
	return C.CString(d)
}
