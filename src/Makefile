server: server.o http.o logger.o
	gcc server.o http.o logger.o -o server
	
server.o: server.c http.h logger.h
	gcc -c server.c
http.o: http.c http.h logger.h
	gcc -c http.c
logger.o: logger.c logger.h
	gcc -c logger.c
	
clean:
	rm *.o
