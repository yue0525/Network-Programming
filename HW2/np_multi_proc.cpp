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
#include <dirent.h>
#include <sys/mman.h>
using namespace std;
#define USERPIPE_PATH string("user_pipe/")
#define SM_PATH string("user_pipe/")

typedef struct Block{
    vector<string> blockcmd;//把block從中分開 " "
    int count=-1;//判斷numpipe時，pipe在第幾個位置
    bool back=false;//後面有無pipenumber 不管|n
    bool backpipe=false;//後面有pipe '|'
    int pipenumber=-1;//告知目前是用幾號管子
    bool front_ispipe = false;
    bool error = false;
    bool usercatch=false;
    bool userthrow=false;
    int dupnull = 0; // 1:nulldev-stdout'>' 2:nulldev-stdin'<'
    bool writefile=false;
    int catchnum=0;
    int pipeTo = -1; // index of userpipe To
    int fifoToFD = -1;
    int pipeFrom = -1; // index of userpipe From
    int fifoFromFD = -1;
}Block;

struct broadcastMsg
{
    char msg[4096];
    bool used = false;
    bool tell = false;
    bool sendto_me = true;
    int tellwho = 0; 
    int sendto_me_id = 0;
};


typedef struct User{
	bool hasUser;  
	int ssock;
	int ID;
    char nickname[40];
    char envname[20];
    char envvalue[20];
    char ip_addr[46];
    int pid = -1;
}User;

typedef struct Numpipe{
    int fd[2];
    int timer=-1;//是否到你pipe了
}Numpipe;


vector<string> SplitString(const string line, const string delimeter);
void ServerSignalHandler(int sig);
// void sigchld_handler(int );
void init_input(vector<Block> &block,int &numpipeinline,vector<string> &string_nospace);
void exe_cmd(Block block,int ID);
int findindex(vector<string> &vec, string str);
void vectochar(vector<string> &vec, char *arr[], int index);
int oper(string &str);
void parsecmd(vector<string> &vec,int ssock,string input);
void init_share_memory();
void npshell(int ID);
void welcome(int ssock);
void user_login(int ID);
void user_logout(int ID);
// void broadcast(int fromID,string broadcast_Msg);
void who(int ID);
void rename(int ID,string newname);
void tell(int fromID,int desID,string tell_msg);
void yell(int ID,string tell_msg);
void init_userset(int ID,int ssock,string IP);
void who(int ID);
void init_user();
void init_broad();
void user_login(int ID);
void signalHandler(int sig);
// User Users[30];
void userPipeFDArray_value();
// int userpipe_number[30][30];
const string prompt = "% ";
const int FD_NULL = open("/dev/null", O_RDWR);

// User *master;
User *Users;
broadcastMsg *BM;
vector<Numpipe> numpipe;
string csfile;
string bmfile;
// slave global variable
int myIndex;
int server_pid = 0;
int user_id = 0;
int userPipeFDArray[30];

