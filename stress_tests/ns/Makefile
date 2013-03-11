CFLAGS += -Wall -g

all: stress_namespaces

stress_namespaces_objects := stresser.o
stress_namespaces: stress_namespaces.o $(stress_namespaces_objects)
	gcc -lpthread -Wall -o $@ $< $(stress_namespaces_objects)

clean:
	rm -f *.o core stress_namespaces
