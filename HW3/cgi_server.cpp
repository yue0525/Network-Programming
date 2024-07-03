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
#include <memory>
#include <unistd.h>

using boost::asio::ip::tcp;
using namespace std;
boost::asio::io_context io_context;

//http_server
void parsedata(string data);

//console
typedef struct QUERY_STRING_data{
    string host="";
    string port="";
    string text="";
}QUERY_STRING_data;

void parse_envirnoment(string QUERY_STRING);
void HTML();
string output_shell(string session, string content);
string output_command(string session, string content);
void escape(string& str);
string console();
string panel();
// typedef boost::shared_ptr<socket> socket_ptr;

QUERY_STRING_data request[5];

string REQUEST_METHOD;
string REQUEST_URI;
string QUERY_STRING;
string SERVER_PROTOCOL;
string HTTP_HOST;
string SERVER_ADDR;
string SERVER_PORT;
string REMOTE_ADDR;
string REMOTE_PORT;

class console_session : public std::enable_shared_from_this<console_session>
{
private:
    int ID;
    tcp::socket _socket;
    shared_ptr<tcp::socket> serversocket;
    tcp::resolver resolver;
    boost::asio::io_context& io_context;
    tcp::resolver::query query;
    tcp::endpoint endpoint;
    ifstream testfile;
    string session_s;
    enum { max_length = 1024 };
    char input[max_length]={0};
    string line;

public:
    console_session(boost::asio::io_context& io_context, int ID , shared_ptr<tcp::socket> sock)
        : ID(ID), _socket(io_context),serversocket(sock),resolver(io_context),io_context(io_context),query(request[ID].host, request[ID].port)
    {
        session_s = "s" + to_string(ID);
        string path = "./test_case/" + request[ID].text;
        cout<<path<<endl;
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
        _socket.async_connect(endpoint,[this,self](boost::system::error_code ec){
            cerr<<endpoint<<endl;
            if(!ec){                
                do_read();
            }
            else{
                perror("do_connect fail");
            }
        });
    }

    void do_read(){
        auto self(shared_from_this());
        _socket.async_read_some(boost::asio::buffer(input, max_length),[this, self](boost::system::error_code ec, std::size_t length)
        {
            if(!ec){                
                string Msg = input;    
                memset(input, '\0' ,sizeof(input));              
                cout<< Msg <<endl;  
                string output;   
                output = output_shell(session_s, Msg);
                boost::asio::async_write(*serversocket, boost::asio::buffer(output.c_str(), output.length()),[this, self](boost::system::error_code ec, std::size_t /*length*/)
                {
                    if(!ec) {
                       cout<<"output do_read failed"<<endl; 
                    }
                }
                );
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
        line="";
        getline(testfile,line);
        // cout<<"line :"<<line<<endl;
        if(line == "exit"){
            testfile.close();
        }
        line += '\n';
        // cerr << line <<endl;
        
        boost::asio::async_write(_socket,boost::asio::buffer(line.c_str(), line.length()),[this, self](boost::system::error_code ec, std::size_t length)
        {
            if(!ec){
                string buf1=line;
                string output;
                output = output_command(session_s,buf1);
                boost::asio::async_write(*serversocket, boost::asio::buffer(output.c_str(), output.length()),[this, self](boost::system::error_code ec, std::size_t /*length*/)
                {
                    if(!ec) {
                        do_read();
                    }
                }
                );
            }
        }
        );
        
    }
};
class server_session
  : public std::enable_shared_from_this<server_session>
{
public:
    server_session(tcp::socket socket)
        : socket_(std::move(socket))
    {
    }
    void start()
    {
        do_read();
    }

private:
    void do_read()
    {
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_, max_length),[this, self](boost::system::error_code ec, std::size_t length)
        {
            if (!ec)
            {         
                Msg = data_;
                
                // parsedata(Msg);
                cout<<"here"<<endl;
                cout<<Msg<<endl;
                do_write(length);
            }
        });          
    }
    void do_write(std::size_t length)
    {
        auto self(shared_from_this());
        // if(Msg.find("GET") != string::npos){
            if(Msg.find("GET /console.cgi") != string::npos){
                cout<<"console"<<endl;
                parsedata(Msg);
                parse_envirnoment(QUERY_STRING); 
                cout<<"a"<<endl;
                cout<<request[0].host<<" aq "<<endl;
                for(int i = 0; i < 5; i++){
                    if(request[i].host == "")
                        break;
                    cout<<request[i].host<<endl;
                    cout<<request[i].port<<endl;
                    cout<<request[i].text<<endl;
                }
                string console_output = console();
                               
                boost::asio::async_write(socket_, boost::asio::buffer(console_output.c_str(), console_output.length()),
                [this, self](boost::system::error_code ec, std::size_t /*length*/)
                {
                if (!ec)
                {
                    shared_ptr<tcp::socket> sock = make_shared<tcp::socket>(move(socket_));
                    for(int i = 0; i < 5; i++){
                        if(request[i].host.size() == 0)
                            break;
                        make_shared<console_session>(io_context, i, sock)->start();
                    }
                    do_read();
                }
                });
            }
            else if(Msg.find("GET /panel.cgi") != string::npos){
                cout<<"panel"<<endl;
                // cout<<Msg<<endl;
                string panel_output = panel();
                boost::asio::async_write(socket_, boost::asio::buffer(panel_output.c_str(), panel_output.length()),
                [this, self](boost::system::error_code ec, std::size_t /*length*/)
                {
                if (!ec)
                {
                    do_read();
                }
                });
            }
            
        // }
        // socket_.close();
    }

    tcp::socket socket_; //this socket is connect to rhe browser
    enum { max_length = 4096 };
    char data_[max_length];
    string Msg;
};

class server
{
public:
    server(boost::asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();
    }

private:
    void do_accept()
    {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket)
            {
            if (!ec)
            {
                std::make_shared<server_session>(std::move(socket))->start();
            }

            do_accept();
            });
    }

    tcp::acceptor acceptor_;
};


