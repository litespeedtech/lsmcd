#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_LEN  65536
int sock_fd = -1;
unsigned char buffer[BUFFER_LEN];

int do_connect()
{
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0)
    {
        printf("Error in socket: %s\n", strerror(errno));
        return -1;
    }
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
    sa.sin_port = htons(11211);
    if (connect(sock_fd, (const struct sockaddr *)&sa, sizeof(sa)))
    {
        printf("Error in connect: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}


int do_login()
{
/* My login example:
0000: 80210005 00000000 00000013 00000000 .!..............
0010: 00000000 00000000 504c4149 4e007573 ........PLAIN.us
0020: 65720070 61737377 6f7264            er.password
*/
    memset(buffer, 0, 43);
    buffer[0] = 0x80;
    buffer[1] = 0x21;
    buffer[3] = 0x05;
    buffer[11] = 0x13;
    strcpy((char *)&buffer[24], "PLAIN");
    strcpy((char *)&buffer[30], "user");
    strcpy((char *)&buffer[35], "password");
    if (send(sock_fd, buffer, 43, 0) != 43)
    {
        printf("Error in send of login data: %s\n", strerror(errno));
        return -1;
    }
/* My login response example:
0000: 81210000 00000000 0000000d 00000000 .!..............
0010: 00000000 00000000 41757468 656e7469 ........Authenti
0020: 63617465 64                         cated
*/
    ssize_t recvd = recv(sock_fd, buffer, BUFFER_LEN, 0);
    if (recvd != 37)
    {
        printf("Error in login response: %s\n", strerror(errno));
        return -1;
    }
    buffer[37] = 0;
    if (buffer[0] != 0x81 ||
        buffer[1] != 0x21 ||
        buffer[11] != 0x0d ||
        strcmp((char *)&buffer[24], "Authenticated"))
    {
        printf("Error in login response data (turn on the trace)\n");
        return -1;
    }
    return 0;
}


int do_set()
{
/* My set example:
0000: 80010003 08000000 00000010 00000000 ................
0010: 00000000 00000000 00000000 00000000 ................
0020: 6b657976 616c7565                  keyvalue
*/
    memset(buffer, 0, 40);
    buffer[0] = 0x80;
    buffer[1] = 0x01;
    buffer[3] = 0x03;
    buffer[4] = 0x08;
    buffer[11] = 0x10;
    strcpy((char *)&buffer[32], "keyvalue");
    if (send(sock_fd, buffer, 40, 0) != 40)
    {
        printf("Error in send of set data: %s\n", strerror(errno));
        return -1;
    }

/* My set response:
0000: 81010000 00000000 00000000 00000000 ................
0010: 00000000 00006945                  ......iE
*/
    ssize_t recvd = recv(sock_fd, buffer, BUFFER_LEN, 0);
    if (recvd != 24)
    {
        printf("Error in set response: %s\n", strerror(errno));
        return -1;
    }
    buffer[37] = 0;
    if (buffer[0] != 0x81 ||
        buffer[1] != 0x01 ||
        buffer[6] != 0x00 ||
        buffer[7] != 0x00)
    {
        printf("Error in set response data (turn on the trace)\n");
        return -1;
    }
    return 0;
}


int do_get(bool should_fail)
{
/* My get example:
0000: 80000003 00000000 00000003 00000000 ................
0010: 00000000 00000000 6b6579            ........key
*/
    memset(buffer, 0, 27);
    buffer[0] = 0x80;
    buffer[3] = 0x03;
    buffer[11] = 0x03;
    strcpy((char *)&buffer[24], "key");
    if (send(sock_fd, buffer, 27, 0) != 27)
    {
        printf("Error in send of get data: %s\n", strerror(errno));
        return -1;
    }

/* My get response:
0000: 81000000 04000000 00000009 00000000 ................
0010: 00000000 00006945 00000000 76616c75 ......iE....valu
0020: 65                                  e
*/
    ssize_t recvd = recv(sock_fd, buffer, BUFFER_LEN, 0);
    if (recvd != 33)
    {
        if (should_fail)
        {
            printf("get failed as expected, rc: %d\n", buffer[6] * 0x80 + buffer[7]);
            return 0;
        }
        printf("Error in get response: %s\n", strerror(errno));
        return -1;
    }
    buffer[33] = 0;
    if (buffer[0] != 0x81 ||
        buffer[4] != 0x04 ||
        buffer[6] != 0x00 ||
        buffer[7] != 0x00 ||
        buffer[11] != 0x09 ||
        strcmp((char *)&buffer[28], "value"))
    {
        if (should_fail)
        {
            printf("get failed as expected, (data) rc: %d\n", buffer[6] * 0x80 + buffer[7]);
            return 0;
        }
        printf("Error in get response data (turn on the trace)\n");
        return -1;
    }
    return 0;
}


int do_multi1()
{
/* My multiple requests example (2 list_mechs): 
0000: 80200000 00000000 00000000 00010000 
0010: 00000000 00000000 80200000 00000000 
0020: 00000000 00010000 00000000 00000000  
*/
    memset(buffer, 0, 48);
    buffer[0] = 0x80;
    buffer[1] = 0x20;
    buffer[24] = 0x80;
    buffer[25] = 0x20; 
    if (send(sock_fd, buffer, 48, 0) != 48)
    {
        printf("Error in send of multi data1: %s\n", strerror(errno));
        return -1;
    }
/* My LIST_MECHS response 
0000: 81200000 00000000 00000005 00680000 . ...........h..
0010: 00000000 00000000 504c4149 4e       ........PLAIN
*/
    ssize_t recvd = recv(sock_fd, buffer, BUFFER_LEN, 0);
    if (recvd < 58)
    {
        printf("Error in list_mechs response1: %s (recvd: %ld)\n", strerror(errno), recvd);
        return -1;
    }
    if (buffer[0] != 0x81 ||
        buffer[1] != 0x20 ||
        buffer[6] != 0x00 ||
        buffer[7] != 0x00 ||
        buffer[11] < 0x05 ||
        memcmp((char *)&buffer[24], "PLAIN", 5))
    {
        printf("Error in list_mechs response data1 (turn on the trace)\n");
        return -1;
    }

/* My LIST_MECHS response 
0000: 81200000 00000000 00000005 00680000 . ...........h..
0010: 00000000 00000000 504c4149 4e       ........PLAIN
*/
    if (buffer[29+buffer[11]-5+0] != 0x81 ||
        buffer[29+buffer[11]-5+1] != 0x20 ||
        buffer[29+buffer[11]-5+6] != 0x00 ||
        buffer[29+buffer[11]-5+7] != 0x00 ||
        buffer[29+buffer[11]-5+11] < 0x05 ||
        memcmp((char *)&buffer[24], "PLAIN", 5))
    {
        printf("Error in list_mechs response data2 (turn on the trace)\n");
        return -1;
    }
    
    return 0;
}


int do_partial()
{
/* My partial requests example (turn off Nagle)
 *   - send the first 3 bytes
 *   - send the next 5 bytes
 *   - send the next 15 bytes
 *   - send the final byte
0000: 80200000 00000000 00000000 00010000 
0010: 00000000 00000000 
*/
    int one = 1;
    int parts[] = { 3, 5, 15, 1 };
    int total = 0, index = 0;
    if (setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)))
    {
        printf("Error in turning off Nagle: %s\n", strerror(errno));
        return -1;
    }
    memset(buffer, 0, 24);
    buffer[0] = 0x80;
    buffer[1] = 0x20;
    while (total < 24)
    {
        if (send(sock_fd, &buffer[total], parts[index], 0) != parts[index])
        {
            printf("Error in send of partial data #%d: %s\n", index, strerror(errno));
            return -1;
        }
        total = total + parts[index];
        ++index;
    }
/* My LIST_MECHS response 
0000: 81200000 00000000 00000005 00680000 . ...........h..
0010: 00000000 00000000 504c4149 4e       ........PLAIN
*/
    ssize_t recvd = recv(sock_fd, buffer, BUFFER_LEN, 0);
    if (recvd < 29)
    {
        printf("Error in list_mechs response1 (partial): %s (recvd: %ld)\n", strerror(errno), recvd);
        return -1;
    }
    if (buffer[0] != 0x81 ||
        buffer[1] != 0x20 ||
        buffer[6] != 0x00 ||
        buffer[7] != 0x00 ||
        buffer[11] < 0x05 ||
        memcmp((char *)&buffer[24], "PLAIN", 5))
    {
        printf("Error in list_mechs response data partial (turn on the trace)\n");
        return -1;
    }
    printf("Partial send ok\n");
    return 0;
}


