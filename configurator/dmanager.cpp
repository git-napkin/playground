#include "dmanager.h"
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>

std::string daemonStatusString(DaemonStatus s) {
    switch (s) {
        case DaemonStatus::NotInstalled: return "Not Installed";
        case DaemonStatus::InstalledRunning: return "Running";
        case DaemonStatus::InstalledStopped: return "Stopped";
    }
    return "Unknown";
}

std::string DaemonManager::plistPath() {
    return "/Library/LaunchDaemons/com.pluginplayground.grant.plist";
}

static bool runCommand(const char *cmd, const char *const argv[]) {
    pid_t pid;
    int r = posix_spawn(&pid, cmd, nullptr, nullptr,
                        (char *const *)argv, nullptr);
    if (r != 0) return false;
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool runPrivilegedScript(const char *script) {
    pid_t pid;
    const char *args[] = {"/usr/bin/osascript", "-e", script, nullptr};
    int r = posix_spawn(&pid, "/usr/bin/osascript", nullptr, nullptr,
                        (char *const *)args, nullptr);
    if (r != 0) return false;
    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

DaemonStatus DaemonManager::status() {
    std::string path = plistPath();
    if (access(path.c_str(), F_OK) != 0)
        return DaemonStatus::NotInstalled;

    const char *args[] = {
        "/bin/launchctl", "print", "system/com.pluginplayground.grant", nullptr};
    if (runCommand("/bin/launchctl", args))
        return DaemonStatus::InstalledRunning;
    return DaemonStatus::InstalledStopped;
}

bool DaemonManager::install() {
    const char *plist =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n"
        "<dict>\n"
        "    <key>Label</key>\n"
        "    <string>com.pluginplayground.grant</string>\n"
        "    <key>ProgramArguments</key>\n"
        "    <array>\n"
        "        <string>/opt/pluginplayground/bin/grant</string>\n"
        "    </array>\n"
        "    <key>RunAtLoad</key>\n"
        "    <true/>\n"
        "    <key>KeepAlive</key>\n"
        "    <true/>\n"
        "    <key>StandardOutPath</key>\n"
        "    <string>/var/log/pluginplayground/grant.log</string>\n"
        "    <key>StandardErrorPath</key>\n"
        "    <string>/var/log/pluginplayground/grant.err</string>\n"
        "</dict>\n"
        "</plist>\n";

    FILE *f = fopen("/tmp/com.pluginplayground.grant.plist", "w");
    if (!f) return false;
    fputs(plist, f);
    fclose(f);

    std::string script =
        "do shell script \""
        "mkdir -p /var/log/pluginplayground && "
        "cp /tmp/com.pluginplayground.grant.plist "
        "/Library/LaunchDaemons/com.pluginplayground.grant.plist && "
        "chown root:wheel /Library/LaunchDaemons/com.pluginplayground.grant.plist && "
        "chmod 644 /Library/LaunchDaemons/com.pluginplayground.grant.plist && "
        "launchctl load /Library/LaunchDaemons/com.pluginplayground.grant.plist"
        "\" with administrator privileges";

    bool ok = runPrivilegedScript(script.c_str());
    remove("/tmp/com.pluginplayground.grant.plist");
    return ok;
}

bool DaemonManager::uninstall() {
    std::string script =
        "do shell script \""
        "launchctl unload /Library/LaunchDaemons/com.pluginplayground.grant.plist 2>/dev/null; "
        "rm -f /Library/LaunchDaemons/com.pluginplayground.grant.plist"
        "\" with administrator privileges";

    return runPrivilegedScript(script.c_str());
}
