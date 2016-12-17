/*******************************************************************************
* Author: Alex Herman
*
* Class: Operating Systems
*
* Professor: Dr. McGough
*
* Due Date: March 14, 2016
*
* Description: This program is a shell. It is a command line that takes
*   commands and arguments from the user through a text interface. There are six
*   intrinsic commands in the shell: systat, cmdnm, pwd, cd, signal, and exit.
*   These commands do the following:
*     systat - Displays information about the hardware and operating system
*     cmdnm - Displays the command used to start a chosen process
*     pwd - Prints the current working directory of the shell
*     cd - Changes the working directory to the specified directory
*     signal - Sends a signal to a process
* 	  hb - Prints the system timestamp every chosen interval
*     exit - Exits the the shell
*   Any command issued by the user that is not intrinsic is executed through an
*   exec() call. If the chosen command is not found, an error message is
*   printed. Input and Output redirection is also supported. These can be used
* 	with the following syntax:
* 	  [command] > [file] - Redirect standard out to the chosen file
* 	  [command] < [file] - Redirect standard in to the chosen file
* 	  [command] | [command] - Redirect standard out from the first command to
* 								standard in for the second command
* 	  [command] )) [port] - Redirect standard out to a the port specified
* 	  [command] (( [address] [port] - Redirects standard in to the port and ip
* 									  address specified
*
*******************************************************************************/

#include <algorithm>
#include <arpa/inet.h>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <netinet/in.h>
#include <pthread.h>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using namespace std;

bool chooseCommand(vector<string> tokens, int fd[]);
void *cmdnm(void *pid);
int createSocketClient(string address, string port);
int createSocketServer(string port);
void execute(vector<string> tokens, int fd[]);
void handleRedirection(vector<string>& tokens, vector<string>& lhs, int fd[],
	bool &piped, bool &redirectIn, bool &redirectOut);
void *hb(void *arg);
bool isNotBlank(const char* c);
bool isInteger(string c);
bool pipeCommands(vector<string> lhs, vector<string> rhs);
void printCurrentTime();
void pwd();
void signal(int signalNum, int pid);
void *systat(void *arg);
void usage(string command);

struct hbArg
{
	int tinc;
	int tend;
	char tval[10];
};


/*******************************************************************************
* Function: main()
*
* Description: This is the main entry point of the program. It is in charge of
*   displaying the prompt and parsing the user's arguments, which it then
*   passes to another function that determines the course of action.
*
* Parameters:
*   argc - The number of arguments passed to the program from the command line
*   argv - An array of strings passed to the program from the command line
*
*******************************************************************************/
int main(int argc, char* argv[])
{
    bool quit = false;

    //Event loop, runs once per command from the user
    while (!quit)
    {
    	string input, command;
        cout << "dsh> ";

        getline(cin, input);

        //If the user entered a command, check if it is an intrinsic functions
        //If so, call that function. If not, pass it off to exec()
        if (isNotBlank(input.c_str()))
        {
            //Tokenize the input string
            istringstream iss(input);
            vector<string> tokens;
			int fd[2] = {STDIN_FILENO, STDOUT_FILENO};
			bool redirectIn = false, redirectOut = false, piped = false;

            copy(istream_iterator<string>(iss), istream_iterator<string>(),
                 back_inserter(tokens));

			//Handle all IO redirection
			vector<string> lhs;
			handleRedirection(tokens, lhs, fd, piped, redirectIn, redirectOut);

			//Execute the command if it was not piped, in which case both
			//commands have already been executed.
			if (!piped)
        		quit = chooseCommand(lhs, fd);

			//Close input or output files if used
			if (redirectIn)
				close(fd[0]);
			if (redirectOut)
				close(fd[1]);
        }
    }

    return 0;
}

