/**
 *  This file is part of SoftGREd
 *
 *    SoftGREd is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesse General Public License as published by the Free Software Foundation, either
 *  version 3 of the License, or (at your option) any later version.
 *
 *  SoftGREd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU Lesse General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesse General Public License
 *  along with SoftGREd.
 *  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Copyright (C) 2014, Jorge Pereira <jpereiran@gmail.com>
 */

#include "softgred.h"
#include "softgred_config.h"
#include "payload.h"
#include "provision.h"
#include "iface_bridge.h"
#include "iface_ebtables.h"
#include "service.h"
#include "log.h"
static int local_address(void)
{
    struct softgred_config *cfg = softgred_config_ref();
    struct ifaddrs *addresses;
    if (getifaddrs(&addresses)) return -1;
    bool found = false;
    for (struct ifaddrs *p = addresses; p; p = p->ifa_next)
        if (p->ifa_addr && p->ifa_addr->sa_family == AF_INET &&
            !strcmp(p->ifa_name, cfg->ifname)) {
            cfg->local = ((struct sockaddr_in *)p->ifa_addr)->sin_addr;
            found = true; break;
        }
    freeifaddrs(addresses);
    if (!found) errno = EADDRNOTAVAIL;
    return found ? 0 : -1;
}
int main(int argc, char **argv)
{
    struct softgred_config *cfg = softgred_config_ref();
    int cli = softgred_config_load_cli(argc, argv);
    if (cli <= 0) { softgred_config_release(); return cli ? EXIT_FAILURE : EXIT_SUCCESS; }
    if (!softgred_config_load_conf(cfg->config_file ? cfg->config_file :
                                   SOFTGRED_DEFAULT_CONFFILE)) {
        softgred_config_release(); return EXIT_FAILURE;
    }
    int result = EXIT_FAILURE, signal_fd = -1, notify_fd = -1;
    bool provision_ready = false, firewall_ready = false;
    if (geteuid() != 0) {
        D_CRIT("Run as root (packet capture and network administration)\n"); goto end;
    }
    umask(0077);
    if (log_open(cfg->log_file)) {
        D_CRIT("Log file %s: %s\n", cfg->log_file, strerror(errno)); goto end;
    }
    D_INFO("Starting %s capture=%s log=%s\n", PACKAGE_STRING, cfg->ifname, cfg->log_file);
    if (local_address()) { D_CRIT("Capture interface: %s\n", strerror(errno)); goto end; }
    signal(SIGPIPE, SIG_IGN);
    if (!cfg->is_foreground) {
        int pipefd[2];
        if (pipe2(pipefd, O_CLOEXEC)) goto end;
        pid_t child = fork();
        if (child < 0) { close(pipefd[0]); close(pipefd[1]); goto end; }
        if (child > 0) {
            close(pipefd[1]);
            unsigned char status = EXIT_FAILURE;
            ssize_t got;
            do { got = read(pipefd[0], &status, 1); } while (got < 0 && errno == EINTR);
            close(pipefd[0]); log_close(); softgred_config_release();
            return got == 1 ? status : EXIT_FAILURE;
        }
        close(pipefd[0]); notify_fd = pipefd[1];
        if (setsid() < 0) goto end;
        int nullfd = open("/dev/null", O_RDONLY);
        if (nullfd < 0 || dup2(nullfd, STDIN_FILENO) < 0) goto end;
        if (nullfd != STDIN_FILENO) close(nullfd);
    }
    /* Resolve relative configuration/PID paths before changing directory. */
    if (softgred_config_create_pid_file(getpid())) {
        D_CRIT("PID file: %s\n", strerror(errno)); goto end;
    }
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT); sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGUSR1); sigaddset(&mask, SIGUSR2);
    if (sigprocmask(SIG_BLOCK, &mask, NULL)) goto end;
    signal_fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (signal_fd < 0) goto end;
    int capture_fd = payload_loop_init();
    if (capture_fd < 0) { D_CRIT("Capture: %s\n", strerror(errno)); goto end; }
    if (service_init()) { D_CRIT("Status API: %s\n", strerror(errno)); goto end; }
    for (unsigned i = 0; i < cfg->bridge_slot; i++) {
        bool existed = if_nametoindex(cfg->bridge[i].ifname) != 0;
        if (iface_bridge_prepare(cfg->bridge[i].ifname, cfg->bridge_force)) {
            D_CRIT("Bridge %s: %s\n", cfg->bridge[i].ifname, strerror(errno)); goto end;
        }
        D_INFO("Bridge ready interface=%s created=%s\n", cfg->bridge[i].ifname,
               existed ? "false" : "true");
    }
    if (iface_ebtables_init()) { D_CRIT("Cannot initialize owned firewall chains\n"); goto end; }
    firewall_ready = true;
    provision_init(); provision_ready = true;
    if (notify_fd >= 0) {
        unsigned char status = EXIT_SUCCESS;
        if (write(notify_fd, &status, 1) != 1) goto end;
        close(notify_fd); notify_fd = -1;
    }
    D_INFO("%s listening on %s\n", PACKAGE_STRING, cfg->ifname);
    bool running = true;
    result = EXIT_SUCCESS;
    while (running) {
        struct pollfd fds[3 + SERVICE_MAX_CLIENTS] = {
            { .fd = signal_fd, .events = POLLIN }, { .fd = capture_fd, .events = POLLIN }
        };
        unsigned count = service_pollfds(fds + 2);
        int ready = poll(fds, count + 2, 250);
        if (ready < 0) { if (errno == EINTR) continue; result = EXIT_FAILURE; break; }
        if (fds[0].revents & POLLIN) {
            struct signalfd_siginfo info;
            while (read(signal_fd, &info, sizeof(info)) == sizeof(info)) {
                if (info.ssi_signo == SIGHUP) {
                    if (log_open(cfg->log_file))
                        D_CRIT("Log reopen failed path=%s error=%s\n", cfg->log_file,
                               strerror(errno));
                    else D_INFO("Log reopened path=%s\n", cfg->log_file);
                } else if (info.ssi_signo == SIGUSR1) provision_delall();
                else if (info.ssi_signo == SIGUSR2) provision_stats();
                else {
                    D_INFO("Stopping signal=%u\n", info.ssi_signo);
                    running = false;
                }
            }
        }
        if (!running) break;
        if ((fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) ||
            ((fds[1].revents & POLLIN) && payload_receive())) {
            D_CRIT("Capture stopped: %s\n", strerror(errno)); result = EXIT_FAILURE; break;
        }
        service_dispatch(fds + 2, count);
    }
end:
    service_end(); payload_loop_end();
    if (provision_ready) provision_end();
    if (firewall_ready && iface_ebtables_end()) result = EXIT_FAILURE;
    if (signal_fd >= 0) close(signal_fd);
    softgred_config_remove_pid_file();
    if (notify_fd >= 0) {
        unsigned char status = EXIT_FAILURE;
        (void)write(notify_fd, &status, 1); close(notify_fd);
    }
    D_INFO("Daemon stopped exit=%d\n", result);
    log_close();
    softgred_config_release();
    return result;
}
