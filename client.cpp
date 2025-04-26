#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1"
#define PORT 12345

uint32_t toBigEndian(uint32_t value) {
    return htonl(value);
}

uint32_t fromBigEndian(uint32_t value) {
    return ntohl(value);
}

bool recvAll(int sock, char* buffer, int totalBytes) {
    int received = 0;
    while (received < totalBytes) {
        int ret = recv(sock, buffer + received, totalBytes - received, 0);
        if (ret <= 0) return false;
        received += ret;
    }
    return true;
}

bool sendAll(int sock, const char* buffer, int totalBytes) {
    int sent = 0;
    while (sent < totalBytes) {
        int ret = send(sock, buffer + sent, totalBytes - sent, 0);
        if (ret <= 0) return false;
        sent += ret;
    }
    return true;
}

void sendCommand(int sock, const std::string& command) {
    uint32_t cmdLen = toBigEndian(command.size());
    sendAll(sock, (char*)&cmdLen, sizeof(cmdLen));
    sendAll(sock, command.c_str(), command.size());
}

std::string receiveResponse(int sock) {
    uint32_t size_be;
    if (!recvAll(sock, (char*)&size_be, sizeof(size_be))) return "";
    uint32_t size = fromBigEndian(size_be);

    std::vector<char> buffer(size);
    if (!recvAll(sock, buffer.data(), size)) return "";

    return std::string(buffer.begin(), buffer.end());
}

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);
    serverAddr.sin_port = htons(PORT);

    connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr));

    srand(time(nullptr));
    uint32_t n = rand() % 10 + 1;
    uint32_t threads = rand() % 4 + 1;

    std::cout << "[CLIENT] Matrix size: " << n << "x" << n << ", Threads: " << threads << "\n";

    int totalElements = n * n;
    std::vector<int32_t> A(totalElements);
    std::vector<int32_t> B(totalElements);

    for (int i = 0; i < totalElements; ++i) {
        A[i] = rand() % 10 + 1;
        B[i] = rand() % 10 + 1;
    }

    uint32_t n_be = toBigEndian(n);
    sendAll(sock, (char*)&n_be, sizeof(n_be));

    uint32_t threads_be = toBigEndian(threads);
    sendAll(sock, (char*)&threads_be, sizeof(threads_be));

    sendAll(sock, (char*)A.data(), totalElements * sizeof(int32_t));
    sendAll(sock, (char*)B.data(), totalElements * sizeof(int32_t));

    sendCommand(sock, "START");

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        sendCommand(sock, "STATUS");

        std::string response = receiveResponse(sock);
        if (response == "DONE") {
            std::cout << "[CLIENT] Computation done!\n";
            break;
        } else {
            std::cout << "[CLIENT] Computation in progress...\n";
        }
    }

    sendCommand(sock, "GET_RESULT");

    uint32_t resultSize_be;
    recvAll(sock, (char*)&resultSize_be, sizeof(resultSize_be));
    uint32_t resultSize = fromBigEndian(resultSize_be);

    std::vector<int32_t> C(totalElements);
    recvAll(sock, (char*)C.data(), resultSize);

    std::cout << "[CLIENT] Result matrix:\n";
    for (uint32_t i = 0; i < n; ++i) {
        for (uint32_t j = 0; j < n; ++j) {
            std::cout << C[i * n + j] << "\t";
        }
        std::cout << "\n";
    }

    close(sock);
    return 0;
}
