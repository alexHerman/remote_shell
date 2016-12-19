#include <algorithm>
#include <arpa/inet.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <netinet/in.h>
//#include <pthread.h>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using namespace std;

int createSocketClient(string address, int port);
void execute(string command);
bool isNotBlank(const char* str);
int main (int argc, char* argv[]);

string SERVER_ADDRESS = "127.0.0.1";
int SERVER_PORT = 1234;

int main(int argc, char* argv[])
{
	bool quit = false;
	int socketFD = createSocketClient(SERVER_ADDRESS, SERVER_PORT);

	if (socketFD < 0)
		exit(1);

	dup2(socketFD, STDIN_FILENO);
	dup2(socketFD, STDOUT_FILENO);

	while (!quit)
	{
		string input;
		getline(cin, input);

		if (isNotBlank(input.c_str()))
		{
			execute(input);
		}

		if (input == "exit")
			quit = true;
	}
}

int createSocketClient(string address, int port)
{
	int sockfd;
	struct sockaddr_in serv_addr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

	if(inet_pton(AF_INET, address.c_str(), &serv_addr.sin_addr)<=0)
	{
		printf("\n inet_pton error occured\n");
		return -1;
	}

	printf("Searching for server...\n");
	while( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) ;
	printf("Connected. Listening...\n");

	return sockfd;
}

bool isNotBlank(const char* str)
{
    //For each character in the string
    while (*str != '\0')
    {
        //If a non-whitespace character appears, return true
        if (!iswspace(*str))
            return true;
        str++;
    }
    return false;
}

void execute(string command)
{
    char* args[50] = {0};
	vector<string> tokens;
    int i, pid;
    struct rusage usage1, usage2;

	istringstream iss(command);
    copy(istream_iterator<string>(iss), istream_iterator<string>(),
        back_inserter(tokens));


    //Fork the process, duplicating it in another part of memory
    pid = fork();

    if (pid == 0)
    {
        //Convert the vector of arguments to an array of character strings
        for (i = 0; i < tokens.size(); i++)
            args[i] = (char *)tokens[i].c_str();
        //End the list with a null terminator
        args[i] = 0;

        //Attempt to start the new process in /bin

        execvp(args[0], args);

        //If the other attempts fail, display an error message
        cout << "Failed to find or start new process." << endl;
        exit(1);
    }
}
