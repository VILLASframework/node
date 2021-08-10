#include <stdio.h>

#include "test.h"

int main() {
	void *ptr = NewTest2(55);

	TestBla(ptr);
	TestBla(ptr);
	TestBla(ptr);
	int r = TestBla(ptr);

	printf("final %d\n", r);
}
