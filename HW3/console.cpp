#include <boost/asio/io_service.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string.hpp> // include Boost, a C++ library
#include <array>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>

using namespace std;
using boost::asio::ip::tcp;

typedef struct QUERY_STRING_data{
    string host="";
    string port="";
    string text="";
}QUERY_STRING_data;

QUERY_STRING_data request[5];

void parse_envirnoment(string QUERY_STRING);
void HTML();
void output_shell(string session, string content);
void output_command(string session, string content);
void escape(string& str);

class session : public std::enable_shared_from_this<session>
{
private:
    int ID;
    tcp::socket socket;
    tcp::resolver resolver;
    tcp::resolver::query query;
    tcp::endpoint endpoint;
    ifstream testfile;
    string session_s;
    enum { max_length = 1024 };
    char input[max_length];

public:
    session(boost::asio::io_context& io_context, int ID)
        : ID(ID), socket(io_context),resolver(io_context),query(request[ID].host, request[ID].port)
    {
        session_s = "s" + to_string(ID);
        string path = "./test_case/" + request[ID].text;
        testfile.open(path.data());

    }
    void start()
    {
        do_resolve();
    }

private:
    void do_resolve(){
        auto self(shared_from_this());
        resolver.async_resolve(query,[this, self](const boost::system::error_code ec, tcp::resolver::iterator iter){
                if(!ec) {
                    endpoint = *iter;
                    // cerr<<"do_resolve"<<endl;
                    do_connect();
                }else{
                    perror("do_resolve fail");
                }
                
            }
        );
    }

    void do_connect(){
        auto self(shared_from_this());
        socket.async_connect(endpoint,[this,self](boost::system::error_code ec){
            cerr<<endpoint<<endl;
            if(!ec){                
                // cerr<<"do_connect"<<endl;
                do_read();
            }
            else{
                perror("do_connect fail");
            }
        });
    }

    void do_read(){
        auto self(shared_from_this());
        socket.async_read_some(boost::asio::buffer(input, max_length),[this, self](boost::system::error_code ec, std::size_t length)
        {
            if(!ec){
                // cerr<<"do_read"<<endl;
                string Msg = input;
                bzero(input, sizeof(input));   
                // cerr<<"Msg"<<endl<<Msg<<endl<<endl;         
                output_shell(session_s, Msg);
                if(Msg.find("%") != std::string::npos) { // contain %
                    do_write();
                }
                else{
                    do_read();
                }
            }
        }
        );
    }

    void do_write(){
        // cerr<<"do_write"<<endl;
        auto self(shared_from_this());
        string line="";
        getline(testfile,line);
        if(line == "exit"){
            testfile.close();
        }
        line += '\n';
        // cerr << line <<endl;
        output_command(session_s,line);
        
        boost::asio::async_write(socket,boost::asio::buffer(line.c_str(), line.length()),[this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if(!ec) {
                do_read();
            }
        }
        );
    }
};

