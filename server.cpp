#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>

#define PORT 12345

std::mutex cout_mutex;

enum class ComputationStatus {
    IN_PROGRESS,
    DONE
};

struct ClientData {
    uint32_t n;
    uint32_t threads;
    std::vector<int32_t> A;
    std::vector<int32_t> B;
    std::vector<int32_t> C;
    ComputationStatus status;
};

void computePart(ClientData& data, int startRow, int endRow) {
    int n = data.n;
    for (int i = startRow; i < endRow; ++i) {
        for (int j = 0; j < n; ++j) {
            int32_t sum = 0;
            for (int k = 0; k < n; ++k) {
                sum += data.A[i * n + k] * data.B[k * n + j];
            }
            data.C[i * n + j] = sum;
        }
    }
}

uint32_t toBigEndian(uint32_t val) {
    return htonl(val);
}

uint32_t fromBigEndian(uint32_t val) {
    return ntohl(val);
}

bool recvAll(int sock, char* buffer, int bytes) {
    int totalReceived = 0;
    while (totalReceived < bytes) {
        int received = recv(sock, buffer + totalReceived, bytes - totalReceived, 0);
        if (received <= 0) return false;
        totalReceived += received;
    }
    return true;
}

bool sendAll(int sock, const char* buffer, int bytes) {
    int totalSent = 0;
    while (totalSent < bytes) {
        int sent = send(sock, buffer + totalSent, bytes - totalSent, 0);
        if (sent <= 0) return false;
        totalSent += sent;
    }
    return true;
}

void handleClient(int clientSocket) {
    ClientData data;

    uint32_t n_be;
    if (!recvAll(clientSocket, (char*)&n_be, sizeof(n_be))) {
        close(clientSocket);
        return;
    }
    data.n = fromBigEndian(n_be);

    uint32_t threads_be;
    if (!recvAll(clientSocket, (char*)&threads_be, sizeof(threads_be))) {
        close(clientSocket);
        return;
    }
    data.threads = fromBigEndian(threads_be);

    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "[SERVER] Matrix size received: " << data.n
                  << " | Threads requested: " << data.threads << std::endl;
    }

    int totalElements = data.n * data.n;
    data.A.resize(totalElements);
    data.B.resize(totalElements);
    data.C.resize(totalElements);

    if (!recvAll(clientSocket, (char*)data.A.data(), totalElements * sizeof(int32_t))) {
        close(clientSocket);
        return;
    }
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "[SERVER] Matrix A received." << std::endl;
    }

    if (!recvAll(clientSocket, (char*)data.B.data(), totalElements * sizeof(int32_t))) {
        close(clientSocket);
        return;
    }
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "[SERVER] Matrix B received." << std::endl;
    }

    bool running = true;
    while (running) {
        uint32_t cmdSize_be;
        if (!recvAll(clientSocket, (char*)&cmdSize_be, sizeof(cmdSize_be))) {
            close(clientSocket);
            return;
        }
        uint32_t cmdSize = fromBigEndian(cmdSize_be);

        std::vector<char> cmdBuffer(cmdSize);
        if (!recvAll(clientSocket, cmdBuffer.data(), cmdSize)) {
            close(clientSocket);
            return;
        }
        std::string command(cmdBuffer.begin(), cmdBuffer.end());

        if (command == "START") {
            {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "[SERVER] Command START received. Starting computation..." << std::endl;
            }

            data.status = ComputationStatus::IN_PROGRESS;
            std::vector<std::thread> workers;
            int rowsPerThread = data.n / data.threads;
            int extraRows = data.n % data.threads;
            int currentRow = 0;

            for (uint32_t i = 0; i < data.threads; ++i) {
                int startRow = currentRow;
                int endRow = startRow + rowsPerThread + (i < extraRows ? 1 : 0);
                workers.emplace_back(computePart, std::ref(data), startRow, endRow);
                currentRow = endRow;
            }

            for (auto& th : workers) {
                th.join();
            }

            {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "[SERVER] Computation finished." << std::endl;
            }

            data.status = ComputationStatus::DONE;
        }
        else if (command == "STATUS") {
            std::string statusMsg = (data.status == ComputationStatus::DONE) ? "DONE" : "IN_PROGRESS";
            uint32_t statusLen = toBigEndian(statusMsg.size());
            sendAll(clientSocket, (char*)&statusLen, sizeof(statusLen));
            sendAll(clientSocket, statusMsg.c_str(), statusMsg.size());
            {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "[SERVER] STATUS request processed: " << statusMsg << std::endl;
            }
        }
        else if (command == "GET_RESULT") {
            if (data.status == ComputationStatus::DONE) {
                uint32_t payloadSize = toBigEndian(totalElements * sizeof(int32_t));
                sendAll(clientSocket, (char*)&payloadSize, sizeof(payloadSize));
                sendAll(clientSocket, (char*)data.C.data(), totalElements * sizeof(int32_t));
                {
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cout << "[SERVER] Result matrix sent to client." << std::endl;
                }
                running = false;
            }
        }
        else {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "[SERVER] Unknown command: " << command << std::endl;
            close(clientSocket);
            return;
        }
    }

    close(clientSocket);
}

int main() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, 5);

    std::cout << "[SERVER] Server started. Listening on port " << PORT << "..." << std::endl;

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientSize = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientSize);
        std::thread(handleClient, clientSocket).detach();
    }

    close(serverSocket);
    return 0;
}