int do_rehash()
{
/* My rehash request example
0000: 80200000 00000000 00000000 01020304 
0010: 00000000 00000000 
*/
    memset(buffer, 0, 24);
    buffer[0] = 0x80;
    buffer[1] = 0x20;
    buffer[12] = 0x01;
    buffer[13] = 0x02;
    buffer[14] = 0x03;
    buffer[15] = 0x04;
    if (send(sock_fd, buffer, 24, 0) != 24)
    {
        printf("Error in send of rehash: %s\n", strerror(errno));
        return -1;
    }
/* My LIST_MECHS response 
0000: 81200000 00000000 00000005 00680000 . ...........h..
0010: 00000000 00000000 504c4149 4e       ........PLAIN
*/
    ssize_t recvd = recv(sock_fd, buffer, BUFFER_LEN, 0);
    if (recvd < 29)
    {
        printf("Error in list_mechs response1 (partial): %s (recvd: %ld)\n", strerror(errno), recvd);
        return -1;
    }
    if (buffer[0] != 0x81 ||
        buffer[1] != 0x20 ||
        buffer[6] != 0x00 ||
        buffer[7] != 0x00 ||
        buffer[11] < 0x05 ||
        memcmp((char *)&buffer[24], "PLAIN", 5))
    {
        printf("Error in list_mechs response data rehash (turn on the trace)\n");
        return -1;
    }
    printf("Rehash send ok\n");
    return 0;
}