/*******************************************************************************
* Function: chooseCommand()
*
* Description:  This functions is the decision making engine. Given a vector of
*   strings that represent the arguments passed into the shell by the user, it
*   either calls the correct intrinsic function, or it calls a function to
*   execute the command in a new process. The input and output of any of these
* 	functions will be redirected to the files specified in the fd argument.
* 	If it returns true, then the exit command has been entered and it will exit
* 	the program.
*
* Parameters:
*   tokens - The vector of string arguments passed by the user into the shell
* 	fd	   - This is an array that contains the file descriptors for input and
* 			 output.
*
*******************************************************************************/
bool chooseCommand(vector<string> tokens, int fd[])
{
    string command = tokens[0];
    transform(command.begin(), command.end(), command.begin(), ::tolower);
	int stdInFD = dup(STDIN_FILENO);
	int stdOutFD = dup(STDOUT_FILENO);
	pthread_t thread;

	dup2(fd[0], STDIN_FILENO);
	dup2(fd[1], STDOUT_FILENO);
    if (command == "exit")
        return true;

    else if (command == "pwd")
        pwd();

    else if (command == "cmdnm")
    {
        if (tokens.size() > 1 && isInteger(tokens[1]))
		{
            pthread_create(&thread, NULL, cmdnm, (void *) tokens[1].c_str());
			pthread_join(thread, NULL);
		}
        else
            usage(command);
    }

    else if (command == "systat")
	{
        pthread_create(&thread, NULL, systat, NULL);
		pthread_join(thread, NULL);
	}

    else if (command == "cd")
    {
		if (tokens.size() > 1)
        {
            if (chdir(tokens[1].c_str()) == -1)
                cout << "Directory not found" << endl;
        }
        else
            usage(command);
    }

    else if (command == "signal")
    {
        if (tokens.size() > 2 && isInteger(tokens[1]) && isInteger(tokens[2]))
            signal(atoi(tokens[1].c_str()), atoi(tokens[2].c_str()));
        else
            usage(command);
    }
	else if (command == "hb")
	{
		if (tokens.size() == 4 && isInteger(tokens[1]) && isInteger(tokens[2]))
		{
			struct hbArg *arg = new struct hbArg;
			arg->tinc = atoi(tokens[1].c_str());
			arg->tend = atoi(tokens[2].c_str());
			strcpy(arg->tval, tokens[3].c_str());
			pthread_create(&thread, NULL, hb, (void *)arg);
		}
		else
			usage(command);
	}
    else
	{
		dup2(stdInFD, STDIN_FILENO);
		dup2(stdOutFD, STDOUT_FILENO);
        execute(tokens, fd);
	}

	dup2(stdInFD, STDIN_FILENO);
	dup2(stdOutFD, STDOUT_FILENO);

    return false;
}

/*******************************************************************************
* Function: cmdnm()
*
* Description: This function stands for "command name" and returns the contents
*   of the cmdline file in the proc file system. This file contains the command
*   that was issued in order to start the process.
*
* Parameters:
*   pid - The id of the process whos name is being retrieved
*
*******************************************************************************/
void *cmdnm(void *pidq)
{
	string pid = (char *)pidq;
    ifstream fin;

    string content;
    //This is the file in the proc file system that contains the information
    string fileName = "/proc/" + pid + "/cmdline";

    fin.open(fileName.c_str());
    if (fin.is_open())
    {
        //Read the file
        getline(fin, content);
        cout << content << endl;
        fin.close();
    }
    else
    {
        //If the process doesn't exist
        cout << "Process with PID " << pid << " not found."
            << endl;
    }

	pthread_exit(0);
}

/*******************************************************************************
* Function: createSocketClient()
*
* Description: This function creates a socket that is used to receive data from
* 	the specified ip address and port combination. It returns a file descriptor
* 	that the input of the command is redirected to.
*
* Parameters:
*   address - The ip address of the device sending the information
* 	port   - The number of the port that the information is sent on
*
*******************************************************************************/
int createSocketClient(string address, string port)
{
	int sockfd;
	char recvBuff[1024];
	struct sockaddr_in serv_addr;

    memset(recvBuff, '0',sizeof(recvBuff));
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(port.c_str()));

	if(inet_pton(AF_INET, address.c_str(), &serv_addr.sin_addr)<=0)
	{
		printf("\n inet_pton error occured\n");
		return 1;
	}

	if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
	   printf("\n Error : Connect Failed \n");
	   return 1;
	}

	return sockfd;
}

