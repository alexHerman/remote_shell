#include <algorithm>
#include <arpa/inet.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <netinet/in.h>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using namespace std;

int createSocketServer(int port);
int main (int argc, char* argv[]);

int SERVER_PORT = 1234;
int STDIN_FD, STDOUT_FD;

int main(int argc, char* argv[])
{
	bool quit = false;
	string input, returnInput;
	STDIN_FD = dup(STDIN_FILENO);
	STDOUT_FD = dup(STDOUT_FILENO);
	int socketFD = createSocketServer(SERVER_PORT);

	if (socketFD < 0)
		exit(1);


	while (true)
	{
		//Output prompt to std out
		dup2(STDOUT_FD, STDOUT_FILENO);
		cout << "hsh> ";
		fflush(stdout);
		dup2(socketFD, STDOUT_FILENO);

		//Get input from std in to send to client
		dup2(STDIN_FD, STDIN_FILENO);
		getline(cin, input);
		cout << input << endl;
		fflush(stdin);
		dup2(socketFD, STDIN_FILENO);

		//Exit if applicable
		if (input == "exit")
		{
			close(socketFD);
			exit(1);
		}

		//Get the output from the client
		getline(cin, returnInput);

		//Display client output on std out
		dup2(STDOUT_FD, STDOUT_FILENO);
		cout << returnInput << endl;
		fflush(stdout);
		dup2(socketFD, STDOUT_FILENO);
	}

	close(socketFD);
}

int createSocketServer(int port)
{
	int listenfd = 0, connfd = 0;
	struct sockaddr_in serv_addr;

	char sendBuff[1025];

	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	memset(&serv_addr, '0', sizeof(serv_addr));
	memset(sendBuff, '0', sizeof(sendBuff));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

	cout << "Binding..." << endl;
	while (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) ;

	cout << "Listening for client..." << endl;
	listen(listenfd, 10);

	connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);
	cout << "Connection accepted. Ready for input." << endl;

	return connfd;
}
