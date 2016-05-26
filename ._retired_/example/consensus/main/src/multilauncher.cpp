#include <iostream>
#include "system/common.hpp"
#include "unistd.h"
#include "signal.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>

using namespace std;
using namespace solid;

static uint32		crtidx;

int childMain(int argc, char *argv[]);

int main(int argc, char *argv[]){
	if(argc < 4){
		cout<<"Usage: "<<endl;
		
		cout<<"$ multi_launcher 3 ./client \"\" localhost:1112 localhost:1122 localhost:1132"<<endl;
		cout<<"\t-> ./client localhost:1112 localhost:1122 localhost:1132"<<endl;
		cout<<"\t-> ./client localhost:1112 localhost:1122 localhost:1132"<<endl;
		cout<<"\t-> ./client localhost:1112 localhost:1122 localhost:1132"<<endl;
		
		cout<<"$ multi_launcher 3 ./client \"%\" localhost:1112 localhost:1122 localhost:1132"<<endl;
		cout<<"\t-> ./client 0 localhost:1112 localhost:1122 localhost:1132"<<endl;
		cout<<"\t-> ./client 1 localhost:1112 localhost:1122 localhost:1132"<<endl;
		cout<<"\t-> ./client 2 localhost:1112 localhost:1122 localhost:1132"<<endl;
		
		cout<<"$ multi_launcher 3 ./client \"-i\" localhost:1112 localhost:1122 localhost:1132"<<endl;
		cout<<"\t-> ./client -i 0 localhost:1112 localhost:1122 localhost:1132"<<endl;
		cout<<"\t-> ./client -i 1 localhost:1112 localhost:1122 localhost:1132"<<endl;
		cout<<"\t-> ./client -i 2 localhost:1112 localhost:1122 localhost:1132"<<endl;
		return 0;
	}
	int cnt(atoi(argv[1]));
	if(cnt <= 0){
		cout<<"Need a non-null, pozitive count!"<<endl;
		return 0;
	}
	signal(SIGCHLD, SIG_IGN);
	for(crtidx = 0; crtidx < cnt; ++crtidx){
		pid_t pid(fork());
		
		if(pid == 0){//child
			return childMain(argc, argv);
		}else{//parent
			
		}
	}
	return 0;
}

int childMain(const int argc, char *argv[]){
	char buf[128];
	char *appstr(argv[2]);
	char *idxstr(argv[3]);

	//make way for the final NULL
	//memmove(argv, argv + 2, argc - 2);
	//argv[argc - 2] = NULL;
	
	if(!idxstr[0]){
		memmove(argv, argv + 3, (argc - 3) * sizeof(char*));
		argv[argc - 3] = NULL;
		argv[0] = appstr;
		
	}else if(!strcmp(idxstr, "%")){
		memmove(argv, argv + 2, (argc - 2) * sizeof(char*));
		argv[argc - 2] = NULL;
		argv[1] = buf;
		sprintf(buf, "%d", crtidx);
	}else{
		memmove(argv, argv + 1, (argc - 1) * sizeof(char*));
		argv[argc - 1] = NULL;
		argv[0] = appstr;
		argv[1] = idxstr;
		argv[2] = buf;
		sprintf(buf, "%d", crtidx);
	}
	
	char *penv[1];
	penv[0] = NULL;
	
	execve(appstr, argv, penv);
	exit(EXIT_FAILURE);
}
