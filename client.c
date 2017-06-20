/*
 * client.c
 *
 *  Created on: Jun 16, 2017
 *      Author: abhimanyu
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <resolv.h>
#include <arpa/inet.h>



int main(int argc, char **argv){
	int portno=4000;

	if(argc<2)
		printf("usage: ./client PORTNO");
	else
		portno = strtoul(argv[1],NULL,10);

	int sock_desc;
	struct sockaddr_in addr;
	memset(&addr, '\0', sizeof(addr));

	char* cmd = (char *)malloc(512000 * sizeof(char));
	memset(cmd, '\0', (512000 * sizeof(char)));

	char* recv_data = (char *)malloc(512000 * sizeof(char));
	memset(recv_data, '\0', (512000 * sizeof(char)));



	addr.sin_family = AF_INET;
	addr.sin_port = portno;
	if(inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr)<=0){
		fprintf(stderr,"Incorrect Hostname\n");
		exit(EXIT_FAILURE);
	}
	//create a IPV4, TCP socket
	if ((sock_desc = socket(AF_INET, SOCK_STREAM, 0))<0){
		fprintf(stderr,"Error opening new socket. Error Code:%d %s\n",errno,strerror(errno) );
		exit(EXIT_FAILURE);
	}
	//connect to sock
	if (connect(sock_desc, (struct sockaddr *)&addr, sizeof(addr)) < 0){
		fprintf(stderr,"Error connecting to server. Error Code:%d %s\n",errno,strerror(errno) );
		exit(EXIT_FAILURE);
	}
	else
		printf("Connected to server.\n");
	while(!(strncmp(cmd,"exit",4))==0){
		printf("Enter command: ");
		//reset cmd buffer
		memset(cmd, '\0', (512000 * sizeof(char)));
		//read command
		fgets(cmd,512000 * sizeof(char),stdin);
		//send command
		if((send(sock_desc,cmd,strlen(cmd),0))<0){
			fprintf(stderr,"Error sending command. Error Code:%d %s\n",errno,strerror(errno) );
			exit(EXIT_FAILURE);
		}
		//read data from socket
		if((recv(sock_desc, recv_data, (512000 * (sizeof (char))), 0) )<0 ){
			fprintf(stderr,"Error receiving command. Error Code:%d %s\n",errno,strerror(errno) );
			exit(EXIT_FAILURE);
		}
		//print received data
		printf("%s\n",recv_data );
		memset(recv_data, '\0', (512000 * sizeof(char)));

	}
	free(cmd);
	free(recv_data);
	return 0;
}

