/*
 * server.c
 *
 *  Created on: Jun 15, 2017
 *      Author: abhimanyu
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <stddef.h>
#include <sys/time.h>



#define PORT 4500			// Port number start address
#define TCOUNT 50			// Sets max client queue

// type def structures
typedef struct request_queue{
	struct request_queue* next;
	struct request_queue* prev;
	int handle;
	unsigned num_web;
	// web saves website names - Max 63 characters
	char web[10][68];
	// result[][0] - Min ping
	// result[][1] - Average ping
	// result[][2] - Max ping
	// result[][3] - Status - 0-Pending, 1-Queued, 2-Complete
	unsigned result[10][4];
}request;

typedef struct thread{
	pthread_t tid;
	int client_socket;
	struct thread* next;

}thread_list;


// global variables

thread_list* clients_head=NULL;
request* head=NULL;
sem_t queue_r, queue_mutex, worker_token,key;
unsigned queue_readers = 0;
unsigned last_handle = 0;
int run=1;
int portno = PORT;


// Function to initialize new_request to be added to queue
request* new_request(unsigned num_of_web){
	request* new;
	if(!(new = malloc(sizeof(request)))){
		fprintf(stderr,"Error in malloc of new request" );
	}
	new->num_web = num_of_web;
	new->handle = -1;
	new->next = NULL;
	new->prev = NULL;
	return new;
}


// This function is responsible for communicating with the client
// and adding pingSites request on to the queue
void* client_thread(void *socket){
	// cmd - cmd received from client
	// send_data - string to be sent to the client

	int client_socket = *((int*) socket);
	char* cmd = (char *)malloc(512000 * sizeof(char));
	char* send_data = (char *)malloc(512000 * sizeof(char));
	memset(cmd, '\0', (512000 * sizeof(char)));
	memset(send_data, '\0', (512000 * sizeof(char)));

	// Declare temporary variables
	int num_of_web=0,i=0,y=0,k=0,flag=0;
	unsigned temp_handle=0;
	char status[20],temp_handle_str[20];
	request *temp;

	// keep looping until client sends exit command
	while(!(strncmp(cmd,"exit",4))==0){
		//reset cmd back to null characters
		memset(cmd, '\0', (512000 * sizeof(char)));

		//read data on socket, if any
		if((recv(client_socket, cmd, (512000 * sizeof(char)), 0) )<0 ){
			fprintf(stderr,"Error receiving command. Error Code:%d %s\n",errno,strerror(errno) );
			exit(EXIT_FAILURE);
		}

		// if cmd is pingSites, start parsing data
		// look for www. in the received string and
		// count them using num_of_web
		if (strncmp(cmd,"pingSites",9)==0){

			for(i=0;i<strlen(cmd)-4;i++){
				if(cmd[i]=='w' && cmd[i+1]=='w' && cmd[i+2]=='w' && cmd[i+3]=='.'){
					num_of_web++;
				}
			}

			flag = 0;
			// If number of hostnames in the command is greater than 0 & less than 11
			// save them in the request structure
			if(num_of_web>0 && num_of_web<=10){
				request* add = new_request(num_of_web);
				// i - send_data index, y - web index, k - index within y
				for(i=0,y=0,k=0;i<strlen(cmd) && y<num_of_web;i++){
					if(cmd[i]=='w' && cmd[i+1]=='w' && cmd[i+2]=='w' && cmd[i+3]=='.' && flag==0){
						flag=1;
						i=i+4;
					}
					if(flag==1){
						add->web[y][k]=cmd[i];
						k++;
						if(cmd[i]==','){
							add->web[y][k-1]='\0';
							// set results to pending
							add->result[y][0]=0;
							add->result[y][1]=0;
							add->result[y][2]=0;
							add->result[y][3]=0;
							flag=0;
							y++;
							k=0;

						}
					}
				}
				add->web[y][k-1]='\0';

				// Gain writing access to request queue and add new request
				sem_wait(&queue_r);
				add->handle = temp_handle = last_handle;
				last_handle++;
				if(head==NULL)
					head = add;
				else{
					temp = head;
					while(temp->next!=NULL){
						temp=temp->next;
					}
					temp->next = add;
				}
				sem_post(&queue_r);
				// Signal worker thread for availability of request
				sem_post(&worker_token);
				//create send_data string to be sent to client
				sprintf(send_data,"Ping request received.\nRequest handle: %u",temp_handle);

			}
			else
				sprintf(send_data,"Ping request does not contain any web site name or conatins more than 10 website names.");
			num_of_web = 0;
		}


		// if cmd is showHandles, add all available <handles> to send_data string
		else if (strncmp(cmd,"showHandles\n",12)==0){
			char handle_str[10];
			temp=head;
			sprintf(send_data,"Available Handles\n");
			i=18;
			//Gain reading access to request queue
			sem_wait(&queue_mutex);
			if(queue_readers==0){
				queue_readers++;
				sem_wait(&queue_r);
				sem_post(&queue_mutex);
			}
			else{
				queue_readers++;
				sem_post(&queue_mutex);
			}

			// Go through the queue and add handles to send_data buffer
			while(temp!=NULL){
				sprintf(handle_str,"%u",temp->handle);
				for(k=0;k<strlen(handle_str);k++){
					send_data[i]=handle_str[k];
					i++;
				}
				send_data[i]='\t';
				i++;
				temp=temp->next;
			}

			// release access to queue
			sem_wait(&queue_mutex);
			if(queue_readers==1){
				queue_readers--;
				sem_post(&queue_r);
				sem_post(&queue_mutex);
			}
			else{
				queue_readers--;
				sem_post(&queue_mutex);
			}

			send_data[i]='\n';
		}

		//if cmd is showHandleStatus or showHandleStatus <handle>
		else if (strncmp(cmd,"showHandleStatus",16)==0){
			if(!(cmd[16]==' ')){
				//if showHandleStatus, gain reading access to queue
				sem_wait(&queue_mutex);
				if(queue_readers==0){
					queue_readers++;
					sem_wait(&queue_r);
					sem_post(&queue_mutex);
				}
				else{
					queue_readers++;
					sem_post(&queue_mutex);
				}

				// Go through the queue and prepare send_data string
				// by copying all details of request queue to string
				temp=head;
				while(temp!=NULL){
					num_of_web = temp->num_web;
					while(num_of_web>0){
						if(temp->result[num_of_web-1][3]==0){
							strcpy(status,"IN_QUEUE");
						}
						else if(temp->result[num_of_web-1][3]==1){
							strcpy(status,"IN_PROGRESS");
						}
						else if(temp->result[num_of_web-1][3]==2){
							strcpy(status,"COMPLETE");
						}
						if(strlen(send_data)<(512000*sizeof(char))-200)
							sprintf(send_data+strlen(send_data),"%d\twww.%s\t%u\t%u\t%u\t%s\n",temp->handle,temp->web[num_of_web-1],
									temp->result[num_of_web-1][0],temp->result[num_of_web-1][1],temp->result[num_of_web-1][2],
									status);
						num_of_web--;
					}
					temp=temp->next;
				}

				// release access to queue
				sem_wait(&queue_mutex);
				if(queue_readers==1){
					queue_readers--;
					sem_post(&queue_r);
					sem_post(&queue_mutex);
				}
				else{
					queue_readers--;
					sem_post(&queue_mutex);
				}


			}
			else{
				// if showHandleStatus <handle>
				// do same but for specific handle
				// go through the queue search for <handle>
				// and save to send_data string

				for(i=17,k=0;i<strlen(cmd);i++,k++){
					temp_handle_str[k]=cmd[i];
				}
				temp_handle = strtoul(temp_handle_str, NULL, 10);
				printf("Enquiry for handle %u\n",temp_handle);
				fflush(stdout);


				sem_wait(&queue_mutex);
				if(queue_readers==0){
					queue_readers++;
					sem_wait(&queue_r);
					sem_post(&queue_mutex);
				}
				else{
					queue_readers++;
					sem_post(&queue_mutex);
				}

				temp=head;
				int found = 0;
				while(temp!=NULL){
					if(temp->handle==temp_handle){
						found = 1;
						num_of_web = temp->num_web;
						while(num_of_web>0){
							if(temp->result[num_of_web-1][3]==0){
								strcpy(status,"IN_QUEUE");
							}
							else if(temp->result[num_of_web-1][3]==1){
								strcpy(status,"IN_PROGRESS");
							}
							else if(temp->result[num_of_web-1][3]==2){
								strcpy(status,"COMPLETE");
							}
							if(strlen(send_data)<(512000*sizeof(char))-200)
								sprintf(send_data+strlen(send_data),"%d\twww.%s\t%u\t%u\t%u\t%s\n",temp->handle,temp->web[num_of_web-1],
										temp->result[num_of_web-1][0],temp->result[num_of_web-1][1],temp->result[num_of_web-1][2],
										status);
							num_of_web--;
						}
					}

					temp=temp->next;
				}
				//if handle was not found in the queue, send invalid handle message to client
				if(found == 0)
					sprintf(send_data,"Invalid Handle.\n");



				sem_wait(&queue_mutex);
				if(queue_readers==1){
					queue_readers--;
					sem_post(&queue_r);
					sem_post(&queue_mutex);
				}
				else{
					queue_readers--;
					sem_post(&queue_mutex);
				}
			}
		}

		// if cmd is help, send help message to client
		else if (strncmp(cmd,"help",4)==0){
			sprintf(send_data,"Help Menu\n1.pingSites\n\tUsage: pingSites www.google.com,www.cnn.com\n\tUser can enter multiple websites"
								"\n\tWebsite Format: www.LABEL.com"
								"\n\tMax website limit per request - 10"
								"\n2.showHandles\n\tThis command returns the handles of different request made by all clients of the server "
								"\n3.showHandleStatus <handle>\n\tThis command will show the status of the specified handle"
								"\n\tIf command entered without a <handle>,\n\tStatus of all handles will be shown"
								"\n4.exit\n\tExits the client application."
								"\n5.help\n\tShows this screen.\n");
		}
		else if ((strncmp(cmd,"exit",4))==0){
			sprintf(send_data,"Disconnected from Server. Thank you!\n");
		}
		// case for unknown command
		else
			sprintf(send_data,"Unknown Command.\n");

		// Display cmd received from client on server terminal
		printf("Command Received: %s",cmd );

		// send send_data string to client through socket
		if((send(client_socket,send_data,strlen(send_data),0))<0){
			fprintf(stderr,"Error sending command. Error Code:%d %s\n",errno,strerror(errno) );
			exit(EXIT_FAILURE);
		}

		// reset contents to send_data
		memset(send_data, '\0', (512000 * sizeof(char)));



	}

	// free memory and exit client thread
	printf("Exiting client thread\n");
	free(cmd);
	free(send_data);
	return NULL;

}


// This function waits for a keypress 'q' to start the exit procedure
void* keystroke( void *nothing ){

	char key='a';
	printf("\nPress 'q' to exit\n");
	do{
		key = getchar();
	}while(key!='q');
	printf("Shutting down server.\n");
	run=0;
	return NULL;
}


// Worker thread responsible for pinging the servers and saving results on the queue
void* worker( void *nothing ){

	int portno=80,sock_desc;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	request* work;
	request* local=malloc(sizeof(request));
	work = head;
	int i=0,flag=0;

	char host[256] = "www.google.com";

	while(run){
		//wait for pending requests
		sem_wait(&worker_token);
		work = head;
		// gain write access to queue, change status of request and copy to local structure
		sem_wait(&queue_r);
		while(work!=NULL){
			if(work->result[0][3]==0){
				flag=1;
				for(i=0;i<work->num_web;i++)
					work->result[i][3]=1;
					local->handle=work->handle;
					local->next=work->next;
					local->num_web=work->num_web;
					local->prev=work->prev;
					for(i=0;i<work->num_web;i++){
						local->result[i][0]=work->result[i][0];
						local->result[i][1]=work->result[i][1];
						local->result[i][2]=work->result[i][2];
						local->result[i][3]=work->result[i][3];
						strcpy(local->web[i],work->web[i]);
					}
					break;
				}
				work = work->next;
		}
		sem_post(&queue_r);

		// if pending request was found in the queue
		// create socket and start ping process
		if(flag==1){
			struct timeval stop, start;
			for(i=0;i<local->num_web;i++){
				sprintf(host,"www.%s",local->web[i]);
				//set min ping to high value for comparison
				local->result[i][0]=99999;
				for(int j=0;j<10;j++){

					if ((sock_desc = socket(AF_INET, SOCK_STREAM, 0))<0){
						fprintf(stderr,"Error opening new socket. Error Code:%d %s\n",errno,strerror(errno) );
						exit(EXIT_FAILURE);
					}
					// get ip address from label provided by client
					server = gethostbyname(host);

					if (server == NULL) {
						fprintf(stderr,"Error. Cannot find host.\n");
						exit(EXIT_FAILURE);
					}
					bzero((char *) &serv_addr, sizeof(serv_addr));
					serv_addr.sin_family = AF_INET;
					bcopy((char *)server->h_addr,
							(char *)&serv_addr.sin_addr.s_addr,
							server->h_length);
					serv_addr.sin_port = htons(portno);

					// get start time
					gettimeofday(&start, NULL);
					// connect to website
					if (connect(sock_desc,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
						fprintf(stderr,"Error connecting to server. Error Code:%d %s\n",errno,strerror(errno) );
						exit(EXIT_FAILURE);
					}
					else{
						//get end time
						gettimeofday(&stop, NULL);
						// PING = start time - end time, divide by 1000 to get result in ms
						unsigned ping = (stop.tv_usec - start.tv_usec)/1000;
						// compare value and save result
						if(ping<local->result[i][0] )
							local->result[i][0]= ping;
						else if(ping>local->result[i][2])
							local->result[i][2]= ping;
						local->result[i][1]+=ping;
					}
					close(sock_desc);
				}
				// save average result
				local->result[i][1]=local->result[i][1]/10;

		   	}
			for(i=0;i<local->num_web;i++)
			local->result[i][3]=2;
			flag=0;
		}
		work=head;

		// gain access to queue and copy results
		sem_wait(&queue_r);
		while(work!=NULL){
			if(local->handle==work->handle){
				for(i=0;i<local->num_web;i++){
					work->result[i][0]=local->result[i][0];
					work->result[i][1]=local->result[i][1];
					work->result[i][2]=local->result[i][2];
					work->result[i][3]=local->result[i][3];
				}
				break;
			}
			work=work->next;
		}
		sem_post(&queue_r);

	}
	free(local);
	printf("Exiting worker thread.\n");
	return NULL;
}

// This function is similar to client.c
// accesses the socket for clients to trigger the parent off the blocking call on accept
void* exiter( void *nothing ){

	int sock_desc;
	struct sockaddr_in addr;
	memset(&addr, '\0', sizeof(addr));

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
	while(run);
	if (connect(sock_desc, (struct sockaddr *)&addr, sizeof(addr)) < 0){
		fprintf(stderr,"Error connecting to server. Error Code:%d %s\n",errno,strerror(errno) );
		exit(EXIT_FAILURE);
	}
	return NULL;

}


int main(){
	int sock_desc;
	struct sockaddr_in addr;
	int addrlen = sizeof(addr);
	pthread_t tid, tid_init[7];
	int *arg = malloc(sizeof(*arg));
	thread_list* c_temp;

	//initialize semaphores
	sem_init(&queue_r, 0, 1);
	sem_init(&queue_mutex,0, 1);
	sem_init(&worker_token,0, 0);
	sem_init(&key, 0, 1);

	//AF_INET - IPV4, Incoming address set to any
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;

	int connected = 0;
	while(connected == 0){
		addr.sin_port = portno;
		//create a IPV4, TCP socket
		if ((sock_desc = socket(AF_INET, SOCK_STREAM, 0))<0){
			fprintf(stderr,"Error opening new socket. Error Code:%d %s\n",errno,strerror(errno) );
			exit(EXIT_FAILURE);
		}

		//bind socket to address, keep incrementing port number by 1 if unable to connect
		if((bind(sock_desc, (struct sockaddr *) &addr, addrlen))<0){
			fprintf(stderr,"Error binding socket. Error Code:%d %s\n",errno,strerror(errno) );
			portno++;
			if(portno==5001)
				exit(EXIT_FAILURE);
		}
		else
			connected = 1;
	}

	//create thread which waits for keypress 'q' to exit
	if((errno = pthread_create(&(tid_init[0]), NULL, &keystroke, NULL))!=0){
		fprintf(stderr,"Error creating keyboard input thread. Error Code:%d %s\n",errno,strerror(errno) );
		exit(EXIT_FAILURE);
	}

	//create 5 worker threads
	for(int i=1;i<6;i++){
		if((errno = pthread_create(&(tid_init[i]), NULL, &worker, NULL))!=0){
			fprintf(stderr,"Error creating worker thread. Error Code:%d %s\n",errno,strerror(errno) );
			exit(EXIT_FAILURE);
		}
		else
			printf("Worker Thread %d Running.\n",i);
	}

	//create exit thread, helps exit the parent waiting for incoming request on socket
	if((errno = pthread_create(&(tid_init[6]), NULL, &exiter, NULL))!=0){
		fprintf(stderr,"Error creating exit thread. Error Code:%d %s\n",errno,strerror(errno) );
		exit(EXIT_FAILURE);
	}

	//set socket in passive mode, accepts incoming requests
	if((listen(sock_desc, 50))<0){
		fprintf(stderr,"Error setting socket in passive mode. Error Code:%d %s\n",errno,strerror(errno) );
		exit(EXIT_FAILURE);
	}
	else
		printf("Server Running. Listening for incoming requests on port %d\n",addr.sin_port);


	int i=0;
	while(run){
		//initiate structure to save client_thread info
		if(clients_head == NULL)
			clients_head = c_temp = malloc(sizeof(thread_list));
		else{
			c_temp = clients_head;
			while(c_temp->next!=NULL){
				c_temp=c_temp->next;
			}
			c_temp = malloc(sizeof(thread_list));
		}

		//accept connection if available, blocks if not available
		if((c_temp->client_socket=(accept(sock_desc, (struct sockaddr *)&addr, (socklen_t*)&addrlen)))<0){
			fprintf(stderr,"Error accepting connection. Error Code:%d %s\n",errno,strerror(errno) );
			exit(EXIT_FAILURE);
		}
		else{
			// create client_thread
			if((errno = pthread_create(&tid, NULL, &client_thread, &(c_temp->client_socket)    ))!=0){
				fprintf(stderr,"Error creating client thread. Error Code:%d %s\n",errno,strerror(errno) );
				exit(EXIT_FAILURE);
			}
			else{
				printf("Incoming connection accepted at ID %d Socket_Descriptor %d.\n",i,(c_temp->client_socket));
				c_temp->tid=tid;
			}
			i++;
		}
	}

	//Get ready to shutdown and close
	//close socket
	close(sock_desc);


	// Wait for all client threads to terminate
	printf("Waiting for client threads to exit\n");
	c_temp = clients_head;
	while(c_temp!=NULL){
		pthread_join(c_temp->tid,NULL);
		c_temp=c_temp->next;
	}

	// Wait for all worker threads to terminate
	printf("Waiting for worker threads to exit\n");
	sem_post(&worker_token);
	sem_post(&worker_token);
	sem_post(&worker_token);
	sem_post(&worker_token);
	sem_post(&worker_token);
	for(i=0;i<7;i++){
		sem_post(&worker_token);
		pthread_join(tid_init[i],NULL);
	}

	// Free up malloc-ed memory
	printf("Free-ing memory.\n");
	c_temp = clients_head;
	while ((c_temp = clients_head) != NULL) {
	    clients_head = clients_head->next;
	    free (c_temp);
	}

	request* temp = head;
	while ((temp = head) != NULL) {
		head = head->next;
		free (temp);
	}

	printf("Thank You. Bye!\n");

	return 0;
}




