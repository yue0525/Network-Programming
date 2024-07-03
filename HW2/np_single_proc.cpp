#include<iostream>
#include<stdlib.h>
#include<map>
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
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h> 
#include <arpa/inet.h> //htons / ip_addr 100....->192.0.0.0
#include <errno.h>

using namespace std;

typedef struct Userpipe{
    // int fromid;
    // int desid;
    bool closepipe=false;
    bool isusing=false; // 判斷是否已經存在
    int fd[2];
}Userpipe;

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
}Block;

typedef struct Numpipe{
    int fd[2];
    int timer=-1;//是否到你pipe了
}Numpipe;

typedef struct User{
	bool hasUser = false;  
	int ssock = 0;
	string ip_addr = "";
	int ID = 0;
	string nickname = "(no name)";
    vector<Numpipe> User_npipes;
	string envVal_name="PATH";
    string envVal_value="bin:.";
}User;



void sigchld_handler(int );

// vector<Numpipe> numpipe;

void init_input(vector<Block> &block,int &numpipeinline,vector<string> &string_nospace);

void exe_cmd(Block block,int ID);

int findindex(vector<string> &vec, string str);

void vectochar(vector<string> &vec, char *arr[], int index);
int oper(string &str);
vector<string> SplitString(const string line, const string delimeter);

void parsecmd(vector<string> &vec,int ID,string input);

void npshell(int ID);
void welcome(int ID);
void user_login(int ID);
void user_logout(int ID);
void broadcast(int fromID,string broadcast_Msg);
void who(int ID);
void rename(int ID,string newname);
void tell(int fromID,int desID,string tell_msg);
void yell(int ID,string tell_msg);
void init_userset(int ID,int ssock,string IP);
//global parameters
fd_set rfds; /* read file descriptor set */
fd_set afds; /* active file descriptor set */
User Users[30];
const string prompt = "% ";
vector<Userpipe> userpipe;
void userpipe_number_value();
// map<int,map<int,int>> userpipe_number;
int userpipe_number[30][30];
// map<map<int,int>,int> userpipe_number1;
const int FD_NULL = open("/dev/null", O_RDWR);

int main(int argc, char *argv[]){
    userpipe_number_value();
    int port = atoi(argv[1]);
    int msock,ssock;
	int alen; /* from-address length */
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
	
	FD_ZERO(&afds);
	FD_ZERO(&rfds);

    userpipe.clear();
    clearenv();
	FD_SET(msock, &afds);//to set a sit for server
    listen(msock, 30);
	int nfds = getdtablesize();
	while(true){
        memcpy(&rfds, &afds, sizeof(rfds)); // copy afds to rfds for size(rfds)
        
        int stat,error;	
		do{
            stat = select(nfds, &rfds, (fd_set *)0, (fd_set *)0,(struct timeval *)0);
            if (stat < 0){
                error = errno;
            }
        }while ((stat < 0) && (error == 4));//EINTR=4	
        		        
		if (FD_ISSET(msock, &rfds)) {
			unsigned int clilen;
			clilen = sizeof(cli_addr);
			
			for(int i = 0; i < 30; i++){
				if (Users[i].hasUser==true){
					continue;
				}
				else{
					ssock = accept(msock, (struct sockaddr *) &cli_addr, &clilen);
					if (ssock < 0){
						cout<<"accept error";
					}
                    string IP=inet_ntoa(cli_addr.sin_addr);
                    IP += ":" + to_string(ntohs(cli_addr.sin_port));
                    init_userset(i,ssock,IP);		
					welcome(Users[i].ID);
					user_login(Users[i].ID);
                    write(Users[i].ssock,prompt.c_str(),prompt.length());
                    break;
				}				
			}
		}
        
		for (int fd = 0; fd < 30; fd++){
			if (Users[fd].ssock != msock && FD_ISSET(Users[fd].ssock, &rfds)){
                		
                dup2(Users[fd].ssock, STDOUT_FILENO);
                dup2(Users[fd].ssock, STDERR_FILENO);
                // cout<<msock<<endl;		
                npshell(fd);
			}
        }
	}
}

