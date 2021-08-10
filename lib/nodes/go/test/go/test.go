package main

/*

 */
import "C"
import (
	"fmt"
	"unsafe"

	gopointer "github.com/mattn/go-pointer"
)

type Bla interface {
	Bla() int
}

type Test struct {
	Number int
}

type Test2 struct {
	Number int
}

type TestPtr unsafe.Pointer

func (t *Test) Bla() int {
	fmt.Printf("Hello World: %d\n", t.Number)

	t.Number++

	return t.Number
}

func (t *Test2) Bla() int {
	fmt.Printf("Hello World: %d\n", t.Number)

	t.Number++
	t.Number++

	return t.Number
}

//export TestBla
func TestBla(ptr unsafe.Pointer) int {
	t := gopointer.Restore(ptr).(Bla)
	return t.Bla()
}

//export NewTest
func NewTest(number int) unsafe.Pointer {
	t := &Test{
		Number: number,
	}

	ptr := gopointer.Save(t)

	return ptr
}

//export NewTest2
func NewTest2(number int) unsafe.Pointer {
	t := &Test2{
		Number: number,
	}

	ptr := gopointer.Save(t)

	return ptr
}

func main() {
	// We need the main function to make possible
	// CGO compiler to compile the package as C shared library
}
