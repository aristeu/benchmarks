CFLAGS += -Wall -g

all: ipc_sem tty_flush_hangup

stresser_objects := stresser.o

ipc_sem: ipc_sem_rmid.c $(stresser_objects)
	gcc -lpthread -Wall -o $@ $< $(stresser_objects)

tty_flush_hangup: tty_flush_hangup.c $(stresser_objects)
	gcc -lpthread -Wall -o $@ $< $(stresser_objects)

clean:
	rm -f *.o core ipc_sem_rmid
