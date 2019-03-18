CC = gcc
client = clientDNSIpUpload
server = serverDNSIpReceive


all:$(client) $(server)

$(client):$(client).c
	$(CC) -o $(client) $(client).c -lpthread
$(server):$(server).c
	$(CC) -o $(server) $(server).c -lpthread

clean:
	rm $(client) $(server) 
