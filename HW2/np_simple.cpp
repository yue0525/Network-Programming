// #include "npshell.cpp"
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h> 
#include <arpa/inet.h> //htons
#include<iostream>
#include<stdlib.h>
#include<vector>
#include<string>
#include<cstring>
#include<sstream>
#include<unistd.h> //fork,exec,pipe
#include<sys/wait.h> //waitpid()
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <queue>
using namespace std;

using namespace std;

typedef struct Block{
    vector<string> blockcmd;//把block從中分開 " "
    int count=-1;//判斷numpipe時，pipe在第幾個位置
    bool back=false;//後面有無pipenumber 不管|n
    bool backpipe=false;//後面有pipe '|'
    int pipenumber=-1;//告知目前是用幾號管子
    bool front_ispipe = false;
    bool error = false;
}Block;


typedef struct Numpipe{
    int fd[2];
    int timer=-1;//是否到你pipe了
}Numpipe;

void sigchld_handler(int );

vector<Numpipe> numpipe;

void init_input(vector<Block> &block,int &numpipeinline,vector<string> &string_nospace);

void exe_cmd(Block block);

int findindex(vector<string> &vec, string str);

void vectochar(vector<string> &vec, char *arr[], int index);
int oper(string &str);
vector<string> SplitString(const string line, const string delimeter);

void parsecmd(vector<string> &vec);

void npshell();

