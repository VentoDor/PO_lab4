#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>

#define SERVER_IP "127.0.0.1"
#define PORT 12345

using namespace std::chrono;

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

void sendCommand(int sock, const std::string& command) {
    uint32_t len = toBigEndian(command.size());
    sendAll(sock, (char*)&len, sizeof(len));
    sendAll(sock, command.c_str(), command.size());
    std::cout << "[CLIENT] Sent command: " << command << std::endl;
}

std::string receiveResponse(int sock) {
    uint32_t size_be;
    if (!recvAll(sock, (char*)&size_be, sizeof(size_be))) return "";
    uint32_t size = fromBigEndian(size_be);
    std::vector<char> buf(size);
    if (!recvAll(sock, buf.data(), size)) return "";
    return std::string(buf.begin(), buf.end());
}

int main() {
    srand(time(nullptr));

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);
    serverAddr.sin_port = htons(PORT);

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "[CLIENT] Connection failed.\n";
        return 1;
    }
    std::cout << "[CLIENT] Connected to server.\n";

    uint32_t n = rand() % 10 + 1;
    uint32_t threads = rand() % 4 + 1;
    int totalElements = n * n;

    std::cout << "[CLIENT] Matrix size: " << n << "x" << n << " | Threads: " << threads << std::endl;

    std::vector<int32_t> A(totalElements);
    std::vector<int32_t> B(totalElements);

    for (int i = 0; i < totalElements; ++i) {
        A[i] = rand() % 10 + 1;
        B[i] = rand() % 10 + 1;
    }

    uint32_t n_be = toBigEndian(n);
    sendAll(sock, (char*)&n_be, sizeof(n_be));
    std::cout << "[CLIENT] Sent matrix size.\n";

    uint32_t threads_be = toBigEndian(threads);
    sendAll(sock, (char*)&threads_be, sizeof(threads_be));
    std::cout << "[CLIENT] Sent number of threads.\n";

    sendAll(sock, (char*)A.data(), totalElements * sizeof(int32_t));
    std::cout << "[CLIENT] Sent matrix A.\n";

    sendAll(sock, (char*)B.data(), totalElements * sizeof(int32_t));
    std::cout << "[CLIENT] Sent matrix B.\n";

    sendCommand(sock, "START");

    auto startTime = high_resolution_clock::now();

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        sendCommand(sock, "STATUS");

        std::string response = receiveResponse(sock);
        std::cout << "[CLIENT] STATUS response: " << response << std::endl;
        if (response == "DONE") {
            std::cout << "[CLIENT] Computation completed!\n";
            break;
        }
    }

    sendCommand(sock, "GET_RESULT");

    uint32_t resultSize_be;
    recvAll(sock, (char*)&resultSize_be, sizeof(resultSize_be));
    uint32_t resultSize = fromBigEndian(resultSize_be);

    std::vector<int32_t> C(totalElements);
    recvAll(sock, (char*)C.data(), resultSize);

    auto endTime = high_resolution_clock::now();
    double elapsedSeconds = duration<double>(endTime - startTime).count();

    std::cout << "[CLIENT] Result matrix received in " << elapsedSeconds << " seconds.\n";
    for (uint32_t i = 0; i < n; ++i) {
      for (uint32_t j = 0; j < n; ++j) {
          std::cout << C[i * n + j] << "\t";
      }
      std::cout << "\n";
  }

  close(sock);
  std::cout << "[CLIENT] Disconnected.\n";
  return 0;
}
