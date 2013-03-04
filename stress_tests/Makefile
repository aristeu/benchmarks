all: stress_namespaces

stress_namespaces_objects := stresser.o
stress_namespaces: stress_namespaces.o
	gcc -lpthread -Wall -o $@ $< $(stress_namespaces_objects)