void broadcast(int fromID , string broadcast_Msg){
    for(int i=0;i<30;i++){
        if(Users[i].hasUser && i != fromID){
            write(Users[i].ssock,broadcast_Msg.c_str(),broadcast_Msg.length());
        }
    }
}
void init_userset(int ID,int ssock,string IP){
    Users[ID].hasUser = true;
    Users[ID].ID = ID;
    Users[ID].ip_addr = IP;
    Users[ID].ssock = ssock;
    FD_SET(Users[ID].ssock, &afds);//to set a sit for client
}
void who(int ID){
    string Msg;
    Msg="<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
    for(int i=0;i<30;i++){
        if(Users[i].hasUser){
            Msg += to_string(Users[i].ID + 1) + '\t' +  Users[i].nickname + '\t' +  Users[i].ip_addr;
            if(Users[i].ID == ID){
                Msg += "\t<-me";
            }
            Msg +='\n';
        }
    }
    write(Users[ID].ssock,Msg.c_str(),Msg.length());
}
void rename(int ID,string newname){
    string Msg;
    for(int i=0;i<30;i++){//check someone has the same name
        if(Users[i].hasUser){
            if(newname == Users[i].nickname){
                Msg = "*** User '" + newname +"' already exists. ***\n";
                write(Users[ID].ssock,Msg.c_str(),Msg.length());
                return;
            }
        }
    }
    Users[ID].nickname = newname;
    Msg = "*** User from " + Users[ID].ip_addr + " is named '" + Users[ID].nickname + "'. ***\n";
    broadcast(ID,Msg);
    write(Users[ID].ssock,Msg.c_str(),Msg.length());
}
void tell(int fromID,int desID,string tell_msg){
    string Msg;
    if(!Users[desID].hasUser){
        Msg = "*** Error: user #" + to_string(desID + 1) +" does not exist yet. ***\n";
        write(Users[fromID].ssock,Msg.c_str(),Msg.length());
        return;
    }
    Msg = "*** " + Users[fromID].nickname + " told you ***: " + tell_msg + "\n";
    write(Users[desID].ssock,Msg.c_str(),Msg.length());
}
void yell(int ID,string yell_msg){
    string Msg;
    Msg = "*** " + Users[ID].nickname + " yelled ***: " + yell_msg + "\n";
    write(Users[ID].ssock,Msg.c_str(),Msg.length());
    broadcast(ID,Msg);
}
void welcome(int ID){
	string Msg;
	Msg+="****************************************\n";
	Msg+="** Welcome to the information server. **\n";
	Msg+="****************************************\n";
	write(Users[ID].ssock, Msg.c_str(), Msg.length()); //write() is syscall
}
void user_login(int ID){
	string Msg = "*** User '" + Users[ID].nickname + "' entered from " + Users[ID].ip_addr + ". ***\n";
	write(Users[ID].ssock, Msg.c_str(), Msg.length());
    broadcast(ID,Msg);
}
void user_logout(int ID){
    string Msg = "*** User '" + Users[ID].nickname + "' left. ***\n";
	// write(Users[ID].ssock, Msg.c_str(), Msg.length());
    broadcast(ID,Msg);
}
void npshell(int ID){    
	int servingID = ID;
    
	if (!Users[servingID].hasUser){
		return;
	}
    char buf [15000];    
    setenv(Users[servingID].envVal_name.c_str() , Users[servingID].envVal_value.c_str() , 1);
    string input;
    
    bzero(buf, sizeof(buf));
    int cc = read(Users[servingID].ssock, buf, sizeof(buf));
    if (cc<0){
        cout << servingID << ": read error." << endl;
        return;
    }
    input = buf;
    
    if (input[input.length()-1] == '\n'){
        input = input.substr(0, input.length()-1);
    }
    if (input[input.length()-1] == '\r'){
        input = input.substr(0, input.length()-1);
    }
    if (input.length() == 0){
        write(Users[servingID].ssock,prompt.c_str(),prompt.length());
        return;
    }
    	
    vector<string> string_nospace;
    string_nospace = SplitString(input," ");
    if(string_nospace.size()>0){
        parsecmd(string_nospace,servingID,input);
        write(Users[servingID].ssock,prompt.c_str(),prompt.length());
    }
}
void userpipe_number_value(){
    for (int i=0;i<30;i++){
        for (int j=0;j<30;j++){
            userpipe_number[i][j]=-1;
        }
    }
}