int do_multi2()
{
/* My multiple requests example: 
0000: 80200000 00000000 00000000 00010000 . ..............
0010: 00000000 00000000 80070000 00000000 ................
0020: 00000000 00020000 00000000 00000000 ................
*/
    memset(buffer, 0, 48);
    buffer[0] = 0x80;
    buffer[1] = 0x20;
    buffer[24] = 0x80;
    buffer[25] = 0x07; // QUIT - NO RESPONSE
    if (send(sock_fd, buffer, 48, 0) != 48)
    {
        printf("Error in send of multi data: %s\n", strerror(errno));
        return -1;
    }
/* My LIST_MECHS response 
0000: 81200000 00000000 00000005 00680000 . ...........h..
0010: 00000000 00000000 504c4149 4e       ........PLAIN
*/
    ssize_t recvd = recv(sock_fd, buffer, BUFFER_LEN, 0);
    if (recvd < 53)
    {
        printf("Error in list_mechs multi response: %s, size: %ld\n", strerror(errno), recvd);
        return -1;
    }
    if (buffer[0] != 0x81 ||
        buffer[1] != 0x20 ||
        buffer[6] != 0x00 ||
        buffer[7] != 0x00 ||
        buffer[11] < 0x05 ||
        memcmp((char *)&buffer[24], "PLAIN", 5),
        buffer[29 + buffer[11] - 5] != 0x81 ||
        buffer[30 + buffer[11] - 5] != 0x07)
    {
        printf("Error in list_mechs multi response data (turn on the trace)\n");
        return -1;
    }
    return 0;
}


int main(int argc, char *argv[])
{
    int ret = 1;
    int login = 0;
    int rehash = 0;
    int test = 0;
    int passed = 1;
    signal(SIGPIPE, SIG_IGN);    
    char opt;
    while ((opt = getopt(argc, argv, "lrth?")) != -1)
    {
        switch (opt)
        {
            case 'l':
                printf("Do login with the default user/password\n");
                login = 1;
                break;
            case 'r':
                printf("Force rehash\n");
                rehash = 1;
                break;
            case 't':
                printf("Test\n");
                test = 1;
                break;
            case 'h':
            case '?':
                printf("unittest for lsmcd and forces rehash\n");
                printf("Use: 'l' (login), 't' (test), 'r' (rehash)\n");
                return 1;
        }
    }
    if (!login && !rehash && !test)
    {
        printf("You did not specify an option\n");
        return 1;
    }
    if (do_connect())
        return 1;
    if (login)
    {
        if (do_login())
            passed = 0;
        else
        {
            if (do_set() || do_get(false))
                passed = 0;
            else if (do_multi2() ||   // multi2 must be last as it closes the connection    
                     do_connect())
                passed = 0; // Can't reconnect
        }
    }
    if (passed && test)
        if (do_multi1() || do_partial())
            passed = 0;
    if (passed && rehash)
        if (do_rehash())
            passed = 0;
    if (passed)
        printf("All tests passed\n");
    if (sock_fd > 0)
        close(sock_fd);
    return !passed;
}
