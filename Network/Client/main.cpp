#include "client.h"
#include "../../gpio/user/gpio_control.h"
#include <iostream>
#include <thread>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <atomic>
#include <chrono>

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
void send_correctpacket(int fd, const CorrectPacket& pkt) {
    send(fd, &pkt.type, sizeof(pkt.type), 0);
    send_string(fd, pkt.nickname);
}
bool recv_correctpacket(int fd, CorrectPacket& pkt) {
    int header;
    if (recv(fd, &header, sizeof(header), MSG_WAITALL) != sizeof(header)) return false;
    pkt.type = header;
    pkt.nickname = recv_string(fd);
    return true;
}
void send_wrongpacket(int fd, const WrongPacket& pkt) {
    send(fd, &pkt.type, sizeof(pkt.type), 0);
    send_string(fd, pkt.message);
}
bool recv_wrongpacket(int fd, WrongPacket& pkt) {
    int header;
    if (recv(fd, &header, sizeof(header), MSG_WAITALL) != sizeof(header)) return false;
    pkt.type = header;
    pkt.message = recv_string(fd);
    return true;
}

std::atomic<bool> stop_draw{false};

void recv_thread(int sockfd) {
    while (true) {
        int msg_type = 0;
        ssize_t n = recv(sockfd, &msg_type, sizeof(int), MSG_PEEK);
        if (n <= 0) break;

        if (msg_type == MSG_DRAW) {
            DrawPacket pkt;
            if (!recv_drawpacket(sockfd, pkt)) break;
            std::cout << "[DRAW] (" << pkt.x << ", " << pkt.y << ") color:" << pkt.color << " thick:" << pkt.thick << '\n';
        } else if (msg_type == MSG_CORRECT) {
            CorrectPacket pkt;
            if (!recv_correctpacket(sockfd, pkt)) break;
            std::cout << "[정답!] " << pkt.nickname << "님이 정답을 맞혔습니다!\n";
            gpio_led_correct();
            stop_draw = true;
        } else if (msg_type == MSG_WRONG) {
            WrongPacket pkt;
            if (!recv_wrongpacket(sockfd, pkt)) break;
            std::cout << "[오답] " << pkt.message << std::endl;
            gpio_led_wrong();
        } else {
            char buf[256];
            recv(sockfd, buf, sizeof(buf), 0);
        }
    }
    std::cout << "서버 연결 종료\n";
    stop_draw = true;
}

void run_draw_loop(int sockfd) {
    int x = 0, y = 0;
    while (!stop_draw) {
        DrawPacket pkt{};
        pkt.type = MSG_DRAW;
        pkt.x = x; pkt.y = y; pkt.color = (x+y)%10; pkt.thick = 1+(x%5);
        send_drawpacket(sockfd, pkt);
        std::cout << "[좌표전송] (" << pkt.x << ", " << pkt.y << ")\n";
        x += 10; y += 7;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "[draw] 정지됨\n";
}

void run_client(const std::string& mode, const std::string& arg) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); exit(1); }
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    serv_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    if (connect(sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect"); exit(1);
    }

    std::thread(recv_thread, sockfd).detach();

    if (mode == "draw") {
        run_draw_loop(sockfd);
    } else if (mode == "answer") {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        AnswerPacket apkt{};
        apkt.type = MSG_ANSWER;
        apkt.nickname = ""; // 서버에서 부여
        apkt.answer = arg;
        send_answerpacket(sockfd, apkt);
        std::cout << "[정답전송] : " << arg << std::endl;
        while (!stop_draw) std::this_thread::sleep_for(std::chrono::milliseconds(200));
    } else {
        std::cout << "Unknown mode: " << mode << std::endl;
    }

    close(sockfd);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "usage: " << argv[0] << " <mode:draw|answer> <answer_word>\n";
        std::cerr << "예시: ./client_app draw _\n";
        std::cerr << "예시: ./client_app answer 사과\n";
        return 1;
    }
    run_client(argv[1], argv[2]);
    return 0;
}