MODELS = simple_circuit

CFLAGS = -fPIC -DVILLAS -I../../include/villas

all:  $(addsuffix .so,${MODELS})

%.so: %.o
	$(CC) -shared $^ -o $@