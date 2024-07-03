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
#include<sys/wait.h> //waitpid()
#include <boost/asio/signal_set.hpp>
#include <boost/bind/bind.hpp>
#include <memory>
#include <sstream>
using namespace std;
using boost::asio::ip::tcp;

boost::asio::io_context io_context;

const vector<string> split(const string &str, const char &delimiter) {
    vector<string> result;
    stringstream ss(str);
    string tok;

    while (getline(ss, tok, delimiter)) {
        result.push_back(tok);
    }
    return result;
}

class Connect_session
    : public std::enable_shared_from_this<Connect_session>
{
private:
    tcp::socket socket_c; //this socket is connect to the browser
    tcp::socket socket_h; 
    string DSTIP;
    unsigned int DSTPORT;    
    tcp::resolver resolver;
    unsigned char CD;
    tcp::endpoint endpoint;
    enum { max_length = 1024};
    unsigned char input_h[max_length] = {0};
    unsigned char input_c[max_length] = {0};
    bool reply_accept=true;
public:
    Connect_session(tcp::socket socket, string ip, unsigned int port, unsigned char CD)
        :socket_c(std::move(socket)),socket_h(io_context), DSTIP(ip), DSTPORT(port), resolver(io_context),CD(CD)
    {
    }
    void start()
    {
        do_resolve();
    }

private:
    bool check_valid(){
        ifstream fin;
        fin.open("./socks.conf");
        string line;
        vector<string> dstip_buffer = split(DSTIP, '.');
        while(getline(fin,line)){
            string firewall;
            if (line[7] == 'c'){
                firewall = line.substr(9);
                vector<string> firewall_buffer = split(firewall, '.');
                for(int i=0;i<firewall_buffer.size();i++){
                    if(firewall_buffer[i] == "*")
                        break;
                    else if(dstip_buffer[i] != firewall_buffer[i]){
                        reply_accept = false;
                        return false;
                    }
                }
            }
        }
        return true;
    }
    void do_resolve(){
        auto self(shared_from_this());
        resolver.async_resolve(DSTIP,to_string(DSTPORT),[this, self](const boost::system::error_code ec, tcp::resolver::iterator iter){
                if(!ec) {
                    endpoint = *iter;
                    do_connect();
                }else{
                    socket_c.close();
                    socket_h.close();
                    perror("do_resolve fail");
                }                
            }
        );
    }

    void do_connect(){
        auto self(shared_from_this());
        socket_h.async_connect(endpoint,[this,self](boost::system::error_code ec){
            // cerr<<endpoint.address()<<endl;
            // cout<<endpoint.port()<<endl;
            if(!ec){        
                do_reply();
            }
            else{
                perror("do_connect fail");
            }
        });
    }
    void do_read_host(){
        auto self(shared_from_this());
        socket_h.async_read_some(boost::asio::buffer(input_h, max_length),[this, self](boost::system::error_code ec, std::size_t length)
        {
            if(!ec){                
                do_write_browser(length);
            }
            else{
                socket_h.close();
                socket_c.close();
            }
        }
        );
    }

    void do_read_browser(){
        auto self(shared_from_this());
        socket_c.async_read_some(boost::asio::buffer(input_c, max_length),[this, self](boost::system::error_code ec, std::size_t length)
        {
            if(!ec){                
                do_write_host(length);
            }   
            else{
                socket_c.close();
                socket_h.close();
            }         
        }
        );
    }

    void do_write_host(std::size_t length){
        auto self(shared_from_this());
        boost::asio::async_write(socket_h,boost::asio::buffer(input_c, length),[this, self](boost::system::error_code ec, std::size_t length)
        {
            if(!ec){
                memset(input_c, 0x00 ,sizeof(input_c)); 
                do_read_browser();
            }
        }
        );
    }

    void do_write_browser(std::size_t length){
        auto self(shared_from_this());
        boost::asio::async_write(socket_c,boost::asio::buffer(input_h, length),[this, self](boost::system::error_code ec, std::size_t length)
        {
            if(!ec){
                memset(input_h, 0x00 ,sizeof(input_h)); 
                do_read_host();
            }
        }
        );
    }

    void reply_msg(){
        cout<<"<S_IP>: "<<socket_c.remote_endpoint().address().to_string()<<endl;
        cout<<"<S_PORT>: "<<to_string(socket_c.remote_endpoint().port())<<endl;
        cout<<"<DST_IP>: "<< DSTIP << endl;
        cout<<"<DST_PORT>: "<< DSTPORT << endl;
        cout<<"<Command>: "<<(CD == 1 ? "CONNECT" : "BIND")<<endl;
        cout<<"<Reply>: "<<(reply_accept == true ? "Accept" : "Reject")<<endl;
        cout<<"-------------"<<endl;
    } 
    void do_reply(){
        bool check_value = check_valid();
        unsigned char reply[8]={0};
        if(check_value){
            reply[1] = 90;
        }
        if(!check_value){
            reply[1] = 91;
        }
        reply_msg();
        auto self(shared_from_this());
        boost::asio::async_write(socket_c, boost::asio::buffer(reply, sizeof(reply)),[this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if(!ec){
                if(reply_accept){
                    do_read_browser();
                    do_read_host();  
                }
                else{
                    socket_c.close();
                    socket_h.close();
                }          
            }
        }       
        ); 
    }

};



