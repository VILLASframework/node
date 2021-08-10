package node

import "C"
import (
	"time"
	"unsafe"

	gopointer "github.com/mattn/go-pointer"
)

func registerNode(name string, ctor CreateNode) {

}

type Signal struct {
}

type Sample struct {
	Timestamp struct {
		Origin   time.Time
		Received time.Time
	}

	Data []interface{}
}

type Node interface {
	Close() error

	Prepare() error

	Parse(cfg []byte) error

	Check() error

	Start() error
	Stop() error

	Pause() error
	Resume() error
	Restart() error

	Read(smps []Sample) (int, error)
	Write(smps []Sample) (int, error)

	Reverse() error

	GetPollFDs() ([]int, error)
	GetNetemFDs() ([]int, error)

	// GetMemoryType()

	Details() string
}

type CreateNode func() Node

// Wrapper

func intSliceToCArray(s []int) unsafe.Pointer {
	b := (*[1 << 16]byte)(unsafe.Pointer(&s[0]))[0 : len(s)*8 : len(s)*8]
	return C.CBytes(unsafe.Pointer(&b[0]))
}

func errorToInt(err error) int {
	if err == nil {
		return -1
	} else {
		return -22
	}
}

//export NodeClose
func NodeClose(p unsafe.Pointer) int {
	n := gopointer.Restore(p).(Node)
	return errorToInt(n.Close())
}

//export NodePrepare
func NodePrepare(p unsafe.Pointer) int {
	n := gopointer.Restore(p).(Node)
	return errorToInt(n.Prepare())
}

//export NodeParse
func NodeParse(p unsafe.Pointer, c unsafe.Pointer) int {
	cfg := gopointer.Restore(c).([]byte)
	n := gopointer.Restore(p).(Node)
	return errorToInt(n.Parse(cfg))
}

//export NodeCheck
func NodeCheck(p unsafe.Pointer) int {
	n := gopointer.Restore(p).(Node)
	return errorToInt(n.Check())
}

//export NodeStart
func NodeStart(p unsafe.Pointer) int {
	n := gopointer.Restore(p).(Node)
	return errorToInt(n.Start())
}

//export NodeStop
func NodeStop(p unsafe.Pointer) error {
	n := gopointer.Restore(p).(Node)
	return errorToInt(n.Stop())
}

//export NodePause
func NodePause(p unsafe.Pointer) int {
	n := gopointer.Restore(p).(Node)
	return errorToInt(n.Pause())
}

//export NodeResume
func NodeResume(p unsafe.Pointer) int {
	n := gopointer.Restore(p).(Node)
	return errorToInt(n.Resume())
}

//export NodeRestart
func NodeRestart(p unsafe.Pointer) int {
	n := gopointer.Restore(p).(Node)
	return errorToInt(n.Restart())
}

//export NodeRead
func NodeRead(p unsafe.Pointer, smps []Sample) (int, error) {
	n := gopointer.Restore(p).(Node)
	return n.Read(smps)
}

//export NodeWrite
func NodeWrite(p unsafe.Pointer, smps []Sample) (int, int) {
	n := gopointer.Restore(p).(Node)
	r, err := n.Write(smps)
	return r, errorToInt(err)
}

//export NodeReverse
func NodeReverse(p unsafe.Pointer) int {
	n := gopointer.Restore(p).(Node)
	return errorToInt(n.Reverse())
}

//export NodeGetPollFDs
func NodeGetPollFDs(p unsafe.Pointer) (unsafe.Pointer, int) {
	n := gopointer.Restore(p).(Node)
	f, err := n.GetPollFDs()
	if err == nil {
		return intSliceToCArray(f), 0
	} else {
		return nil, errorToInt(err)
	}
}

//export NodeGetNetemFDs
func NodeGetNetemFDs(p unsafe.Pointer) (unsafe.Pointer, int) {
	n := gopointer.Restore(p).(Node)
	f, err := n.GetNetemFDs()
	if err == nil {
		return intSliceToCArray(f), 0
	} else {
		return nil, errorToInt(err)
	}
}

//export NodeDetails
func NodeDetails(p unsafe.Pointer) *C.char {
	n := gopointer.Restore(p).(Node)
	d := n.Details()
	return C.CString(d)
}

// Example

type ExampleNode struct {
	Node
}

func NewExampleNode() Node {
	return &ExampleNode{}
}

func (n *ExampleNode) Close() error {

	return nil
}

func (n *ExampleNode) Prepare() error {

	return nil
}

func (n *ExampleNode) Parse(cfg []byte) error {

	return nil
}

func (n *ExampleNode) Check() error {

	return nil
}

func (n *ExampleNode) Start() error {

	return nil
}

func (n *ExampleNode) Stop() error {

	return nil
}

func (n *ExampleNode) Pause() error {

	return nil
}

func (n *ExampleNode) Resume() error {

	return nil
}

func (n *ExampleNode) Restart() error {

	return nil
}

func (n *ExampleNode) Read(smps []Sample) (int, error) {

	return 0, nil
}

func (n *ExampleNode) Write(smps []Sample) (int, error) {

	return 0, nil
}

func (n *ExampleNode) Reverse() error {

	return nil
}

func (n *ExampleNode) GetPollFDs() ([]int, error) {

	return []int{}, nil
}

func (n *ExampleNode) GetNetemFDs() ([]int, error) {
	return []int{}, nil
}

func (n *ExampleNode) Details() string {

	return ""
}

// init

func init() {
	registerNode("example-go", NewExampleNode)
}
