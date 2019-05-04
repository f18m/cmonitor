/*
 * njmon_remote.cpp -- collects Linux performance data and generates JSON format data.
 * Developer: Nigel Griffiths.
 * (C) Copyright 2018 Nigel Griffiths

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

#ifndef NOREMOTE
/* below incldes are for socket handling */
#include <arpa/inet.h>
#include <netinet/in.h>

void pexit(char* msg)
{
    perror(msg);
    exit(1);
}

int en[94] = { 8, 85, 70, 53, 93, 72, 61, 1, 41, 36, 49, 92, 44, 42, 25, 58, 81, 15, 57, 10, 54, 60, 12, 45, 43, 91, 22,
    86, 65, 9, 27, 18, 37, 39, 2, 68, 46, 71, 6, 79, 76, 84, 59, 75, 82, 4, 48, 55, 64, 3, 7, 56, 40, 73, 77, 69, 88,
    13, 35, 11, 66, 26, 52, 78, 28, 89, 51, 0, 30, 50, 34, 5, 32, 21, 14, 38, 19, 29, 24, 33, 47, 31, 80, 16, 83, 90,
    67, 23, 20, 17, 74, 62, 87, 63 };

int de[94] = { 67, 7, 34, 49, 45, 71, 38, 50, 0, 29, 19, 59, 22, 57, 74, 17, 83, 89, 31, 76, 88, 73, 26, 87, 78, 14, 61,
    30, 64, 77, 68, 81, 72, 79, 70, 58, 9, 32, 75, 33, 52, 8, 13, 24, 12, 23, 36, 80, 46, 10, 69, 66, 62, 3, 20, 47, 51,
    18, 15, 42, 21, 6, 91, 93, 48, 28, 60, 86, 35, 55, 2, 37, 5, 53, 90, 43, 40, 54, 63, 39, 82, 16, 44, 84, 41, 1, 27,
    92, 56, 65, 85, 25, 11, 4 };

void mixup(char* s)
{
    int i;
    for (i = 0; s[i]; i++) {
        if (s[i] <= ' ')
            continue;
        if (s[i] > '~')
            continue;
        s[i] = en[s[i] - 33] + 33;
    }
}

void unmix(char* s)
{
    int i;
    for (i = 0; s[i]; i++) {
        if (s[i] <= ' ')
            continue;
        if (s[i] > '~')
            continue;
        s[i] = de[s[i] - 33] + 33;
    }
}

void create_socket(char* ip_address, long port, char* hostname, char* utc, char* secretstr)
{
    int i;
    char buffer[8196];
    static struct sockaddr_in serv_addr;

    DEBUGLOG_FUNCTION_START();
    LogDebug("socket: trying to connect to %s:%ld\n", ip_address, port);
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        pexit("njmon:socket() call failed");

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip_address);
    serv_addr.sin_port = htons(port);

    /* Connect tot he socket offered by the web server */
    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
        pexit("njmon: connect() call failed");

    /* Now the sockfd can be used to communicate to the server the GET request */
    sprintf(buffer, "preamble-here njmon %s %s %s %s postamble-here", hostname, utc, secretstr, COLLECTOR_VERSION);
    LogDebug("hello string=\"%s\"\n", buffer);
    mixup(buffer);
    if (write(sockfd, buffer, strlen(buffer)) < 0)
        pexit("njmon: write() to socket failed");
}
#endif /* NOREMOTE */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
