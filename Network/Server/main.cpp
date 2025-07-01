#include "server.h"
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <algorithm>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

void send_string(int fd, const std::string& s) {
    uint32_t len = s.size();
    send(fd, &len, sizeof(len), 0);
    if (len > 0) send(fd, s.data(), len, 0);
}

std::string recv_string(int fd) {
    uint32_t len = 0;
    recv(fd, &len, sizeof(len), MSG_WAITALL);
    std::string s;
    if (len > 0) {
        s.resize(len);
        recv(fd, &s[0], len, MSG_WAITALL);
    }
    return s;
}

struct ClientInfo {
    int fd;
    std::string nickname;
};

std::mutex clients_mutex;
std::vector<ClientInfo> clients;
std::string current_answer;

// 구조체 패킷 전송/수신
void send_drawpacket(int fd, const DrawPacket& pkt) {
    send(fd, &pkt, sizeof(pkt), 0);
}
bool recv_drawpacket(int fd, DrawPacket& pkt) {
    return recv(fd, &pkt, sizeof(pkt), MSG_WAITALL) == sizeof(pkt);
}
void send_answerpacket(int fd, const AnswerPacket& pkt) {
    send(fd, &pkt.type, sizeof(pkt.type), 0);
    send_string(fd, pkt.nickname);
    send_string(fd, pkt.answer);
}
bool recv_answerpacket(int fd, AnswerPacket& pkt) {
    int header;
    if (recv(fd, &header, sizeof(header), MSG_WAITALL) != sizeof(header)) return false;
    pkt.type = header;
    pkt.nickname = recv_string(fd);
    pkt.answer = recv_string(fd);
    return true;
}
void send_correctpacket(int fd, const CorrectPacket& pkt) {
    send(fd, &pkt.type, sizeof(pkt.type), 0);
    send_string(fd, pkt.nickname);
}
void send_wrongpacket(int fd, const WrongPacket& pkt) {
    send(fd, &pkt.type, sizeof(pkt.type), 0);
    send_string(fd, pkt.message);
}

void broadcast_draw(const DrawPacket& pkt, int except_fd = -1) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (const auto& client : clients) {
        if (client.fd == except_fd) continue;
        send_drawpacket(client.fd, pkt);
    }
}
void broadcast_correct(const CorrectPacket& pkt) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (const auto& client : clients)
        send_correctpacket(client.fd, pkt);
}

void handle_client(int client_fd, int player_num) {
    std::string nickname = "player" + std::to_string(player_num);

    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.push_back({client_fd, nickname});
    }
    std::cout << "Client connected (" << nickname << ")\n";

    bool correct = false;
    while (true) {
        int msg_type = 0;
        ssize_t n = recv(client_fd, &msg_type, sizeof(int), MSG_PEEK);
        if (n <= 0) break;

        if (msg_type == MSG_DRAW) {
            DrawPacket pkt;
            if (!recv_drawpacket(client_fd, pkt)) break;
            broadcast_draw(pkt, client_fd);
        } else if (msg_type == MSG_ANSWER) {
            AnswerPacket pkt;
            if (!recv_answerpacket(client_fd, pkt)) break;
            std::cout << "[정답시도] " << nickname << ": " << pkt.answer << std::endl;
            if (pkt.answer == current_answer) {
                CorrectPacket correct_pkt{};
                correct_pkt.type = MSG_CORRECT;
                correct_pkt.nickname = nickname;
                broadcast_correct(correct_pkt);
                correct = true;
            } else {
                WrongPacket wrong_pkt{};
                wrong_pkt.type = MSG_WRONG;
                wrong_pkt.message = "오답입니다!";
                send_wrongpacket(client_fd, wrong_pkt);
            }
        } else {
            // unknown
            char buf[256];
            recv(client_fd, buf, sizeof(buf), 0);
        }
        if (correct) break;
    }
    close(client_fd);
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.erase(
            std::remove_if(clients.begin(), clients.end(),
                [client_fd](const ClientInfo& c) { return c.fd == client_fd; }),
            clients.end()
        );
    }
    std::cout << "Client disconnected (" << nickname << ")\n";
}

void run_server(unsigned short port, const std::string& answer_word) {
    current_answer = answer_word;
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(25000);
    if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind"); exit(1);
    }
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen"); exit(1);
    }
    std::cout << "[서버] 192.168.10.3:25000에서 대기중... (정답:" << current_answer << ")\n";
    int player_counter = 1;
    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) { perror("accept"); continue; }
        std::thread(handle_client, client_fd, player_counter++).detach();
    }
    close(server_fd);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <answer_word>\n";
        return 1;
    }
    run_server(25000, argv[1]);
    return 0;
}