/*******************************************************************************
* Function: createSocketServer()
*
* Description: This function creates a socket that is used to transmit the
* 	output of the command to another process. It transmits this information over
* 	the port specified. This function returns a file descriptor that the output
*	of the command is redirected to.
*
* Parameters:
* 	port - The number of the port that the information is sent on
*
*******************************************************************************/
int createSocketServer(string port)
{
	int listenfd = 0, connfd = 0;
	struct sockaddr_in serv_addr;

	char sendBuff[1025];

	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	memset(&serv_addr, '0', sizeof(serv_addr));
	memset(sendBuff, '0', sizeof(sendBuff));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(atoi(port.c_str()));

	if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
		cout << "aser;dlkj" << endl;

	listen(listenfd, 10);

	connfd = accept(listenfd, (struct sockaddr*)NULL, NULL);

	return connfd;
}

/*******************************************************************************
* Function: execute()
*
* Description: This function is called when a non-intrinsic command is entered
*	by the user. It starts by forking itself. The child process then runs an exec
*	command to start the process specified by the user. The original process
*	prints the pid of the child process, and once the child has finished executing
*	it displays some statistics about the child. The input and output of the new
* 	process are redirected to the file descriptors specified.
*
* Parameters:
*   tokens - This is a vector of strings, each of which is an argument passed
*            into the shell by the user. This includes the name of the command.
* 	fd 	   - This is an array that contains the file descriptors for input and
* 			 output.
*
*******************************************************************************/
void execute(vector<string> tokens, int fd[])
{
    char* args[50] = {0};
    int i, pid;
    struct rusage usage1, usage2;

    //Fork the process, duplicating it in another part of memory
    pid = fork();

    getrusage(RUSAGE_CHILDREN, &usage1);
    //The parent process
    if (pid != 0)
    {
        //Display the id of the new process
        cout << "Child PID: " << pid << "\n\n";
        //Wait for the child process to finish
        wait(NULL);

        //Get some usage statistics about the child process
        getrusage(RUSAGE_CHILDREN, &usage2);
        long user_sec = usage2.ru_utime.tv_sec - usage1.ru_utime.tv_sec;
        long user_usec = usage2.ru_utime.tv_usec - usage1.ru_utime.tv_usec;
        long sys_sec = usage2.ru_stime.tv_sec - usage1.ru_stime.tv_sec;
        long sys_usec = usage2.ru_stime.tv_usec - usage1.ru_stime.tv_usec;
        printf("\nUser Time:   %ld.%.9lds\n", user_sec, user_usec);
        printf("System Time: %ld.%.9lds\n", sys_sec, sys_usec);
        printf("Page Faults: %ld\n", usage2.ru_majflt - usage1.ru_majflt);
        printf("Swaps:       %ld\n", usage2.ru_nswap - usage1.ru_nswap);
    }
    //The new/child process
    else
    {
		dup2(fd[0], STDIN_FILENO);
		dup2(fd[1], STDOUT_FILENO);
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

/*******************************************************************************
* Function: handleRedirection()
*
* Description: This function is used to handle all of the possible redirection
* 	operators that exist within a command. It can handle input and output
* 	redirection, command piping, and remote piping. If any output is redirected,
* 	the corresponding referenced boolean value is set.
*
* Parameters:
*   tokens     - This is a vector of strings, each of which is an argument passed
*            	 into the shell by the user. This includes the name of the command.
* 	lhs	       - This is used to return just the command tokens if redirection is used.
* 	fd         - This is an array that contains the file descriptors for input and
* 			 	 output.
* 	piped      - A boolean value passed by reference to tell the calling function
* 			 	 that two commands were piped.
* 	redirectIn - A  boolean reference to tell the calling function that input
* 				 was redirected
* 	redirectOut- A  boolean reference to tell the calling function that output
* 				 was redirected
*
*******************************************************************************/
void handleRedirection(vector<string>& tokens, vector<string>& lhs, int fd[],
	bool &piped, bool &redirectIn, bool &redirectOut)
{
	FILE *outStream, *inStream;

	for (int i = 0; i < tokens.size(); i++)
	{
		//Redirect stdin
		if (tokens[i] == "<")
		{
			redirectIn = true;
			inStream = fopen(tokens[i + 1].c_str(), "r");
			fd[0] = fileno(inStream);
			i = tokens.size();
			break;
		}
		//Redirect stdout
		else if (tokens[i] == ">")
		{
			redirectOut  = true;
			outStream = fopen(tokens[i + 1].c_str(), "w");
			fd[1] = fileno(outStream);
			i = tokens.size();
			break;
		}
		//Pipe output of left command to input of right command
		else if (tokens[i] == "|")
		{
			piped = true;
			vector<string> rhs;
			for (int j = i + 1; j < tokens.size(); j++)
				rhs.push_back(tokens[j]);

			pipeCommands(lhs, rhs);
			i = tokens.size();
			break;
		}
		//Remote pipe output
		else if (tokens[i] == "))")
		{
			redirectOut = true;
			fd[1] = createSocketServer(tokens[i + 1]);
			i = tokens.size();
			break;
		}
		//Remote pipe input
		else if (tokens[i] == "((")
		{
			redirectIn = true;
			fd[0] = createSocketClient(tokens[i + 1], tokens[i + 2]);
			i = tokens.size();
			break;
		}
		lhs.push_back(tokens[i]);
	}
}

/*******************************************************************************
* Function: hb()
*
* Description: This function prints out the current system time at an increment
* 	specified in the structure passed in the argument. This increment can be
* 	either seconds or milliseconds, also specified in the argument.
*
* Parameters:
*   pthreadArg - This is a void pointer to a structure that contains:
* 		tinc - The number of intervals between each timestamp printf
* 		tend - The number of intervals to print for
* 		tval - The interval unit, either s or ms (seconds or milliseconds)
*
*******************************************************************************/
void *hb(void *pthreadArg)
{
	struct hbArg *arg = (struct hbArg*)pthreadArg;
	//Convert tinc to microseconds for usleep() function
	long waitTime = arg->tinc * 1000;
	if (!strcmp(arg->tval, "s"))
		waitTime *= 1000;

	//Print current time stamp then wait tinc intervals
	for (long i = 0; i <= arg->tend; i += arg->tinc)
	{
		printCurrentTime();
		usleep(waitTime);
	}
	cout << "dsh> ";
	fflush(stdout);

	pthread_exit(0);
}

/*******************************************************************************
* Function: isNotBlank()
*
* Description: This function takes a character array and checks to see if it
*   contains any characters aside from white space. It stops searching after it
*   encounters a null terminator. It returns true if the string contains
*   non-whitespace characters. If the string is all whitespace, it returns false.
*
* Parameters:
*   str - A character pointer to the first letter of the string to be checked.
*
*******************************************************************************/
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

/*******************************************************************************
* Function: isInteger()
*
* Description: This function takes a string and determines if it is an integer.
*   If it is, it returns true. If not, it returns false.
*
* Parameters:
*   str - A character pointer to the first letter of the string to be checked.
*
*******************************************************************************/
bool isInteger(string c)
{
    const char* str = c.c_str();
    while (*str != '\0')
    {
        //If a non-whitespace character appears, return true
        if (!isdigit(*str))
            return false;
        str++;
    }
    return true;
}

/*******************************************************************************
* Function: pipeCommands()
*
* Description: This function is used to create a pipe between to processes. It
* takes the output of the command represented by the lhs vector
* and uses it as the input of the command represented by the rhs vector.
*
* Parameters:
*   lhs - The vector of command strings on the left hand side (lhs) of the pipe
*   rhs - The vector of command strings on the right hand side (rhs) of the pipe
*
*******************************************************************************/
bool pipeCommands(vector<string> lhs, vector<string> rhs)
{
	int fd[2];
	int pipeFD[2];
	pipe(fd);
	bool exit1, exit2;

	//Use the piped output for the first process
	pipeFD[0] = STDIN_FILENO;
	pipeFD[1] = fd[1];
	exit1 = chooseCommand(lhs, pipeFD);
	close(fd[1]);

	//Use the piped output for second process
	pipeFD[0] = fd[0];
	pipeFD[1] = STDOUT_FILENO;
	exit2 = chooseCommand(rhs, pipeFD);

	close(fd[0]);
	//The shell will still exit if either of the commands was 'exit'
	return exit1 && exit2;
}


/*******************************************************************************
* Function: printCurrentTime()
*
* Description: This function prints the system's current time. It displays it in
* 	hh:mm:ss format.
*
*******************************************************************************/
void printCurrentTime()
{
	time_t currentTime;
	struct tm *localTime;

	//Get the current time
	time(&currentTime);
	localTime = localtime(&currentTime);

	//Display the time in hh:mm:ss format
	cout << setfill('0') << setw(2) << (int)localTime->tm_hour << ":" <<
	setw(2) << (int)localTime->tm_min << ":" << setw(2) <<
	(int)localTime->tm_sec << endl;
}

/*******************************************************************************
* Function: pwd()
*
* Description: This function returns the current working directory of the shell.
*   It uses the getcwd() function to obtain this information.
*
*******************************************************************************/
void pwd()
{
    //Use getcwd to get the working directory of the current process
    char currentPath[500];
    getcwd(currentPath, sizeof(currentPath));
    cout << currentPath << endl;
}

/*******************************************************************************
* Function: signal()
*
* Description: This functions sends a signal to a selected process. It utilizes
*   the kill() function to send the desired signal.
*
* Parameters:
*   signalNum - This is the actual signal that is sent to the selected process
*   pid - This is the id of the process that the signal is sent to
*
*******************************************************************************/
void signal(int signalNum, int pid)
{
    //Send the specified signal to the specified process
    if (signalNum > 64)
    {
        cout << "Invalid signal" << endl;
        usage("signal");
    }
    kill(pid, signalNum);
}

/*******************************************************************************
* Function: systat()
*
* Description: This function displays some system information obtained from the
*   proc file system. It displays the linux version, system uptime, memory info,
*   and some information about the cpu in the computer.
*
* Parameters:
* 	arg - This argument contains no information but is required to use pthreads
*
*******************************************************************************/
void *systat(void *arg)
{
    string version, uptime, memTotal, memFree, cpuLabel, cpuData;
    bool readCPUInfo = true, beginWrite = false;

    //Open a file stream for each file containing the requested information
    ifstream Fversion("/proc/version");
    ifstream Fuptime("/proc/uptime");
    ifstream Fmeminfo("/proc/meminfo");
    ifstream Fvendorinfo("/proc/cpuinfo");

    //Read from each file
    getline(Fversion, version);
    getline(Fuptime, uptime);
    getline(Fmeminfo, memTotal);
    getline(Fmeminfo, memFree);

    //Parse the uptime file to pull out specific information
    istringstream uptimeStream(uptime);
    vector<string> uptimeTokens;
    copy(istream_iterator<string>(uptimeStream), istream_iterator<string>(),
         back_inserter(uptimeTokens));

     //Display version, uptime, and memory information
    cout << "Version: " << version << endl << endl;;
    cout << "Uptime: " << uptimeTokens[0] << " seconds total uptime" << endl;
    cout << "\t" << uptimeTokens[1] << " seconds spent idle (on all cores)"
        << endl << endl;
    cout << memTotal << endl << memFree << endl << endl;
    cout << "CPU info: " << endl;
    cout << "---------------------------------------------------" << endl;

    //Repeat once per line in the file
    do
    {
        //Read label and data from the file
        getline(Fvendorinfo, cpuLabel, ':');
        getline(Fvendorinfo, cpuData);

        //Begin displaying information starting with vendor_id
        if (cpuLabel.find("vendor_id", 0) != string::npos)
            beginWrite = true;
        if (beginWrite)
            cout << cpuLabel << " : " << cpuData << endl;

        //Stop reading after cache size
        if (cpuLabel.find("cache size", 0) != string::npos)
            readCPUInfo = false;
    }
    while (readCPUInfo);

    //Close input files
    Fversion.close();
    Fuptime.close();
    Fmeminfo.close();
    Fvendorinfo.close();

	pthread_exit(0);
}

/*******************************************************************************
* Function: usage()
*
* Description:  This function displays usage statements based on the command
*   entered by the user. This is only executed when a command is entered with
*   the incorrect number of arguments.
*
* Parameters:
*   command - The command that the user passed into the shell
*
*******************************************************************************/
void usage(string command)
{
    if (command == "cmdnm")
        cout << "Usage: cmdnm <pid>" << endl;

    else if (command == "cd")
        cout << "Usage: cd <directory>" << endl;

    else if (command == "signal")
    {
        cout << "Usage: signal <signalNum> <pid>" << endl;
        cout << "signalNum: values between 1 and 64" << endl;
        cout << "pid: the id of the process to send the signal to" << endl;
    }

	else if (command == "hb")
		cout << "Usage: hb <tinc> <tend> <tval>" << endl
		<< "ex: \"hb 1 60 s\" prints the time every 1 second for 60 seconds"
		<< endl;
}