int main(int argc, char *argv[]){
    int port = atoi(argv[1]);
    int msock,ssock;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    int optval = 1;
    if (setsockopt(msock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == -1)
        perror("S/Setsocket");

    if ( (msock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        cout<<"server: can't open stream socket";
    if (bind(msock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        cout<<"server: can't bind local address";
    listen(msock, 1);
    while(true){
        unsigned int clilen;
        clilen = sizeof(cli_addr);
        ssock = accept(msock, (struct sockaddr *) &cli_addr, &clilen);
        if (ssock < 0) 
            cout<<"server: accept error";
        pid_t pid;
        if ( (pid = fork()) < 0) 
            cout<<"server: fork error";
        if (pid > 0){
            close(ssock);
            waitpid(pid, NULL, 0);
            while(waitpid(-1, NULL, WNOHANG) > 0){}
        }
        if (pid == 0){
            // connect(msock, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
            dup2(ssock, STDIN_FILENO);
            dup2(ssock, STDOUT_FILENO);
            dup2(ssock, STDERR_FILENO);
            close(msock);
            npshell();
            break;
        }
        
    }
    return 0;
}




void npshell(){    
    setenv("PATH", "bin:.", 1);
    
    while(true){
        //int numpipeinline = 0;
        
        string input;
        cout<<"% ";
        getline(cin,input);
        if (input[input.length()-1] == '\r'){
			input = input.substr(0, input.length()-1);
		}
        if (input.length() == 0)
			continue;
        vector<string> string_nospace;
        string_nospace = SplitString(input," ");
        if(string_nospace.size()>0){
            parsecmd(string_nospace);
        }
        
    }
}
void exe_cmd(Block block){
    char *argv[10000];
    int index;
    if((index = findindex(block.blockcmd, ">")) != -1){
        int f = open(block.blockcmd.back().data(), O_CREAT | O_RDWR | O_TRUNC , S_IREAD | S_IWRITE);
        if(dup2(f, STDOUT_FILENO)<0)
            cout<<"error"<<endl;
		close(f);
        vectochar(block.blockcmd, argv, index);
    }
    else{
        vectochar(block.blockcmd, argv, block.blockcmd.size());
    }
    //cerr<<"ready exe"<<endl;
    if(execvp(block.blockcmd[0].data(), argv)<0)
        cerr << "Unknown command: [" << block.blockcmd[0].data() << "]." << endl;
    exit(0);
}
void vectochar(vector<string> &vec, char *arr[], int index){
	for (int i=0; i<index; i++){
		arr[i] = (char*)vec[i].data();	
	}
	arr[vec.size()] = NULL;
}

int findindex(vector<string> &vec, string str){
    int judge=0,index;
    for(index=0;index<vec.size(); index++){
        if (vec[index]==str){
            judge=1;
            break;
        }
    }
    if (judge==0){
		index = -1;
	}
	return index;
}

vector<string> SplitString(const string line, const string delimeter){
    vector <string> v;
    size_t pos1, pos2;
    pos1 = 0;
    pos2 = line.find(delimeter);
    while(string::npos != pos2){
        v.push_back(line.substr(pos1, pos2-pos1));
        pos1 = pos2 + delimeter.size();
        pos2 = line.find(delimeter, pos1);
    }
    if(pos1 != line.length()){
        v.push_back(line.substr(pos1));
    }
    return v;
}
int oper(string &str){
    int buffer=0;
    int a;
    int index=0;
    int back_size=0;
    string str1;
    //cout<<str<<endl;
    // str1 = str1.assign(str, 0, 1);
    // cout<<stoi(str1);
    for(int i=0;i<str.length();i++){
        if(str[i]=='+'){
            buffer+=a;
            index=i+1;
            //cout<<index<<endl;
        }
        else{
            back_size = i - index;
            // cout<<back_size<<endl;
            str1 = str1.assign(str,index,back_size+1);
            //cout<<str1<<endl;
            a = stoi(str1);
            // cout<<a<<endl;
        }
    }
    buffer+=a;

    //cout<<buffer<<endl;
    return buffer;
}
void parsecmd(vector<string> &vec){
    
    bool front = false;
    int pipecase=0;//0:no pipe 1:need pipe
    vector<Block> block;
    vector<string> next_cmd;
    int numpipeinline=0;
    string word;
    Block block1;
    int judge=0;
    queue<pid_t> pids;
    for(int i=0;i<vec.size();i++){
        bool createpipeornot=true;
        if(vec[i][0] == '|' || vec[i][0] == '!'){
            block1.backpipe=true;
            if(vec[i][0] == '!')
                block1.error = true;
            if(vec[i].size()!=1){
                string str = vec[i].erase(0,1);
                int a = oper(str);
                //cout<<a<<endl;
                // int a = stoi(str);
                block1.back=true;
                int index;
                for(index=0;index<numpipe.size();index++){
                    if(numpipe[index].timer == a){
                        createpipeornot=false;
                        break;
                    }
                }
                if(createpipeornot){ //true -->create                   
                    Numpipe numpipe1;
                    int pipe_fd1=pipe(numpipe1.fd);
                    if (pipe_fd1 == -1){
                        perror("c8 cannot pipe");
                    }
                    numpipe1.timer = a;
                    block1.count = numpipe.size();
                    numpipe.push_back(numpipe1);
                }
                if(!createpipeornot){
                    block1.count = index ;
                }
                    
                for(int j=i;j<vec.size()-1;j++){
                    next_cmd.push_back(vec[j+1]);
                }
                if(a==1)
                    judge=1;
                
                for(int k=0;k<numpipe.size();k++){
                    if(numpipe[k].timer == 1){
                        judge = 1;
                        break;
                    }
                }
                break;
            }
            else{
                pipecase=1;
                numpipeinline++;
                front=true;
                block.push_back(block1);
                block1.blockcmd.clear();
                block1.front_ispipe=front;
                block1.backpipe=false;
            }
        }
        else{
            block1.blockcmd.push_back(vec[i]);
        }
    }
    block1.front_ispipe=front;
    block.push_back(block1);

    if(block[0].blockcmd[0] == "printenv"){
        if (getenv(block[0].blockcmd[1].data()) != NULL)
            cout<<getenv(block[0].blockcmd[1].data())<<endl;
    }
    else if(block[0].blockcmd[0] == "setenv"){
        setenv(block[0].blockcmd[1].data(),block[0].blockcmd[2].data(),1);
    }
    else if (block[0].blockcmd[0] == "exit" || block[0].blockcmd[0] == "EOF") {
        exit(0);
    }
    else{
        pid_t pid;
        int status;
        int pipe_fd;
        int pipearray[numpipeinline][2];
        if(numpipeinline!=0){
            for(int i=0;i<numpipeinline;i++){
                pipe_fd=pipe(pipearray[i]);
                if (pipe_fd == -1){
                    perror("c8 cannot pipe");
                }
            }
        }
        signal(SIGCHLD, sigchld_handler);
        for(int i=0;i<numpipeinline+1;i++){ //判斷要fork幾次
            // if (numpipeinline != 0 && i != numpipeinline){
            //     pipe_fd=pipe(pipearray[i]);
            //     if (pipe_fd == -1){
            //         perror("c8 cannot pipe");
            //     }
            // }
            if((pid = fork()) < 0){//你再搞
                i--;//這個i不算
            }
            if(pid > 0){//parent 
                if(!block[i].front_ispipe){
                    for(int k=0;k<numpipe.size();k++){
                        numpipe[k].timer--;                    
                    }
                }
                
                if(i>0){
                    close(pipearray[i-1][0]);
                    close(pipearray[i-1][1]);
                }
                /**/
                for(int j=0;j<numpipe.size();j++){
                    if(numpipe[j].timer==-1){
                        close(numpipe[j].fd[0]);
                        close(numpipe[j].fd[1]);
                    }
                }
                if(!block[i].backpipe)
                    waitpid( pid, &status, 0 );//等到目前這個child回傳
            }
            else if(pid == 0){
                if (pipecase == 0) //我不需要pipe
                {
                    if(block[i].back){
                        if(!block[i].error)
                            dup2(numpipe[block[i].count].fd[1],STDOUT_FILENO);
                        if(block[i].error){
                            dup2(numpipe[block[i].count].fd[1],STDOUT_FILENO);
                            dup2(numpipe[block[i].count].fd[1],STDERR_FILENO);
                        }
                    }
                    for(int j=0;j<numpipe.size();j++){
                        if(numpipe[j].timer==0){
                            dup2(numpipe[j].fd[0],STDIN_FILENO);
                            close(numpipe[j].fd[0]);
                            close(numpipe[j].fd[1]);
                        }
                    }
                    exe_cmd(block[i]);
                }
                    
                else if(pipecase == 1){ //有出現"|" 需要pipe                
                    if(i == 0){//第一個process
                        for(int j=0;j<numpipe.size();j++){
                            if(numpipe[j].timer == 0){
                                dup2(numpipe[j].fd[0],STDIN_FILENO);
                                close(numpipe[j].fd[0]);
                                close(numpipe[j].fd[1]);
                            }
                        }
                        if(!block[i].error)
                            dup2(pipearray[i][1],STDOUT_FILENO);
                        if(block[i].error){
                            dup2(pipearray[i][1],STDERR_FILENO);
                            dup2(pipearray[i][1],STDOUT_FILENO);
                        }                            
                        close(pipearray[i][1]);
                        
                    }
                    else if(i>0 && i<numpipeinline){//中間的process後面有人
                        dup2(pipearray[i-1][0],STDIN_FILENO);
                        if(!block[i].error)
                            dup2(pipearray[i][1],STDOUT_FILENO);
                        if(block[i].error){
                            dup2(pipearray[i][1],STDERR_FILENO);
                            dup2(pipearray[i][1],STDOUT_FILENO);
                        }                    
                        close(pipearray[i-1][0]);
                        close(pipearray[i-1][1]);
                        close(pipearray[i][0]);
                        close(pipearray[i][1]);
                    }
                    else{//後面無人          
                        if(block[i].back){
                            if(!block[i].error)      
                                dup2(numpipe[block[i].count].fd[1],STDOUT_FILENO);
                            if(block[i].error){
                                dup2(numpipe[block[i].count].fd[1],STDERR_FILENO);
                                dup2(numpipe[block[i].count].fd[1],STDOUT_FILENO);
                            }
                            close(numpipe[block[i].count].fd[0]);
                            close(numpipe[block[i].count].fd[1]);
                        }
                        dup2(pipearray[i-1][0],STDIN_FILENO);
                        close(pipearray[i-1][0]);
                        close(pipearray[i-1][1]);
                        
                    }
                    exe_cmd(block[i]);
                }
            }
        }
    }

    if(next_cmd.size()!=0){
        parsecmd(next_cmd);
    }

}
void sigchld_handler(int sig)
{
    int status;
    waitpid(-1, &status, WNOHANG) ;
}