int main(int argc, char* argv[]){
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: async_tcp_echo_server <port>\n";
            return 1;
        }

        // boost::asio::io_context io_context;

        server s(io_context, std::atoi(argv[1]));

        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}




void parsedata(string data){

    int REQUEST_METHOD_Index = data.find(' ', 0);
    REQUEST_METHOD = data.substr(0,REQUEST_METHOD_Index);
    int URI_Index = data.find(' ', REQUEST_METHOD_Index + 1);
    string URI = data.substr(REQUEST_METHOD_Index + 1,URI_Index - REQUEST_METHOD_Index - 1);
    REQUEST_URI = URI;
    size_t index; 
    string cgi;
    index = URI.find('?', 0);
    if(index != std::string::npos){
        cgi = URI.substr(0,index);
        QUERY_STRING = URI.erase(0,index+1);
    }
    if(index == std::string::npos){
        cgi = URI;
        QUERY_STRING = "";
    }
    // cout<<QUERY_STRING<<endl;
    int SERVER_PROTOCOL_Index = data.find('\n',URI_Index+1);
    SERVER_PROTOCOL = data.substr(URI_Index+1,SERVER_PROTOCOL_Index - URI_Index -1);
    int HTTP_HOST_Index = data.find('\n',SERVER_PROTOCOL_Index+1);
    int indexhost = data.find(':',SERVER_PROTOCOL_Index+1);
    HTTP_HOST = data.substr(indexhost + 2,HTTP_HOST_Index - indexhost -2);
    cgi.insert(0,1,'.');
    // return cgi;
}


void escape(string& str) {
    boost::replace_all(str, "&", "&amp;");
    boost::replace_all(str, "\"", "&quot;");
    boost::replace_all(str, "\'", "&apos;");
    boost::replace_all(str, "<", "&lt;");
    boost::replace_all(str, ">", "&gt;");
}

string output_shell(string session, string content){
    string Msg="";
    escape(content);
    boost::replace_all(content, "\n", "&NewLine;");
    boost::replace_all(content, "\r", "");
    Msg += "<script>document.getElementById('"+ session + "').innerHTML += '" + content + "';</script>\n";
    return Msg;
}

