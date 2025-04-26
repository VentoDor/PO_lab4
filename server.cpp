#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <cstring>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 12345
#define BUFFER_SIZE 4096

std::mutex cout_mutex;

enum class ComputationStatus {
    NOT_STARTED,
    IN_PROGRESS,
    DONE
};

struct ClientData {
    std::vector<int32_t> A, B, C;
    uint32_t n;
    uint32_t threads;
    ComputationStatus status = ComputationStatus::NOT_STARTED;
};

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

void computePart(ClientData& data, int startRow, int endRow) {
    uint32_t n = data.n;
    for (int i = startRow; i < endRow; ++i) {
        for (uint32_t j = 0; j < n; ++j) {
            data.C[i * n + j] = data.A[i * n + j] + data.B[i * n + j];
        }
    }
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
        std::cout << "[SERVER] Matrix size: " << data.n << " Threads: " << data.threads << "\n";
    }

    int totalElements = data.n * data.n;
    data.A.resize(totalElements);
    data.B.resize(totalElements);
    data.C.resize(totalElements);

    if (!recvAll(clientSocket, (char*)data.A.data(), totalElements * sizeof(int32_t))) {
        close(clientSocket);
        return;
    }
    if (!recvAll(clientSocket, (char*)data.B.data(), totalElements * sizeof(int32_t))) {
        close(clientSocket);
        return;
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
                std::cout << "[SERVER] Command START received.\n";
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

            data.status = ComputationStatus::DONE;
        }
        else if (command == "STATUS") {
            std::string statusMsg = (data.status == ComputationStatus::DONE) ? "DONE" : "IN_PROGRESS";
            uint32_t statusLen = toBigEndian(statusMsg.size());
            sendAll(clientSocket, (char*)&statusLen, sizeof(statusLen));
            sendAll(clientSocket, statusMsg.c_str(), statusMsg.size());
        }
        else if (command == "GET_RESULT") {
            if (data.status == ComputationStatus::DONE) {
                uint32_t payloadSize = toBigEndian(totalElements * sizeof(int32_t));
                sendAll(clientSocket, (char*)&payloadSize, sizeof(payloadSize));
                sendAll(clientSocket, (char*)data.C.data(), totalElements * sizeof(int32_t));
                running = false;
            }
        }
        else {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "[SERVER] Unknown command: " << command << "\n";
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
    listen(serverSocket, SOMAXCONN);

    std::cout << "[SERVER] Listening on port " << PORT << "...\n";

    while (true) {
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket != -1) {
            std::thread clientThread(handleClient, clientSocket);
            clientThread.detach();
        }
    }

    close(serverSocket);
    return 0;
}
