TARGET := server client_arm


all: $(TARGET)

server:ftp_server.o
	gcc $^ -o $@

client_arm:ftp_client.o
	arm-linux-gcc $^ -o $@

ftp_server.o:ftp_server.c
	gcc -c $< -o $@

ftp_client.o:ftp_client.c
	arm-linux-gcc -c $< -o $@

clean:
	-rm  $(TARGET) ftp_server.o ftp_client.o