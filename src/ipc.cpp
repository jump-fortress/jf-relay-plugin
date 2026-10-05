#include "ipc.h"
#include <chrono>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <sddl.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#endif

bool Ipc::Start() {
    if (!stopping_.exchange(false)) return false;
    startup_ = 0;
    try {
        worker_ = std::thread(&Ipc::Run, this);
    } catch (...) {
        stopping_ = true;
        return false;
    }
    for (int wait = 0; wait < 200 && startup_ == 0; ++wait) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (startup_ == 1) return true;

    Stop();
    return false;
}

void Ipc::Stop() {
    stopping_ = true;
    if (worker_.joinable()) worker_.join();
    ranking_.Clear();
}

void Ipc::Run() {
    struct Finished {
        std::atomic<int>& state;
        ~Finished() { state = -1; }
    } finished{startup_};

    std::string pending;
    const auto consume = [&](const char* bytes, size_t length) {
        pending.append(bytes, length);
        if (pending.size() > 8192) return false;

        size_t newline;
        while ((newline = pending.find('\n')) != std::string::npos) {
            const auto line = pending.substr(0, newline);
            pending.erase(0, newline + 1);
            if (!ranking_.Update(line)) return false;
        }
        return true;
    };

#ifdef _WIN32
    // Only this Windows user can publish rankings to the local pipe.
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return;
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<char> buffer(size);
    if (!GetTokenInformation(token, TokenUser, buffer.data(), size, &size)) { CloseHandle(token); return; }
    LPSTR sid = nullptr;
    if (!ConvertSidToStringSidA(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid, &sid)) { CloseHandle(token); return; }
    const std::string acl = "D:P(A;;GA;;;" + std::string(sid) + ")";
    LocalFree(sid);
    CloseHandle(token);
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(acl.c_str(), SDDL_REVISION_1, &descriptor, nullptr)) return;
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
    // Node's net.Socket uses duplex access even though rankings flow one way.
    HANDLE pipe = CreateNamedPipeA("\\\\.\\pipe\\jf-spec", PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1, 8192, 8192, 0, &security);
    LocalFree(descriptor);
    if (pipe == INVALID_HANDLE_VALUE) return;
    startup_ = 1;

    bool connected = false;
    while (!stopping_) {
        if (!connected) {
            connected = ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED;
        } else {
            char bytes[2048];
            DWORD count = 0;
            if (ReadFile(pipe, bytes, sizeof(bytes), &count, nullptr)) {
                if (count && !consume(bytes, count)) {
                    DisconnectNamedPipe(pipe);
                    connected = false;
                }
            } else if (GetLastError() != ERROR_NO_DATA) {
                DisconnectNamedPipe(pipe);
                connected = false;
            }
            if (!connected) { pending.clear(); ranking_.Clear(); }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
#else
    const std::string directory = "/tmp/jf-spec-" + std::to_string(getuid());
    if (mkdir(directory.c_str(), 0700) != 0 && errno != EEXIST) return;
    struct stat info{};
    if (lstat(directory.c_str(), &info) || !S_ISDIR(info.st_mode)
        || info.st_uid != getuid() || (info.st_mode & 0777) != 0700) return;

    const std::string path = directory + "/rankings.sock";
    const int listener = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listener < 0) return;
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    path.copy(address.sun_path, sizeof(address.sun_path) - 1);
    // Do not unlink another running plugin's socket. A stale socket requires manual removal.
    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) || listen(listener, 1)) {
        close(listener);
        return;
    }
    chmod(path.c_str(), 0600);
    startup_ = 1;
    int client = -1;
    while (!stopping_) {
        if (client < 0) {
            client = accept4(listener, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
        } else {
            char bytes[2048];
            const auto count = read(client, bytes, sizeof(bytes));
            if ((count > 0 && !consume(bytes, static_cast<size_t>(count))) || count == 0
                || (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
                close(client);
                client = -1;
                pending.clear();
                ranking_.Clear();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (client >= 0) close(client);
    close(listener);
    unlink(path.c_str());
#endif
}
