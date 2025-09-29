#ifdef BUILD_NETWORKING

#include "daric/daric.h"
#include <chrono>
#include <thread>

int main() {
    auto& logger = daric::Logger::instance();

    daric::TCPServer server(8080);
    server.start([&logger](int client_fd){
        logger.log(daric::LogLevel::Info, "Client connected!");
        char buffer[1024] = {0};
        read(client_fd, buffer, 1024);
        logger.log(daric::LogLevel::Info, std::string("Received: ") + buffer);
        std::string response = "Hello from server";
        send(client_fd, response.c_str(), response.size(), 0);
        close(client_fd);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    daric::TCPClient client("127.0.0.1", 8080);
    if(client.connectToServer()) {
        client.sendMessage("Hello Daric Server!");
        std::string reply = client.receiveMessage();
        logger.log(daric::LogLevel::Info, "Server replied: " + reply);
        client.closeConnection();
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    server.stop();

    return 0;
}

#endif
