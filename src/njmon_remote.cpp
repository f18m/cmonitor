/*
 * njmon_remote.cpp -- allows njmon to talk to InfluxDB
 * Developer: Francesco Montorsi
 * (C) Copyright 2018 Francesco Montorsi

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "influxdb.hpp"
#include "njmon.h"

#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <getopt.h>
#include <ifaddrs.h>
#include <iostream>
#include <memory.h>
#include <mntent.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pwd.h>
#include <signal.h>
#include <sstream>
#include <stdarg.h> /* va_list, va_start, va_arg, va_end */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/vfs.h>
#include <time.h>
#include <unistd.h>

std::string hostname_to_ip(const std::string& hostname)
{
    struct hostent* he;
    struct in_addr** addr_list;
    std::string ip;
    int i;

    if ((he = gethostbyname(hostname.c_str())) == NULL)
        return "";

    addr_list = (struct in_addr**)he->h_addr_list;
    for (i = 0; addr_list[i] != NULL; i++) {
        // Return the first one;
        ip = inet_ntoa(*addr_list[i]);
        return ip;
    }

    return "";
}

void NjmonCollectorApp::remote_create_influxdb_connection(const std::string& hostname, unsigned int port)
{
    std::string ipaddress = hostname_to_ip(hostname);
    if (ipaddress.empty()) {
        char buf[1024];
        herror(buf);
        fprintf(stderr, "hostname=%s to IP address convertion failed, bailing out: %s\n", hostname.c_str(), buf);
        exit(98);
    }

    m_influxdb_server = new influxdb_cpp::server_info(ipaddress, port, "njmon", "usr", "pwd");

    int ret_code = influxdb_cpp::builder()
                       .meas("foo")
                       .tag("k", "v")
                       .tag("x", "y")
                       .field("x", 10)
                       .field("y", 10.3, 2)
                       .field("z", 10.3456)
                       .field("b", !!10)
                       .timestamp(1512722735522840439)
                       .post_http(*m_influxdb_server);
    if (ret_code != 0)
        LogError("Failed sending sample to InfluxDB server");
}