int main(int argc, char *argv[]){
    userPipeFDArray_value();
    signal(SIGCHLD, ServerSignalHandler);//zombie
    signal(SIGINT, ServerSignalHandler);//crtl C
    signal(SIGUSR1, ServerSignalHandler);//broadcast
    
    init_share_memory();
    init_user();
    init_broad();
    int port = atoi(argv[1]);
    int msock,ssock;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if ( (msock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        cout<<"server: can't open stream socket";
    int optval = 1;
    if (setsockopt(msock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == -1)
        perror("S/Setsocket");
    
    if (bind(msock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        cout<<"server: can't bind local address";

    listen(msock, 50);

    pid_t ppid = getpid();
    server_pid = ppid;

    while(true){
        unsigned int clilen;
        clilen = sizeof(cli_addr);
        ssock = accept(msock, (struct sockaddr *) &cli_addr, &clilen);
        if (ssock < 0) 
            cout<<"server: accept error";

        string IP=inet_ntoa(cli_addr.sin_addr);
        IP += ":" + to_string(ntohs(cli_addr.sin_port));        

        pid_t pid = fork();
        while (pid < 0)
        {
            perror("fork");
            usleep(500);
            pid = fork();
        }
        if (pid > 0){
            // cout<<"parent"<<endl;
            for (int i = 0; i < 30; i++)
            {                
                if (!Users[i].hasUser)
                {
                    // cout<<"i"<<endl;
                    Users[i].ssock = ssock;
                    Users[i].ID = i;
                    strcpy(Users[i].ip_addr, IP.c_str());
                    string name="(no name)";
                    strcpy(Users[i].nickname, name.c_str());
                    Users[i].pid = pid;
                    Users[i].hasUser = true;
                    // welcome(ssock);
                    // user_login(Users[i].ID);
                    break;
                }
            }    
        }
        if (pid == 0){
            welcome(ssock);
            dup2(ssock, STDIN_FILENO);
            dup2(ssock, STDOUT_FILENO);
            dup2(ssock, STDERR_FILENO);
            close(msock);
            npshell(ssock);
        }
    }
    return 0;
}


void npshell(int ssock){   

    signal(SIGCHLD, signalHandler);
    signal(SIGINT, signalHandler);
    signal(SIGUSR2, signalHandler);

    // clearenv();
    while (true)
    {
        if (Users[user_id].pid == getpid())        
            break;        
        user_id++;
        if (user_id > 30){
            user_id = -1;
            return;
        }
            
    }
    user_login(user_id);
    setenv("PATH", "bin:.", 1);
    
    while(true){
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
            parsecmd(string_nospace,ssock,input);
        }
        
    }
}
void exe_cmd(Block block,int ID){
    char *argv[10000];
    int index;
    if(block.writefile){
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
    }
    else{
        vectochar(block.blockcmd, argv, block.blockcmd.size());
    }
    //cerr<<"ready exe"<<endl;
    if(execvp(block.blockcmd[0].data(), argv)<0){
        string Msg;
        Msg = "Unknown command: [" + block.blockcmd[0] + "].\n";
        write(Users[ID].ssock,Msg.c_str(),Msg.length());
    }
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
    for(int i=0;i<str.length();i++){
        if(str[i]=='+'){
            buffer+=a;
            index=i+1;
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
void parsecmd(vector<string> &vec,int ssock,string input){
    int status;
    int servingID = user_id;
    int catchfromID;
    int catchtoID;//which destuser is used
    int throwfromID;
    int throwtoID;
    // cout<<servingID<<endl;
    bool front = false;
    int pipecase=0;//0:no pipe 1:need pipe
    vector<Block> block;
    vector<string> next_cmd;
    int numpipeinline=0;
    string word;
    Block block1;
    int judge=0;
    // queue<pid_t> pids;
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
                block1.usercatch = false;
                block1.userthrow = false;
                block1.blockcmd.clear();
                block1.front_ispipe=front;
                block1.backpipe=false;
            }
        }
        else if(vec[i][0] == '>'){
            if(vec[i].size()!=1){
                string str = vec[i].erase(0,1);
                int a = stoi(str);//a is to/from which id
                // Userpipe userpipe1;            
                throwfromID = servingID;
                throwtoID = a-1;
                string fifoFileName = "./user_pipe/" + to_string(throwtoID * 30 + throwfromID);
                // cout<<"throw: "<<fifoFileName<<endl;
                if(!Users[throwtoID].hasUser){
                    string Msg = "*** Error: user #" + to_string(throwtoID+1)+" does not exist yet. ***\n";
                    write(Users[servingID].ssock,Msg.c_str(),Msg.length());
                    block1.dupnull = 1;
                }
                else if((mknod(fifoFileName.c_str(), S_IFIFO | 0777, 0) < 0) && (errno == EEXIST)){//userpipe[userpipe_number[throwfromID][throwtoID]].isusing
                    string Msg = "*** Error: the pipe #" + to_string(throwfromID+1) + "->#" + to_string(throwtoID+1) + " already exists. ***\n"; 
                    
                    write(Users[servingID].ssock,Msg.c_str(),Msg.length());
                    block1.pipeTo = -1;
                    block1.dupnull = 1;
                }
                else{
                    kill(Users[throwtoID].pid, SIGUSR2);
                    // cout<<"here"<<endl;
                    block1.userthrow = true;
                    int writeFD = open(fifoFileName.c_str(), 1);
                    block1.fifoToFD = writeFD;
                    // cout<<fifoFileName<<" :"<<writeFD<<endl;
                    block1.pipeTo = throwtoID;
                }
            }
            else{
                block1.blockcmd.push_back(vec[i]);//this > is write file
                block1.writefile=true;
            }
        }
        else if(vec[i][0] == '<'){        
            string str = vec[i].erase(0,1);
            int a = stoi(str);//a is to/from which id
            catchtoID = servingID;
            catchfromID = a-1;
            if(!Users[catchfromID].hasUser){
                string Msg = "*** Error: user #" + to_string(catchfromID+1)+" does not exist yet. ***\n";
                write(Users[servingID].ssock,Msg.c_str(),Msg.length());
                block1.dupnull = 2;                
            }
            else if (userPipeFDArray[catchfromID] == -1)
            {
                
                //* 不存在pipe
                string Msg = "*** Error: the pipe #" + to_string(catchfromID+1) + "->#" + to_string(catchtoID+1) + " does not exist yet. ***\n";
                write(Users[servingID].ssock,Msg.c_str(),Msg.length());
                block1.dupnull = 2;
                // parseCommand[parseCommandLine].pipeFrom = 0;
            }
            else{
                block1.usercatch = true;
                block1.pipeFrom = catchfromID;
                block1.fifoFromFD = userPipeFDArray[catchfromID];
                // cout<<"fifofromFD :"<<userPipeFDArray[catchfromID]<<endl;
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
        user_logout(servingID);
        // close(Users[servingID].ssock);
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        Users[servingID].hasUser=false;
        Users[servingID].ssock=0; 
        exit(0);
    }
    else if(block[0].blockcmd[0] == "who"){
        who(ssock);
    }
    else if(block[0].blockcmd[0] == "name"){
        rename(servingID,block[0].blockcmd[1]);
    }
    else if(block[0].blockcmd[0] == "tell"){
        int desID = stoi(block[0].blockcmd[1]);
        string tell_msg=block[0].blockcmd[2];
        for(int i=3;i<block[0].blockcmd.size();i++){
            tell_msg += " " + block[0].blockcmd[i];
        }
        tell(servingID,desID-1,tell_msg);
    }
    else if(block[0].blockcmd[0] == "yell"){
        string tell_msg=block[0].blockcmd[1];
        for(int i=2;i<block[0].blockcmd.size();i++){
            tell_msg += " " + block[0].blockcmd[i];
        }
        yell(servingID,tell_msg);
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
        string Msgcatch;
        string Msgthrow;
        if(block[0].usercatch){//當有指令'<'                        
            Msgcatch = "*** " + string(Users[catchtoID].nickname) + " (#" + to_string(catchtoID+1) + ") just received from " + string(Users[catchfromID].nickname) + " (#" + to_string(catchfromID+1) + ") by '" + input +"' ***\n";
            strcpy(BM[servingID].msg, Msgcatch.c_str());
            BM[servingID].used=true;
            kill(server_pid, SIGUSR1);
            while (BM[servingID].used)
                continue;
        }
        if(block[numpipeinline].userthrow){
            Msgthrow = "*** " + string(Users[throwfromID].nickname) + " (#" + to_string(throwfromID+1) + ") just piped '" + input + "' to " + string(Users[throwtoID].nickname) + " (#" + to_string(throwtoID+1) + ") ***\n";
            strcpy(BM[servingID].msg, Msgthrow.c_str());
            BM[servingID].used=true;
            kill(server_pid, SIGUSR1);
            while (BM[servingID].used)
                continue;
        }
        
        for(int i=0;i<numpipeinline+1;i++){ //判斷要fork幾次
            
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
                if ( block[i].pipeFrom != -1 && Users[block[i].pipeFrom].hasUser)
                {
                    // cout<<"block[i] "<<block[i].usercatch<<endl;
                    close(block[i].fifoFromFD);
                    userPipeFDArray[block[i].pipeFrom] = -1;
                    string fifoFileName = USERPIPE_PATH + to_string(user_id * 30 + block[i].pipeFrom);
                    if(block[i].usercatch){
                        if (unlink(fifoFileName.c_str()) < 0){
                            perror("unlink");
                        } 
                    }
                                           
                }
                if (block[i].pipeTo != -1)
                    close(block[i].fifoToFD);
            }
            else if(pid == 0){
                
                if (pipecase == 0) //我不需要pipe
                {
                    if(block[i].usercatch){
                        if (block[i].pipeFrom != -1){
                            dup2(block[i].fifoFromFD, STDIN_FILENO);
                            close(block[i].fifoFromFD);                   
                        }
                    }
                    if(block[i].userthrow){//當有指令'>'
                        if(block[i].pipeTo != -1){
                            dup2(block[i].fifoToFD, STDOUT_FILENO);
                            close(block[i].fifoToFD);
                        }                                                
                    }
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
                    if(block[i].dupnull==1){
                        dup2(FD_NULL, STDOUT_FILENO);
                        close(FD_NULL);
                    }
                    if(block[i].dupnull==2){
                        dup2(FD_NULL, STDIN_FILENO);
                        close(FD_NULL);
                    }
                    exe_cmd(block[i],servingID);
                }
                    
                else if(pipecase == 1){ //有出現"|" 需要pipe                
                    if(i == 0){//第一個process
                        if(block[i].usercatch){
                            if (block[i].pipeFrom != -1){
                                dup2(block[i].fifoFromFD, STDIN_FILENO);
                                close(block[i].fifoFromFD);                   
                            }
                        }
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
                    
                        if(block[i].userthrow){//當有指令'>'
                            if(block[i].pipeTo != -1){
                                dup2(block[i].fifoToFD, 1);
                                close(block[i].fifoToFD);
                            }                                                
                        }      
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
                    if(block[i].dupnull==1){
                        dup2(FD_NULL, STDOUT_FILENO);
                        close(FD_NULL);
                    }
                    if(block[i].dupnull==2){
                        dup2(FD_NULL, STDIN_FILENO);
                        close(FD_NULL);
                    }
                    exe_cmd(block[i],servingID);
                }
            }
        }
    }

    if(next_cmd.size()!=0){
        parsecmd(next_cmd,ssock,input);
    }

}

void signalHandler(int sig)
{
    if (sig == SIGCHLD){
        int status;
        waitpid(-1, &status, WNOHANG) ;
    }
        // pid_t pid = wait(NULL);
    else if (sig == SIGINT)
    {
        exit(0);
    }
    else if (sig == SIGUSR2)
    {
        // TODO: create , open FIFO read
        for (int i = 0; i < 30; i++)
        {
            if (Users[i].hasUser && userPipeFDArray[i] == -1)
            {
                string fifoName = USERPIPE_PATH + to_string(user_id * 30 + i);
                
                int readFD = open(fifoName.c_str(), 0);
                if (readFD < 0) // FIFO 不存在
                    continue;
                userPipeFDArray[i] = readFD;
                // cout<<fifoName<<endl;
            }
        }
    }
    else
        cout << "accept sig:" << sig << endl;
}

void logoutControl(int pid)
{
    // cout << "[Server] clients ptr:" << clients << endl;
    int fd, closeClientIndex;
    for (int index = 0; index < 30; index++)
    {
        if (Users[index].hasUser && Users[index].pid == pid)
        {
            // printClients(index);
            closeClientIndex = index;
            close(Users[index].ssock);
            Users[index].hasUser = false;
            break;
        }
    }
    // clean up fifo
    for (int i = 0; i < 30; i++)
    {
        string file1 = USERPIPE_PATH + to_string(30 * i + closeClientIndex);
        if ((unlink(file1.c_str()) < 0) && (errno != ENOENT))
            perror("unlink_file1");
        string file2 = USERPIPE_PATH + to_string(30 * closeClientIndex + i);
        if ((unlink(file2.c_str()) < 0) && (errno != ENOENT))
            perror("unlink_file2");
    }
}
void ServerSignalHandler(int sig)
{
    if (sig == SIGCHLD)
    {
        int pid, status;
        pid = wait(&status);
    }
    else if(sig == SIGUSR1){
        for(int i = 0; i < 30; i++){
            if(BM[i].used){
                if(BM[i].tell){
                    string output(BM[i].msg);
                    write(Users[BM[i].tellwho].ssock, output.c_str(), output.size());
                    BM[i].tell = false;
                }
                else if(!BM[i].sendto_me){
                    // cout<<"a"<<endl;
                    string output(BM[i].msg);
                    for(int j = 0; j < 30; j++){ 
                        if(Users[j].hasUser && BM[i].sendto_me_id != j){
                            write(Users[j].ssock, output.c_str(), output.size());
                        }                        
                    }
                    BM[i].sendto_me = true;
                }
                else{
                    string output(BM[i].msg);
                    for(int j = 0; j < 30; j++){ 
                        if(Users[j].hasUser){
                            write(Users[j].ssock, output.c_str(), output.size());
                        }                        
                    }
                }
                BM[i].used = false;
            }
        }
    }
    else if(sig == SIGINT){
        
        if (munmap(Users, sizeof(User) * 30) < 0)
            perror("munmap clients");
        if (remove(csfile.c_str()) < 0)
            perror("rm cs");

        if (munmap(BM, sizeof(broadcastMsg)) < 0)
            perror("munmap BM");
        if (remove(bmfile.c_str()) < 0)
            perror("rm bs");

        if (rmdir(SM_PATH.c_str()) < 0)
            perror("rmdir");

        for (int i = 0; i < 30; i++)
        {
            for (int j = 0; j < 30; j++)
            {
                string fifoFileName = "./user_pipe/" + to_string(30 * (i) + j);
                if ((unlink(fifoFileName.c_str()) < 0) && (errno != ENOENT))
                    perror("unlink_SIGINT");
            }
        }

        if (rmdir(USERPIPE_PATH.c_str()) < 0)
            perror("rmdir");
        exit(0);
    }
}

void init_share_memory(){
    if (NULL == opendir(SM_PATH.c_str()))
        mkdir(SM_PATH.c_str(), 0777);
    csfile = SM_PATH + "clientsSharedMemory";
    int clients_shared_momory_fd = open(csfile.c_str(), O_CREAT | O_RDWR, 00777);
    ftruncate(clients_shared_momory_fd, sizeof(User) * 30);
    Users = (User *)mmap(NULL, sizeof(User) * 30, PROT_READ | PROT_WRITE, MAP_SHARED, clients_shared_momory_fd, 0);
    if (Users == MAP_FAILED)
        perror("mmap");

    if (NULL == opendir(USERPIPE_PATH.c_str()))
        mkdir(USERPIPE_PATH.c_str(), 0777);
    bmfile = SM_PATH + "broadcastSharedMemory";
    int broadcast_shared_momory_fd = open(bmfile.c_str(), O_CREAT | O_RDWR, 00777);
    ftruncate(broadcast_shared_momory_fd, sizeof(broadcastMsg) * 30);
    BM = (broadcastMsg *)mmap(NULL, sizeof(broadcastMsg) * 30, PROT_READ | PROT_WRITE, MAP_SHARED, broadcast_shared_momory_fd, 0);
    if (BM == MAP_FAILED)
        perror("mmap");
}


void welcome(int ssock){
	string Msg;
	Msg+="****************************************\n";
	Msg+="** Welcome to the information server. **\n";
	Msg+="****************************************\n";
	write(ssock, Msg.c_str(), Msg.length()); //write() is syscall
}

void who(int ssock){
    string Msg;
    Msg="<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
    for(int i=0;i<30;i++){
        // User *c = &Users[index];
        if(Users[i].hasUser){
            // cout<<"a"<<endl;
            Msg += to_string(Users[i].ID + 1) + '\t' +  Users[i].nickname + '\t' +  Users[i].ip_addr;
            if(Users[i].ssock == ssock){
                Msg += "\t<-me";
            }
            Msg +='\n';
        }
    }
    write(ssock,Msg.c_str(),Msg.length());
}

void init_user(){
    for(int i=0;i<30;i++){
        Users[i].hasUser = false; 
        Users[i].ssock = 0;
        Users[i].ID = 0;
        Users[i].pid = -1;
        string envname="PATH";
        strcpy(Users[i].envname, envname.c_str());
        string envvalue="bin:.";
        strcpy(Users[i].envvalue, envvalue.c_str());
    }
}

void init_broad(){
    for(int i=0;i<30;i++){
        BM[i].used = false;
        BM[i].tell = false;
        BM[i].tellwho = 0;
        BM[i].sendto_me = true;
        BM[i].sendto_me_id = 0;
    }
}

void yell(int ID,string yell_msg){
    string Msg= "*** " + string(Users[ID].nickname) + " yelled ***: " + yell_msg + "\n";
    strcpy(BM[ID].msg, Msg.c_str());
    BM[ID].used=true;
    kill(server_pid, SIGUSR1);
    while (BM[ID].used)
        continue;
}

void rename(int ID,string newname){
    string Msg;
    for(int i=0;i<30;i++){//check someone has the same name
        if(Users[i].hasUser){
            if(newname == string(Users[i].nickname)){
                Msg = "*** User '" + newname +"' already exists. ***\n";
                write(Users[ID].ssock,Msg.c_str(),Msg.length());
                return;
            }
        }
    }
    strcpy(Users[ID].nickname, newname.c_str());
    // Users[ID].nickname = newname;
    Msg = "*** User from " + string(Users[ID].ip_addr) + " is named '" + string(Users[ID].nickname) + "'. ***\n";
    strcpy(BM[ID].msg, Msg.c_str());
    BM[ID].used=true;
    kill(server_pid, SIGUSR1);
    while (BM[ID].used)
        continue;
}

void tell(int fromID,int desID,string tell_msg){
    string Msg;
    if(!Users[desID].hasUser){
        Msg = "*** Error: user #" + to_string(desID + 1) +" does not exist yet. ***\n";
        write(Users[fromID].ssock,Msg.c_str(),Msg.length());
        return;
    }
    Msg = "*** " + string(Users[fromID].nickname) + " told you ***: " + tell_msg + "\n";
    
    strcpy(BM[fromID].msg, Msg.c_str());
    BM[fromID].used=true;
    BM[fromID].tell = true;
    BM[fromID].tellwho = desID;
    kill(server_pid, SIGUSR1);
    while (BM[fromID].used)
        continue;
}

void user_login(int ID){

	string Msg = "*** User '" + string(Users[ID].nickname) + "' entered from " + string(Users[ID].ip_addr) + ". ***\n";
    strcpy(BM[ID].msg, Msg.c_str());
    BM[ID].used=true;
    kill(server_pid, SIGUSR1);
    while (BM[ID].used)
        continue;
}

void user_logout(int ID){
    // int fd, closeClientIndex;
    // closeClientIndex = index;
    close(Users[ID].ssock);
    string Msg = "*** User '" + string(Users[ID].nickname) + "' left. ***\n";
	strcpy(BM[ID].msg, Msg.c_str());
    BM[ID].used=true;
    BM[ID].sendto_me = false;
    BM[ID].sendto_me_id = ID;
    kill(server_pid, SIGUSR1);
    while (BM[ID].used)
        continue;
    for (int i = 0; i < 30; i++)
    {
        string file1 = USERPIPE_PATH + to_string(30 * i + ID);
        if ((unlink(file1.c_str()) < 0) && (errno != ENOENT))
            perror("unlink_file1");
        string file2 = USERPIPE_PATH + to_string(30 * ID + i);
        if ((unlink(file2.c_str()) < 0) && (errno != ENOENT))
            perror("unlink_file2");
    }
}

void userPipeFDArray_value(){
    for (int i=0;i<30;i++){        
        userPipeFDArray[i]=-1;
    }
}