int main(){
    string QUERY_STRING;
    QUERY_STRING = getenv("QUERY_STRING");
    parse_envirnoment(QUERY_STRING);
    // for(int i=0;i<5;i++){
    //     cerr<<request[i].host<<endl;
    //     cerr<<request[i].port<<endl;
    //     cerr<<request[i].text<<endl;
    // }
    HTML();
    try{
        boost::asio::io_context io_context;
        for(int i = 0; i < 5; i++){
            if(request[i].host.size() == 0)
                break;
            make_shared<session>(io_context,i)->start();
        }
        io_context.run();
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}

void escape(string& str) {
    boost::replace_all(str, "&", "&amp;");
    boost::replace_all(str, "\"", "&quot;");
    boost::replace_all(str, "\'", "&apos;");
    boost::replace_all(str, "<", "&lt;");
    boost::replace_all(str, ">", "&gt;");
}

void output_shell(string session, string content){
    escape(content);
    boost::replace_all(content, "\n", "&NewLine;");
    boost::replace_all(content, "\r", "");
    cout<<"<script>document.getElementById('"<<session<<"').innerHTML += '"<<content<<"';</script>"<<endl;
}

void output_command(string session, string content){
    escape(content);
    boost::replace_all(content, "\n", "&NewLine;");
    boost::replace_all(content, "\r", "");
    cout<<"<script>document.getElementById('"<<session<<"').innerHTML += '<b>"<<content<<"</b>';</script>"<<endl;

}

void parse_envirnoment(string QUERY_STRING){
    QUERY_STRING = QUERY_STRING + "&";
    size_t index = 0;
    size_t front = 0; 
    int count=0;
    index = QUERY_STRING.find('&',front);
    while(index != std::string::npos){
        if((index-front)==3){ //no more information
            break;
        }
        string buf = QUERY_STRING.substr(front,index-front);       
        if(buf[0]=='h'){
            request[count].host = buf.substr(3);
        }
        else if(buf[0]=='p'){
            request[count].port = buf.substr(3);
        }
        else if(buf[0]=='f'){
            request[count].text = buf.substr(3);
            count++; 
        }
        front = index + 1;
        index = QUERY_STRING.find('&',front);
    }
}

void HTML(){
    cout<<"Content-type: text/html\r\n\r\n";
    cout<<"<!DOCTYPE html>"<<endl;
    cout<<"<html lang=\"en\">"<<endl;
    cout<<  "<head>"<<endl;
    cout<<    "<meta charset=\"UTF-8\" />"<<endl;
    cout<<    "<title>NP Project 3 Sample Console</title>"<<endl;
    cout<<    "<link"<<endl;
    cout<<    "rel=\"stylesheet\""<<endl;
    cout<<    "href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\""<<endl;
    cout<<    "integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\""<<endl;
    cout<<    "crossorigin=\"anonymous\""<<endl;
    cout<<    "/>"<<endl;
    cout<<    "<link"<<endl;
    cout<<    "href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\""<<endl;
    cout<<    "rel=\"stylesheet\""<<endl;
    cout<<    "/>"<<endl;
    cout<<    "<link"<<endl;
    cout<<    "rel=\"icon\""<<endl;
    cout<<    "type=\"image/png\""<<endl;
    cout<<    "href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\""<<endl;
    cout<<    "/>"<<endl;
    cout<<    "<style>"<<endl;
    cout<<    "* {"<<endl;
    cout<<        "font-family: 'Source Code Pro', monospace;"<<endl;
    cout<<        "font-size: 1rem !important;"<<endl;
    cout<<    "}"<<endl;
    cout<<    "body {"<<endl;
    cout<<        "background-color: #212529;"<<endl;
    cout<<    "}"<<endl;
    cout<<    "pre {"<<endl;
    cout<<        "color: #cccccc;"<<endl;
    cout<<    "}"<<endl;
    cout<<    "b {"<<endl;
    cout<<        "color: #01b468;"<<endl;
    cout<<    "}"<<endl;
    cout<<    "</style>"<<endl;
    cout<<"</head>"<<endl;
    cout<<"<body>"<<endl;
    cout<<    "<table class=\"table table-dark table-bordered\">"<<endl;
    cout<<    "<thead>"<<endl;
    cout<<        "<tr>"<<endl;
    int number=0;
    for(int i = 0; i < 5; i++){
        if(request[i].host.size()==0)
            break;
        number++;
        string addr = request[i].host + ':' + request[i].port;
        cout<<        "<th scope=\"col\">"<<addr<<"</th>"<<endl;
    }
    cout<<        "</tr>"<<endl;
    cout<<    "</thead>"<<endl;
    cout<<    "<tbody>"<<endl;
    cout<<        "<tr>"<<endl;
    for(int i = 0; i < number +1; i++){
        string s='s'+ to_string(i);
        cout<<    "<td><pre id=\""<< s <<"\" class=\"mb-0\"></pre></td>" <<endl;
    }
    cout<<        "</tr>"<<endl;
    cout<<    "</tbody>"<<endl;
    cout<<    "</table>"<<endl;
    cout<<"</body>"<<endl;
    cout<<"</html>"<<endl;
}


