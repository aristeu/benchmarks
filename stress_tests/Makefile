CFLAGS += -Wall -g

all: ipc_sem

stresser_objects := stresser.o

ipc_sem: ipc_sem_rmid.c $(stresser_objects)
	gcc -lpthread -Wall -o $@ $< $(stresser_objects)

clean:
	rm -f *.o core ipc_sem_rmid
