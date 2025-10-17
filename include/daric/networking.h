#pragma once
#include <atomic>
#include <functional>
#include <iostream>
#include <mutex>
#include <netinet/in.h>  // for sockaddr_in
#include <queue>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace daric
{

class TCPServer
{
  public:
    TCPServer(int port)
      : port_(port)
      , running_(false)
    {}

    ~TCPServer()
    {
        stop();
    }

    void start(std::function<void(int)> clientHandler)
    {
        running_ = true;
        serverThread_ = std::thread([this, clientHandler] {
            int server_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (server_fd == 0)
            {
                perror("socket failed");
                return;
            }

            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = INADDR_ANY;
            address.sin_port = htons(port_);

            if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0)
            {
                perror("bind failed");
                return;
            }

            if (listen(server_fd, 5) < 0)
            {
                perror("listen failed");
                return;
            }

            while (running_)
            {
                int addrlen = sizeof(address);
                int client_fd = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                if (client_fd >= 0)
                {
                    std::thread(clientHandler, client_fd).detach();
                }
            }
            close(server_fd);
        });
    }

    void stop()
    {
        running_ = false;
        if (serverThread_.joinable())
            serverThread_.join();
    }

  private:
    int port_;
    std::atomic<bool> running_;
    std::thread serverThread_;
};

class TCPClient
{
  public:
    TCPClient(const std::string& host, int port)
      : host_(host)
      , port_(port)
      , sock_(-1)
    {}

    bool connectToServer()
    {
        sock_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_ < 0)
        {
            perror("socket failed");
            return false;
        }

        sockaddr_in serv_addr{};
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port_);
        serv_addr.sin_addr.s_addr = INADDR_LOOPBACK;  // localhost

        if (::connect(sock_, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) <
            0)
        {
            perror("connect failed");
            return false;
        }
        return true;
    }

    void sendMessage(const std::string& msg)
    {
        send(sock_, msg.c_str(), msg.size(), 0);
    }

    std::string receiveMessage(size_t maxLen = 1024)
    {
        char buffer[maxLen];
        ssize_t valread = recv(sock_, buffer, maxLen, 0);
        return std::string(buffer, valread);
    }

    void closeConnection()
    {
        if (sock_ >= 0)
            close(sock_);
        sock_ = -1;
    }

  private:
    std::string host_;
    int port_;
    int sock_;
};

}  // namespace daric
