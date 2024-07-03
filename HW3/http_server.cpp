#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include<string>

using boost::asio::ip::tcp;
using namespace std;

string parsedata(string data);

// auto printenv = []() {
//   // std::cout << "EXEC_FILE: " << getenv("EXEC_FILE") << std::endl;
//   std::cout << "REQUEST_METHOD: " << getenv("REQUEST_METHOD") << std::endl;
//   std::cout << "REQUEST_URI: " << getenv("REQUEST_URI") << std::endl;
//   std::cout << "QUERY_STRING: " << getenv("QUERY_STRING") << std::endl;
//   std::cout << "SERVER_PROTOCOL: " << getenv("SERVER_PROTOCOL") << std::endl;
//   std::cout << "HTTP_HOST: " << getenv("HTTP_HOST") << std::endl;
//   std::cout << "SERVER_ADDR: " << getenv("SERVER_ADDR") << std::endl;
//   std::cout << "SERVER_PORT: " << getenv("SERVER_PORT") << std::endl;
//   std::cout << "REMOTE_ADDR: " << getenv("REMOTE_ADDR") << std::endl;
//   std::cout << "REMOTE_PORT: " << getenv("REMOTE_PORT") << std::endl;
// };

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
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
            do_write(length);    
          }
        });          
  }
  const string http_ok ="HTTP/1.1 200 OK";
  void do_write(std::size_t length)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(http_ok, length),
    [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          { 
            string buffer = data_;//use buffer to set the environment
            string cgi = parsedata(data_);
            setenv("SERVER_ADDR",socket_.local_endpoint().address().to_string().c_str(),1);
            setenv("SERVER_PORT",to_string(socket_.local_endpoint().port()).c_str(),1);
            setenv("REMOTE_ADDR",socket_.remote_endpoint().address().to_string().c_str(),1);
            setenv("REMOTE_PORT",to_string(socket_.remote_endpoint().port()).c_str(),1);
            // printenv();
            pid_t pid;  
            if ((pid = fork()) < 0){
              cout<<"fork error"<<endl;
            }
            else if(pid == 0){//children
              char** arg = new char* [2];
              arg[0]=strdup(cgi.c_str());
              arg[1]=NULL;
              int sock = socket_.native_handle();
              // dup2(sock,STDERR_FILENO);
              dup2(sock,STDOUT_FILENO);
              dup2(sock,STDIN_FILENO);
              if(execvp(arg[0],arg)<0){
                  // cerr<<strerror(errno)<<endl;
                  exit(EXIT_FAILURE);
              }
              exit(0);
            } 
            else{//parent
              socket_.close();
            }
          }
        });
  }

  tcp::socket socket_;
  enum { max_length = 1024 };
  char data_[max_length];
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
            std::make_shared<session>(std::move(socket))->start();
          }

          do_accept();
        });
  }

  tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: async_tcp_echo_server <port>\n";
      return 1;
    }

    boost::asio::io_context io_context;

    server s(io_context, std::atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}


string parsedata(string data){
  // cout<<data<<endl;
  int REQUEST_METHOD_Index = data.find(' ', 0);
  string REQUEST_METHOD = data.substr(0,REQUEST_METHOD_Index);
  setenv("REQUEST_METHOD",REQUEST_METHOD.c_str(),1);
  int URI_Index = data.find(' ', REQUEST_METHOD_Index + 1);
  string URI = data.substr(REQUEST_METHOD_Index + 1,URI_Index - REQUEST_METHOD_Index - 1);
  size_t index; 
  string cgi;
  index = URI.find('?', 0);
  if(index != std::string::npos){
    cgi = URI.substr(0,index);
    setenv("REQUEST_URI",URI.c_str(),1);
    string QUERY_STRING = URI.erase(0,index+1);
    setenv("QUERY_STRING",QUERY_STRING.c_str(),1);
  }
  if(index == std::string::npos){
    cgi = URI;
    setenv("REQUEST_URI",URI.c_str(),1);
    setenv("QUERY_STRING","",1);
  }
  int SERVER_PROTOCOL_Index = data.find('\n',URI_Index+1);
  string SERVER_PROTOCOL = data.substr(URI_Index+1,SERVER_PROTOCOL_Index - URI_Index -1);
  setenv("SERVER_PROTOCOL",SERVER_PROTOCOL.c_str(),1);
  int HTTP_HOST_Index = data.find('\n',SERVER_PROTOCOL_Index+1);
  int indexhost = data.find(':',SERVER_PROTOCOL_Index+1);
  string HTTP_HOST = data.substr(indexhost + 2,HTTP_HOST_Index - indexhost -2);
  setenv("HTTP_HOST",HTTP_HOST.c_str(),1);
  cgi.insert(0,1,'.');
  return cgi;
}