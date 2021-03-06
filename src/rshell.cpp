#include <iostream>
#include <string>
#include <boost/tokenizer.hpp> //Boost tokenizer
#include <unistd.h> // Fork()
#include <sys/types.h> // Wait()
#include <sys/wait.h> // Wait()
#include <vector>
#include <stdio.h> //Perror()
#include <errno.h> // Perror()
#include <algorithm>

using namespace std;
using namespace boost;

// Chose to write function comparing c-strings rather than copy to string then compare
// Used for checking exit command
bool cStringEqual(char* c1, char* c2)
{
	int i;
	for(i = 0; c1[i] != '\0' && c2[i] != '\0'; ++i)
	{
		if(c1[i] != c2[i])
		{
			return false;
		}
	}
	if(c1[i] != '\0' || c2[i] != '\0')
	{
		return false;
	}
	return true;
}

int main()
{
	//Hostname/loginname won't change
	char* loginName = getlogin();
	if(!loginName)//Returns NULL if fails
	{
		perror("Login error");
		exit(1);
	}

	char hostName[64];
	int hostStatus = gethostname(hostName, sizeof(hostName));
	if(hostStatus == -1)// hostatus is -1 for error
	{
		perror("Hostname error");
		exit(1);
	}

	while(true) //Shell runs until the exit command
	{
		cout << loginName << "@" << hostName << " $ "; // Prints command prompt
		string commandLine;
		getline(cin, commandLine); 
		if(commandLine.size() == 0)
		{
			continue;
		}

		// Accounts for comments by removing parts that are comments
		if(commandLine.find(" #") != string::npos)
		{
			commandLine = commandLine.substr(0, commandLine.find(" #"));
		} 

		// Finds locations of connectors; a && b, && has a location of 3
		vector<unsigned int> connectorLocs;
		unsigned int marker = 0; // Marks location to start find() from
		while(commandLine.find("&&", marker) != string::npos)//loc != string::npos) 
		{
			connectorLocs.push_back(commandLine.find("&&", marker));
			marker = commandLine.find("&&", marker) + 2;//loc + 2; // Starts searching after "&&"
		}
		marker = 0;
		while(commandLine.find("||", marker) != string::npos) 
		{
			connectorLocs.push_back(commandLine.find("||", marker)); 
			marker = commandLine.find("||", marker) + 2; // Starts searching after "||"
		}
		marker = 0;
		while(commandLine.find(";", marker) != string::npos)
		{
			connectorLocs.push_back(commandLine.find(";", marker));
			marker =  commandLine.find(";", marker)+ 1; // Starts searching after ";"
		}
		connectorLocs.push_back(0); // Will be sorted and put in beginning
		sort(connectorLocs.begin(), connectorLocs.end()); // Sorted to find each subcommand substring
		connectorLocs.push_back(commandLine.size()); // One past end index will act like connector
		
		// Runs through subcommands and runs each one
		// Works for connectors with nothing between them (tokenizer will have "" => syntax error, which is expected) 
		// # of subcommands == # of connectors - 1 (including 0, one-past-end)
		for(unsigned int i = 0; i < connectorLocs.size() - 1; ++i) 
		{
			int offset = 0; // Tells how much offset for connectors (&&, ||, ;)
			if(commandLine.at(connectorLocs.at(i)) == '&' || commandLine.at(connectorLocs.at(i)) == '|')
			{
				offset = 2;
			}
			else
			{
				if(commandLine.at(connectorLocs.at(i)) == ';')
				{
					offset = 1;
				}
			}

			//cout << commandLine.at(connectorLocs.at(i)) << endl; // DEBUGGING
			//cout << offset << endl; // DEBUGGING
			
			// For parsing line of commands; delimiter is whitespace, each token will be a command or an argument
			vector<string> strArgs;
			char_separator<char> sep(" ");
			// FOLLOWING LINE WILL BREAK IF USED DIRECTLY IN TOKENIZER
			string subcommand = commandLine.substr(connectorLocs.at(i) + offset, connectorLocs.at(i+1) - connectorLocs.at(i) - offset);
			//typedef tokenizer<char_separator<char>> tokenizer; // Used to use this
			//cout << "sub: " << subcommand << endl; // DEBUGGING
			tokenizer<char_separator<char>> tok(subcommand, sep);
			// First token is the command, other tokens are the arguments
			for(auto iter = tok.begin(); iter != tok.end(); ++iter)
			{
				//cout << "tok: " << *iter << endl; // DEBUGGING
				strArgs.push_back(*iter);
			}

			// Copy strArgs to vector of c-strings
			// NEED TO DO IT THIS WAY OR THERE'S ISSUES WITH POINTERS
			vector<char*> args;
			for(auto str : strArgs)
			{
				args.push_back(const_cast<char*> (str.c_str()));
			}
			args.push_back(NULL); // NULL terminating at the end of vector/array
			
			//Blank command or consecutive connectors
			if(args.size() == 1)
			{
				continue;
			}
			
			char* exitCString = const_cast<char*> ("exit"); 
				
			//cout << cStringEqual(args.at(0), exitCString) << endl; // DEBUGGING
			if(cStringEqual(args.at(0), exitCString)) // if command is exit, exit shell
			{
				exit(0);
			}
			
			// Executes commands/takes care of errors
			int pid = fork();
			if(pid == -1) // If fork fails
			{
				perror("Fork error");
				exit(1);
			}
			else
			{
				if(pid == 0) // Child process
				{
					execvp(args.at(0), &(args[0]));
					// Following don't run if execvp succeeds
					perror("Command execution error");
					_exit(1);
				}
				else // Parent process
				{
					int status; // Status isn't used but might use in future?
					int waitVar = wait(&status);
					if(waitVar == -1) // If child process has error
					{
						perror("Child process error");
						// exits if next connector is && or one-past-end element
						// continues if next connector is ; or ||
						
					}
					else
					{
						int exitStatus = WEXITSTATUS(status); // Checks whether returns 0/1 when exiting
						if(exitStatus == 1) // If unsuccessful command
						{
							if(connectorLocs.at(i+1) < commandLine.size() && 
								commandLine.at(connectorLocs.at(i+1)) == '&')
							{
								//cout << commandLine.at(connectorLocs.at(i+1)) << endl; // DEBUGGING
								break;
							}
						}
						else
						{
							if(connectorLocs.at(i+1) < commandLine.size() && 
								commandLine.at(connectorLocs.at(i+1)) == '|')
							{
								//cout << commandLine.at(connectorLocs.at(i+1)) << endl; // DEBUGGING
								break;
							}
						}
					}
				}
			}
		}
	}
	return 0;
}
