#include "ipc.h"
#include <chrono>
#include <cstring>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

int main() {
    Ranking ranking;
    Ipc ipc(ranking);
    if (!ipc.Start()) return 1;

#ifdef _WIN32
    HANDLE client = CreateFileA("\\\\.\\pipe\\jf-spec-A", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (client == INVALID_HANDLE_VALUE) return 2;
    const char message[] = "JF1 jump_rush 123 456\n";
    DWORD sent = 0;
    if (!WriteFile(client, message, sizeof(message) - 1, &sent, nullptr)) return 3;
#else
    const int client = socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    const auto path = "/tmp/jf-spec-" + std::to_string(getuid()) + "/rankings-A.sock";
    path.copy(address.sun_path, sizeof(address.sun_path) - 1);
    if (connect(client, reinterpret_cast<sockaddr*>(&address), sizeof(address))) return 2;
    const char message[] = "JF1 jump_rush 123 456\n";
    if (write(client, message, sizeof(message) - 1) != sizeof(message) - 1) return 3;
#endif

    for (int wait = 0; wait < 100 && ranking.Read().userids.size() != 2; ++wait) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (ranking.Read().userids.size() != 2) return 4;

    Ranking otherRanking;
    Ipc other(otherRanking);
    if (!other.Start('B') || !otherRanking.Read().map.empty()) return 7;
    other.Stop();

#ifdef _WIN32
    CloseHandle(client);
#else
    close(client);
#endif
    ipc.Stop();
    if (!ranking.Read().map.empty()) return 5;
    if (!ipc.Start()) return 6;
    ipc.Stop();
    std::cout << "IPC publish, cleanup and restart passed\n";
}
