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
    send_string(fd, pkt.nickname);
    send_string(fd, pkt.message);
}

void send_commonpacket(int fd, const CommonPacket& pkt) {
    send(fd, &pkt.type, sizeof(pkt.type), 0);
    send_string(fd, pkt.nickname);
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

void broadcast_common(const CommonPacket& pkt) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (const auto& client : clients)
        send_commonpacket(client.fd, pkt);
}

void broadcast_playerCnt(const PlayerCntPacket& pkt) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (const auto& client : clients)
        send(client.fd, &pkt, sizeof(pkt), 0);
}

void handle_client(int client_fd, int player_num, bool is_first_client) {
    std::string nickname = "player" + std::to_string(player_num);

    if (is_first_client) {
        // 최초 클라이언트로부터 max_Player 정보 수신
        int msgType = 0;
        ssize_t n = recv(client_fd, &msgType, sizeof(int), MSG_WAITALL);
        if (n <= 0 || msgType != MSG_SET_MAX_PLAYER) {
            std::cerr << "Failed to receive maxPlayer info from first client!\n";
            close(client_fd);
            return;
        }
        int newMaxPlayer = 2;
        n = recv(client_fd, &newMaxPlayer, sizeof(int), MSG_WAITALL);
        if (n != sizeof(int)) {
            std::cerr << "Failed to receive maxPlayer value!\n";
            close(client_fd);
            return;
        }
        max_Player = newMaxPlayer;
        std::cout << "[Server] max_Player set to " << max_Player << " by first client\n";
    }


    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        clients.push_back({client_fd, nickname});
    }

    PlayerNumPacket player_pkt{};
    PlayerCntPacket capacity_pkt{};

    player_pkt.type = MSG_PLAYER_NUM;
    player_pkt.player_num = player_num;

    capacity_pkt.type = MSG_PLAYER_CNT;
    capacity_pkt.currentPlayer_cnt = current_Player;
    capacity_pkt.maxPlayer = max_Player;

    std::cout << "Client connected (" << nickname << ")\n";
    std::cout <<capacity_pkt.currentPlayer_cnt << ")\n";
    broadcast_playerCnt(capacity_pkt);
    send(client_fd, &player_pkt, sizeof(player_pkt), 0);


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
            std::cout << "[Received answer] " << nickname << ": " << pkt.answer << std::endl;
            if (pkt.answer == current_answer) {
                CommonPacket correct_pkt{};
                correct_pkt.type = MSG_CORRECT;
                correct_pkt.nickname = nickname;
                correct_pkt.message = pkt.answer;
                broadcast_common(correct_pkt);
                correct = true;
            } else {
                CommonPacket wrong_pkt{};
                wrong_pkt.type = MSG_WRONG;
                wrong_pkt.nickname = nickname;
                wrong_pkt.message = pkt.answer;
                broadcast_common(wrong_pkt);
            }
        } else if (msg_type == MSG_DISCONNECT) { // ★ 추가
            int dummy;
            recv(client_fd, &dummy, sizeof(int), 0);
            std::cout << "[Server] Player(" << nickname << ") disconnect\n";
            close(client_fd);
            current_Player--;
	    break;

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

    if (clients.empty()) {
        max_Player = 2; // 모두 나갔을 경우 기본값으로 초기화 
        std::cout << "[Server] All clients disconnected. max_Player reset to 2.\n";

    }
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
    current_Player = 0;
    bool is_first_client = true;
    std::mutex is_first_client_mutex; 

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) { perror("accept"); continue; }

        // 현재 is_first_client 값 읽어서 전달
        bool this_is_first_client;
        {
            std::lock_guard<std::mutex> lock(is_first_client_mutex);
            this_is_first_client = is_first_client;
            if (is_first_client) is_first_client = false;
        }

        // 스레드 생성 시 람다로 감싸서, 클라이언트 종료 후 체크
        std::thread([&, client_fd, player_num = player_counter++, this_is_first_client]() {
            handle_client(client_fd, player_num, this_is_first_client);

            // 클라이언트가 종료된 후 체크
            std::lock_guard<std::mutex> lock(clients_mutex);
            if (clients.empty()) {
                max_Player = 2; // 초기값으로 리셋
                std::lock_guard<std::mutex> lock2(is_first_client_mutex);
                is_first_client = true;
                std::cout << "[Server] All clients disconnected. max_Player and is_first_client reset.\n";
            }
        }).detach();

        if(current_Player < max_Player) {
            current_Player++;
        } else {
            std::cout  << "Out of capacity" << std::endl;
        }
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
