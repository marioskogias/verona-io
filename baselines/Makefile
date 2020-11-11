.PHONY: clean

LDFLAGS = -pthread
OBJ_C = server.o common.o


static: $(OBJ_C)
	$(CC) -o $@ $^ $(LDFLAGS)

floating: CFLAGS += -DFLOATING
floating: clean $(OBJ_C) 
	$(CC) -o $@ $(OBJ_C) $(LDFLAGS)

clean:
	rm -f *.o