void exe_cmd(Block block, int ID){
    char *argv[10000];
    int index;
    // cout<<"a"<<block.writefile<<endl;
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
    
    if(execvp(block.blockcmd[0].data(), argv)<0){
        string Msg;
        Msg = "Unknown command: [" + block.blockcmd[0] + "].\n";
        write(Users[ID].ssock,Msg.c_str(),Msg.length());
    }
        // cerr << "Unknown command: [" << block.blockcmd[0].data() << "]." << endl;
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
            str1 = str1.assign(str,index,back_size+1);
            a = stoi(str1);
        }
    }
    buffer+=a;
    return buffer;
}
void parsecmd(vector<string> &vec,int ID,string input){
    int status;
    int servingID = ID;
    int catchfromID;
    int catchtoID;//which destuser is used
    int throwfromID;
    int throwtoID;
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
                block1.back=true;
                int index;
                for(index=0;index<Users[servingID].User_npipes.size();index++){
                    if(Users[servingID].User_npipes[index].timer == a){
                        createpipeornot=false;
                        break;
                    }
                }
                if(createpipeornot){ //true -->create
                    // cout<<"a"<<endl;                   
                    Numpipe numpipe1;
                    int pipe_fd1=pipe(numpipe1.fd);
                    if (pipe_fd1 == -1){
                        perror("c8 cannot pipe");
                    }
                    numpipe1.timer = a;
                    // block1.count = numpipe.size()
                    block1.count = Users[servingID].User_npipes.size();
                    Users[servingID].User_npipes.push_back(numpipe1);
                }
                if(!createpipeornot){
                    block1.count = index ;
                }    
                for(int j=i;j<vec.size()-1;j++){
                    next_cmd.push_back(vec[j+1]);
                }
                if(a==1)
                    judge=1;
                
                for(int k=0;k<Users[servingID].User_npipes.size();k++){
                    if(Users[servingID].User_npipes[k].timer == 1){
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
        else if(vec[i][0] == '>'){
            if(vec[i].size()!=1){
                string str = vec[i].erase(0,1);
                int a = stoi(str);//a is to/from which id
                Userpipe userpipe1;            
                throwfromID = servingID;
                throwtoID = a-1;
                if(!Users[throwtoID].hasUser){
                    string Msg = "*** Error: user #" + to_string(throwtoID+1)+" does not exist yet. ***\n";
                    write(Users[servingID].ssock,Msg.c_str(),Msg.length());
                    block1.dupnull = 1;
                    // dup2(FD_NULL, STDOUT_FILENO);
                }
                else if(userpipe_number[throwfromID][throwtoID] >= 0){//userpipe[userpipe_number[throwfromID][throwtoID]].isusing
                    string Msg = "*** Error: the pipe #" + to_string(throwfromID+1) + "->#" + to_string(throwtoID+1) + " already exists. ***\n"; 
                    write(Users[servingID].ssock,Msg.c_str(),Msg.length());
                    block1.dupnull = 1;
                    // dup2(FD_NULL, STDOUT_FILENO);
                }
                else{
                    block1.userthrow = true;                
                    int pipe_fd1=pipe(userpipe1.fd);
                    if (pipe_fd1 == -1){
                        perror("c8 cannot pipe");
                    }
                    // map<int,int> buf1;
                    // buf1[a-1] = userpipe.size();
                    // userpipe_number[servingID] = buf1;
                    // cout<<userpipe.size()<<endl;
                    userpipe_number[throwfromID][throwtoID] = userpipe.size();
                    // int jk = userpipe.size();
                    // userpipe_number1[servingID]=a-1;
                    // userpipe1.desid = a-1;
                    // userpipe1.fromid = servingID;
                    userpipe.push_back(userpipe1);
                }
            }
            else{
                // cout<<"write"<<endl;
                block1.blockcmd.push_back(vec[i]);//this > is write file
                block1.writefile=true;
            }  
            
        }
        else if(vec[i][0] == '<'){
            // block1.usercatch = true;            
            string str = vec[i].erase(0,1);
            int a = stoi(str);//a is to/from which id
            catchtoID = servingID;
            catchfromID = a-1;
            // cout<<userpipe_number[catchfromID][catchtoID]<<endl;
            if(!Users[catchfromID].hasUser){
                string Msg = "*** Error: user #" + to_string(catchfromID+1)+" does not exist yet. ***\n";
                write(Users[servingID].ssock,Msg.c_str(),Msg.length());
                // dup2(FD_NULL, STDIN_FILENO);
                block1.dupnull = 2;                
            }

            // else if(!userpipe_number[catchfromID].count(catchtoID))
            else if(userpipe_number[catchfromID][catchtoID]<0)
            {
                // cout<<userpipe_number[catchfromID][catchtoID]<<endl;
                string Msg = "*** Error: the pipe #" + to_string(catchfromID+1) + "->#" + to_string(catchtoID+1) + " does not exist yet. ***\n";
                write(Users[servingID].ssock,Msg.c_str(),Msg.length());
                block1.dupnull = 2;
            }
            else{
                // cout<<userpipe_number[catchfromID][catchtoID]<<endl;
                block1.usercatch = true;
                userpipe[userpipe_number[catchfromID][catchtoID]].closepipe = true;//代表你會接到東西 所以需要關掉
                block1.catchnum=userpipe_number[catchfromID][catchtoID];
                // userpipe_number[catchfromID].erase(catchtoID);
                // userpipe_number.erase(catchfromID);    
                userpipe_number[catchfromID][catchtoID] = -1;
            }     
        }
        else{
            block1.blockcmd.push_back(vec[i]);
        }
    }
    block1.front_ispipe=front;
    block.push_back(block1);

    if(block[0].blockcmd[0] == "printenv"){
        if (getenv(block[0].blockcmd[1].data()) != NULL){  
            // dup2(Users[servingID].ssock, STDOUT_FILENO);          
            string Msg = getenv(block[0].blockcmd[1].data());
            Msg +='\n';
            write(Users[servingID].ssock,Msg.c_str(),Msg.length());
        }
    }
    else if(block[0].blockcmd[0] == "setenv"){
        Users[servingID].envVal_name=block[0].blockcmd[1];
        Users[servingID].envVal_value=block[0].blockcmd[2];
        // dup2(Users[servingID].ssock, STDOUT_FILENO);
        setenv(block[0].blockcmd[1].data(),block[0].blockcmd[2].data(),1);
    }
    else if (block[0].blockcmd[0] == "exit" || block[0].blockcmd[0] == "EOF") {
        user_logout(servingID);
        // cout<<servingID<<endl;
        // cout<<Users[servingID].ssock<<endl;
        FD_CLR(Users[servingID].ssock, &afds);
		close(Users[servingID].ssock);
        Users[servingID].hasUser=false;
        Users[servingID].ssock=0; 
        Users[servingID].ip_addr=""; 
        Users[servingID].nickname = "(no name)";
        Users[servingID].envVal_name="PATH"; 
        Users[servingID].envVal_value="bin:."; 

        for (int i=0; i<Users[servingID].User_npipes.size(); i++){
            close(Users[servingID].User_npipes[i].fd[0]);
            close(Users[servingID].User_npipes[i].fd[1]);
        }
        Users[servingID].User_npipes.clear();

        for(int i=0;i<30;i++){
            userpipe_number[servingID][i] = -1;
            userpipe_number[i][servingID] = -1;           
        }
		while(waitpid(-1, &status, WNOHANG) > 0){};
        return;
    }
    else if(block[0].blockcmd[0] == "who"){
        who(servingID);
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
        // int status;
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
        string Msgcatch;
        string Msgthrow;
        if(block[0].usercatch){//當有指令'<'                        
            // if(userpipe_number[catchfromID].count(catchtoID)>0){//這catchfromID -> catchtoID有東西要傳近來                          
            dup2(userpipe[block[0].catchnum].fd[0],STDIN_FILENO);
            close(userpipe[block[0].catchnum].fd[0]);
            close(userpipe[block[0].catchnum].fd[1]);
            Msgcatch = "*** " + Users[catchtoID].nickname + " (#" + to_string(catchtoID+1) + ") just received from " + Users[catchfromID].nickname + " (#" + to_string(catchfromID+1) + ") by '" + input +"' ***\n";
            write(Users[servingID].ssock,Msgcatch.c_str(),Msgcatch.length());
            broadcast(servingID,Msgcatch);                 
        }
        for(int i=0;i<numpipeinline+1;i++){
            if((pid = fork()) < 0){//你再搞
                i--;//這個i不算
            }
            else if(pid == 0){
                // dup2(Users[ID].ssock, STDOUT_FILENO);
                if (pipecase == 0) //我不需要pipe
                {   
                    
                    if(block[i].userthrow){//當有指令'>'
                        // if(userpipe_number[throwfromID].count(throwtoID)){
                        if(userpipe_number[throwfromID][throwtoID] >= 0){
                            dup2(userpipe[userpipe_number[throwfromID][throwtoID]].fd[1],STDOUT_FILENO);
                            userpipe[userpipe_number[throwfromID][throwtoID]].isusing=true;//告知此pipe已經正在使用
                            Msgthrow = "*** " + Users[throwfromID].nickname + " (#" + to_string(throwfromID+1) + ") just piped '" + input + "' to " + Users[throwtoID].nickname + " (#" + to_string(throwtoID+1) + ") ***\n";
                            write(Users[servingID].ssock,Msgthrow.c_str(),Msgthrow.length());
                            broadcast(servingID,Msgthrow);
                        }                                                
                    }
                    if(block[i].back){
                        if(!block[i].error)
                            dup2(Users[servingID].User_npipes[block[i].count].fd[1],STDOUT_FILENO);
                        if(block[i].error){
                            dup2(Users[servingID].User_npipes[block[i].count].fd[1],STDOUT_FILENO);
                            dup2(Users[servingID].User_npipes[block[i].count].fd[1],STDERR_FILENO);
                        }
                    }
                    for(int j=0;j<Users[servingID].User_npipes.size();j++){
                        if(Users[servingID].User_npipes[j].timer==0){
                            dup2(Users[servingID].User_npipes[j].fd[0],STDIN_FILENO);
                            close(Users[servingID].User_npipes[j].fd[0]);
                            close(Users[servingID].User_npipes[j].fd[1]);
                            // dup2(Users[servingID].ssock, STDOUT_FILENO);
                        }
                    }
                    if(block[i].dupnull==1){
                        dup2(FD_NULL, STDOUT_FILENO);
                    }
                    if(block[i].dupnull==2){
                        dup2(FD_NULL, STDIN_FILENO);
                    }
                    exe_cmd(block[i],servingID);
                }

                else if(pipecase == 1){ //有出現"|" 需要pipe 
                    string Msgcatch;
                    string Msgthrow;          
                    if(i == 0){//第一個process
                        for(int j=0;j<Users[servingID].User_npipes.size();j++){
                            if(Users[servingID].User_npipes[j].timer == 0){
                                dup2(Users[servingID].User_npipes[j].fd[0],STDIN_FILENO);
                                close(Users[servingID].User_npipes[j].fd[0]);
                                close(Users[servingID].User_npipes[j].fd[1]);
                            }
                        }
                        if(!block[i].error){
                            dup2(pipearray[i][1],STDOUT_FILENO);
                        }                            
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
                            // if(userpipe_number[throwfromID].count(throwtoID)){
                            if(userpipe_number[throwfromID][throwtoID]){
                                dup2(userpipe[userpipe_number[throwfromID][throwtoID]].fd[1],STDOUT_FILENO);
                                userpipe[userpipe_number[throwfromID][throwtoID]].isusing=true;//告知此pipe已經正在使用
                                Msgthrow = "*** " + Users[throwfromID].nickname + " (#" + to_string(throwfromID+1) + ") just piped '" + input + "' to " + Users[throwtoID].nickname + " (#" + to_string(throwtoID+1) + ") ***\n";
                                write(Users[servingID].ssock,Msgthrow.c_str(),Msgthrow.length());
                                broadcast(servingID,Msgthrow);
                            }                                                
                        }            
                        if(block[i].back){
                            if(!block[i].error)     
                                dup2(Users[ID].User_npipes[block[i].count].fd[1],STDOUT_FILENO);
                            if(block[i].error){
                                dup2(Users[ID].User_npipes[block[i].count].fd[1],STDERR_FILENO);
                                dup2(Users[ID].User_npipes[block[i].count].fd[1],STDOUT_FILENO);
                            }
                            close(Users[ID].User_npipes[block[i].count].fd[0]);
                            close(Users[ID].User_npipes[block[i].count].fd[1]);
                        }
                        dup2(pipearray[i-1][0],STDIN_FILENO);
                        close(pipearray[i-1][0]);
                        close(pipearray[i-1][1]);
                        
                    }
                    if(block[i].dupnull==1){
                        dup2(FD_NULL, STDOUT_FILENO);
                    }
                    if(block[i].dupnull==2){
                        dup2(FD_NULL, STDIN_FILENO);
                    }
                    exe_cmd(block[i],servingID);
                }
            }
            else if(pid > 0){//parent 
                if(!block[i].front_ispipe){
                    for(int k=0;k<Users[servingID].User_npipes.size();k++){
                        Users[servingID].User_npipes[k].timer--;                    
                    }
                }
                
                if(i>0){
                    close(pipearray[i-1][0]);
                    close(pipearray[i-1][1]);
                }

                for(int j=0;j<Users[servingID].User_npipes.size();j++){
                    if(Users[servingID].User_npipes[j].timer==-1){
                        close(Users[servingID].User_npipes[j].fd[0]);
                        close(Users[servingID].User_npipes[j].fd[1]);
                    }
                }
                // cout<<"aaa"<<endl;
                for(int j=0;j<userpipe.size();j++){
                    // cout<<servingID<<"j="<<j<<" "<<userpipe[j].closepipe<<endl;
                    if(userpipe[j].closepipe){
                        // cout<<"a"<<endl;
                        close(userpipe[j].fd[0]);
                        close(userpipe[j].fd[1]);
                        userpipe[j].closepipe=false;
                    }
                }
                if(!block[i].backpipe)
                    waitpid( pid, &status, 0 );//等到目前這個child回傳
            }
        }
    }

    if(next_cmd.size()!=0){
        parsecmd(next_cmd,servingID,input);
    }
    return;
}
void sigchld_handler(int sig)
{
    int status;
    waitpid(-1, &status, WNOHANG) ;
}