string output_command(string session, string content){
    string Msg="";
    escape(content);
    boost::replace_all(content, "\n", "&NewLine;");
    boost::replace_all(content, "\r", "");
    Msg += "<script>document.getElementById('"+session+"').innerHTML += '<b>"+content+"</b>';</script>\n";
    return Msg;
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

string console(){
    string Msg="";
    Msg += "HTTP/1.1 200 OK\n";
    Msg += "Content-type: text/html\r\n\r\n";
    Msg += "<!DOCTYPE html>\n";
    Msg += "<html lang=\"en\">\n";
    Msg +=   "<head>\n";
    Msg +=     "<meta charset=\"UTF-8\" />\n";
    Msg +=     "<title>NP Project 3 Sample Console</title>\n";
    Msg +=     "<link\n";
    Msg +=     "rel=\"stylesheet\"\n";
    Msg +=     "href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\n";
    Msg +=     "integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\n";
    Msg +=     "crossorigin=\"anonymous\"\n";
    Msg +=     "/>\n";
    Msg +=     "<link\n";
    Msg +=     "href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n";
    Msg +=    "rel=\"stylesheet\"\n";
    Msg +=    "/>\n";
    Msg +=    "<link\n";
    Msg +=    "rel=\"icon\"\n";
    Msg +=    "type=\"image/png\"\n";
    Msg +=    "href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\n";
    Msg +=    "/>\n";
    Msg +=    "<style>\n";
    Msg +=    "* {\n";
    Msg +=        "font-family: 'Source Code Pro', monospace;\n";
    Msg +=        "font-size: 1rem !important;\n";
    Msg +=    "}\n";
    Msg +=    "body {\n";
    Msg +=        "background-color: #212529;\n";
    Msg +=    "}\n";
    Msg +=    "pre {\n";
    Msg +=        "color: #cccccc;\n";
    Msg +=    "}\n";
    Msg +=    "b {\n";
    Msg +=        "color: #01b468;\n";
    Msg +=    "}\n";
    Msg +=    "</style>\n";
    Msg +="</head>\n";
    Msg +="<body>\n";
    Msg +=    "<table class=\"table table-dark table-bordered\">\n";
    Msg +=    "<thead>\n";
    Msg +=        "<tr>\n";
    int number=0;
    for(int i = 0; i < 5; i++){
        if(request[i].host.size()==0)
            break;
        number++;
        string addr = request[i].host + ':' + request[i].port;
        Msg +=        "<th scope=\"col\">" + addr + "</th>\n";
    }
    Msg +=        "</tr>\n";
    Msg +=    "</thead>\n";
    Msg +=    "<tbody>\n";
    Msg +=        "<tr>\n";
    for(int i = 0; i < number ; i++){
        string s='s'+ to_string(i);
        Msg +=    "<td><pre id=\"" + s + "\" class=\"mb-0\"></pre></td>\n";
    }
    Msg +=        "</tr>\n";
    Msg +=    "</tbody>\n";
    Msg +=    "</table>\n";
    Msg +="</body>\n";
    Msg +="</html>\n";
    return Msg;
}


string panel(){
    string host_menu = "";
    for (int i=1; i<13; i++){
        host_menu = host_menu + "<option value=\"nplinux" + to_string(i) + ".cs.nctu.edu.tw\">nplinux" + to_string(i) + "</option>";
    }
    string test_case_menu = "";
    for (int i=1; i<6; i++){
        test_case_menu = test_case_menu + "<option value=\"t" + to_string(i) + ".txt\">t" + to_string(i) + ".txt</option>";
    }
    string Msg="";
    Msg += "HTTP/1.1 200 OK\n";
    Msg += "Content-type: text/html\r\n\r\n";
    Msg += "\
    <!DOCTYPE html>\
    <html lang=\"en\">\
    <head>\
    <title>NP Project 3 Panel</title>\
    <link\
    rel=\"stylesheet\"\
    href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\
    integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\
    crossorigin=\"anonymous\"\
    />\
    <link\
      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\
      rel=\"stylesheet\"\
    />\
    <link\
      rel=\"icon\"\
      type=\"image/png\"\
      href=\"https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png\"\
    />\
    <style>\
      * {\
        font-family: 'Source Code Pro', monospace;\
      }\
    </style>\
  </head>\
  <body class=\"bg-secondary pt-5\">\
  <form action=\"console.cgi\" method=\"GET\">\
      <table class=\"table mx-auto bg-light\" style=\"width: inherit\">\
        <thead class=\"thead-dark\">\
          <tr>\
            <th scope=\"col\">#</th>\
            <th scope=\"col\">Host</th>\
            <th scope=\"col\">Port</th>\
            <th scope=\"col\">Input File</th>\
          </tr>\
        </thead>\
        <tbody>";
    for (int i=0;i<5;i++){
        Msg+="\
            <tr>\
                <th scope=\"row\" class=\"align-middle\">Session " + to_string(i + 1) + "</th>\
                <td>\
                    <div class=\"input-group\">\
                        <select name=h" + to_string(i) + " class=\"custom-select\">\
                            <option></option>" + host_menu + "\
                        </select>\
                        <div class=\"input-group-append\">\
                            <span class=\"input-group-text\">.cs.nctu.edu.tw</span>\
                        </div>\
                    </div>\
                </td>\
                <td>\
                    <input name=\"p"+ to_string(i) +"\" type=\"text\" class=\"form-control\" size=\"5\" />\
                </td>\
                <td>\
                <select name=\"f" + to_string(i) + "\" class=\"custom-select\">\
                    <option></option>" + test_case_menu + "\
                </select>\
                </td>\
            </tr>";
    }
    Msg += "\
        <tr>\
            <td colspan=\"3\"></td>\
            <td>\
              <button type=\"submit\" class=\"btn btn-info btn-block\">Run</button>\
            </td>\
          </tr>\
        </tbody>\
      </table>\
    </form>\
  </body>\
</html>\n";

return Msg;
}