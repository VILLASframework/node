package main

import (
	"C"
)

func errorToInt(e error) C.int {
	if e == nil {
		return 0
	} else {
		return -1
	}
}