class Bind_session
    :public std::enable_shared_from_this<Bind_session>
{
private:
    tcp::socket socket_c; //this socket is connect to the browser
    tcp::socket socket_h{io_context}; 
    string DSTIP;
    unsigned int DSTPORT;    
    // tcp::resolver resolver;
    unsigned char CD;
    tcp::acceptor bind_acceptor{io_context};
    
    unsigned short port;
    enum { max_length = 15000};
    unsigned char input_h[max_length] = {0};
    unsigned char input_c[max_length] = {0};
    unsigned char reply[8] = {0};
    bool reply_accept=true;
public:
    Bind_session(tcp::socket socket, string ip, unsigned int port, unsigned char CD)
        :socket_c(std::move(socket)), DSTIP(ip), DSTPORT(port),CD(CD)
    {
    }

    void start()
    {        
        do_reply_first();
    }
private:
    
    bool check_valid(){
        ifstream fin;
        fin.open("./socks.conf");
        string line;
        vector<string> dstip_buffer = split(DSTIP, '.');
        while(getline(fin,line)){
            string firewall;
            if (line[7] == 'b'){
                firewall = line.substr(9);
                vector<string> firewall_buffer = split(firewall, '.');
                for(int i=0;i<firewall_buffer.size();i++){
                    if(firewall_buffer[i] == "*")
                        break;
                    else if(dstip_buffer[i] != firewall_buffer[i]){
                        reply_accept = false;
                        return false;
                    }
                }
            }
        }
        return true;
    }
    void reply_msg(){
        cout<<"<S_IP>: "<<socket_c.remote_endpoint().address().to_string()<<endl;
        cout<<"<S_PORT>: "<<to_string(socket_c.remote_endpoint().port())<<endl;
        cout<<"<DST_IP>: "<< DSTIP << endl;
        cout<<"<DST_PORT>: "<< DSTPORT << endl;
        cout<<"<Command>: "<<(CD == 1 ? "CONNECT" : "BIND")<<endl;
        cout<<"<Reply>: "<<(reply_accept == true ? "Accept" : "Reject")<<endl;
        cout<<"-------------"<<endl;
    } 

    void do_reply_first(){
        bool check_value = check_valid();
        auto self(shared_from_this());
        tcp::endpoint endpoint_(tcp::v4(), 0);
        bind_acceptor.open(endpoint_.protocol());
        bind_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        bind_acceptor.bind(endpoint_);
        bind_acceptor.listen();
        port = bind_acceptor.local_endpoint().port();
        if(check_value){
            reply[1] = 90;
        }
        if(!check_value){
            reply[1] = 91;
        }
        reply[2] = port / 256;
        reply[3] = port % 256;
        reply_msg();

        boost::asio::async_write(socket_c, boost::asio::buffer(reply, sizeof(reply)),[this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if(!ec){
                if(reply_accept)
                    do_accept();
                else{
                    socket_c.close();
                    socket_h.close();
                }
            }
        });         

    }
    void do_accept()
    {     
        auto self(shared_from_this());
        bind_acceptor.async_accept([this, self](boost::system::error_code ec, tcp::socket socket_n)
        {
            socket_h = move(socket_n);
            if(!ec)
            {
                auto self(shared_from_this());
                boost::asio::async_write(socket_c, boost::asio::buffer(reply, sizeof(reply)),[this, self](boost::system::error_code ec, std::size_t /*length*/){ 
                    do_read_browser();
                    do_read_host();
                });
            }
            else{
                cout<<ec.message()<<endl;
            }
        });
    }
    
    void do_read_host(){
        auto self(shared_from_this());
        socket_h.async_read_some(boost::asio::buffer(input_h, max_length),[this, self](boost::system::error_code ec, std::size_t length)
        {
            if(!ec){                
                do_write_browser(length);
            }
        }
        );
    }

    void do_read_browser(){
        auto self(shared_from_this());
        socket_c.async_read_some(boost::asio::buffer(input_c, max_length),[this, self](boost::system::error_code ec, std::size_t length)
        {
            if(!ec){                
                do_write_host(length);
            }
        }
        );
    }

    void do_write_host(std::size_t length){
        auto self(shared_from_this());
        boost::asio::async_write(socket_h,boost::asio::buffer(input_c, length),[this, self](boost::system::error_code ec, std::size_t length)
        {
            if(!ec){
                memset(input_c, 0x00 ,sizeof(input_c)); 
                do_read_browser();
            }
        }
        );
    }

    void do_write_browser(std::size_t length){
        auto self(shared_from_this());
        boost::asio::async_write(socket_c,boost::asio::buffer(input_h, length),[this, self](boost::system::error_code ec, std::size_t length)
        {
            if(!ec){
                memset(input_h, 0x00 ,sizeof(input_h)); 
                do_read_host();
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
        : socket_c(std::move(socket))
    {
    }
    void start()
    {
        do_read_some();
    }
private:
    
    void do_read_some()
    {
        auto self(shared_from_this());
        socket_c.async_read_some(boost::asio::buffer(data_, max_length),[this, self](boost::system::error_code ec, std::size_t length)
        {
            if (!ec)
            {         
                VN = data_[0];
                CD = data_[1];
                DSTPORT = (int(data_[2]) << 8) + int(data_[3]);
                DSTIP = to_string(data_[4]) + "." + to_string(data_[5]) + "." + to_string(data_[6]) + "." + to_string(data_[7]);
                domainName = do_domain_name(length);
                if(domainName != ""){
                    DSTIP = domainName;
                    // cout<<"4a "<<DSTIP<<endl;
                }
                if (CD == 1){                    
                    std::make_shared<Connect_session>(std::move(socket_c),DSTIP,DSTPORT,CD)->start();                
                }    
                else if(CD == 2){     
                    std::make_shared<Bind_session>(std::move(socket_c),DSTIP,DSTPORT,CD)->start();
                }
            }
            else{
                socket_c.close();
            }
        });
    }

    string do_domain_name(size_t length){
        vector<int> null_p;
        for(size_t i=8; i<length; i++) {
            if(data_[i] == 0x00){
                null_p.push_back(i);
            }
        }
        if(null_p.size() == 1) {
            return "";
        }
        else{
            string domain = "";
            int start = null_p[0]+1;
            int end = null_p[1];
            for(int i=start; i<end; i++){
                domain += data_[i];
            }
            return domain;
        }
       
    }

    tcp::socket socket_c; //this socket is connect to the browser
    // tcp::socket socket_h; 
    
    enum { max_length = 4096 };
    unsigned char data_[max_length];
    string Msg;
    unsigned char VN;
    unsigned char CD;
    unsigned int DSTPORT;
    string DSTIP;
    // unsigned char Null;
    string domainName;
};

class Socks4_server{
public:
    Socks4_server(boost::asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    {
        start_signal_wait();
        do_accept();
    }
private:
    void start_signal_wait()
    {
        signal_.async_wait(boost::bind(&Socks4_server::handle_signal_wait, this));
    }

    void handle_signal_wait()
    {
        if (acceptor_.is_open())
        {
            int status = 0;
            while (waitpid(-1, &status, WNOHANG) > 0) {}
            start_signal_wait();
        }
    }

    void do_accept()
    {
        acceptor_.async_accept(socket_,boost::bind(&Socks4_server::handle_accept, this, boost::placeholders::_1));
    }

    void handle_accept(const boost::system::error_code& ec)
    {
        
        if (!ec)
        {   
            io_context.notify_fork(boost::asio::io_context::fork_prepare);
            pid_t pid= fork();
            if(pid < 0){
                cerr<<"fork failed"<<endl;
                int status = 0;
                waitpid(-1, &status, 0);
                // do_accept();
            }
            if(pid == 0){
                io_context.notify_fork(boost::asio::io_context::fork_child);
                acceptor_.close();
                signal_.cancel();
                std::make_shared<server_session>(std::move(socket_))->start();

            }

            else if(pid > 0){
                io_context.notify_fork(boost::asio::io_context::fork_parent);
                socket_.close();
                // do_accept();                    
            }     
            do_accept();        
        } ;
    }
    
    tcp::acceptor acceptor_;    
    tcp::socket socket_{io_context};
    boost::asio::signal_set signal_{io_context};

};

void sigchld_handler(int sig)
{
    int status;
    waitpid(-1, &status, WNOHANG) ;
}

int main(int argc, char* argv[]){

    signal(SIGCHLD, sigchld_handler);

    try{

        Socks4_server s(io_context, std::atoi(argv[1]));
        io_context.run();

    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;

}
