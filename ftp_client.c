#include "head.h"

int connect_serv(char *addr, int port)
{
    int sockfd;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&servaddr, 0, sizeof(struct sockaddr_in));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    inet_pton(AF_INET, addr, &servaddr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
    {
        perror("connect error");
        return -1;
    }

    return sockfd;
}

int main(int argc, char **argv)
{
    if (argc < 3 && argc > 4)
    {
        fprintf(stderr, "usage: %s <IP address> ls OR\n%s <IP address> get <filename> OR\n%s <IP address> put <filename>\n", argv[0], argv[0], argv[0]);
        return -1;
    }
    int sockfd = connect_serv(argv[1], 6666);
    int nread = 1;
    int nremain;
    int nwrite;
    char buf[1024];
    int fd;
    if (sockfd == -1)
    {
        return -1;
    }
    if (strcmp(argv[2], "ls") == 0)
    {
        write(sockfd, "0", 1);
        buf[0] = '\0';
        while (1)
        {
            nread = read(sockfd, buf, 127);
            if (nread == -1 && errno == EAGAIN)
            {
                continue;
            }
            else if (nread == 0)
            {
                break;
            }
            buf[nread] = '\0';
            printf("%s", buf);
        }
    }
    if (strcmp(argv[2], "get") == 0)
    {
        int len = strlen(argv[3]);
        buf[0] = '1';
        buf[1] = len & 0xff;
        buf[2] = (len >> 8) & 0xff;
        buf[3] = (len >> 16) & 0xff;
        buf[4] = (len >> 24) & 0xff;

        write(sockfd, buf, 5);
        write(sockfd, argv[3], len);

        read(sockfd, buf, 1);
        if (buf[0] == '0')
        {
            fprintf(stderr, "recv 0\n");
            return -1;
        }
        fd = open(argv[3], O_WRONLY | O_CREAT, 0777);
        if (fd == -1)
        {
            fprintf(stderr, "filename = %s", argv[3]);
            perror("open file error");
            return -1;
        }

        unsigned char size[4];
        nread = read(sockfd, size, 4);
        nremain = size[0] | size[1] << 8 | size[2] << 16 | size[3] << 24;
        if (nremain <= 0 || nread != 4)
        {
            fprintf(stderr,
                    "read remain error,, nread = %d, nremain = %d, %d %d %d %d\n",
                    nread, nremain, size[0], size[1], size[2], size[3]);
            return -1;
        }

        while (nremain > 0)
        {
            nread = read(sockfd, buf, 1024);
            if (nread < 0)
            {
                return -1;
            }
            nremain -= nread;
            write(fd, buf, nread);
        }
    }
    if (strcmp(argv[2], "put") == 0)
    {
        int fd = open(argv[3], O_RDONLY);
        if (fd == -1)
        {
            fprintf(stderr, "filename = %s", argv[3]);
            perror("open file error");
            return -1;
        }
        int len = strlen(argv[3]);
        buf[0] = '2';
        buf[1] = len & 0xff;
        buf[2] = (len >> 8) & 0xff;
        buf[3] = (len >> 16) & 0xff;
        buf[4] = (len >> 24) & 0xff;

        write(sockfd, buf, 5);
        write(sockfd, argv[3], len);


        struct stat info;
        fstat(fd, &info);
        len = info.st_size;
        buf[0] = len & 0xff;
        buf[1] = (len >> 8) & 0xff;
        buf[2] = (len >> 16) & 0xff;
        buf[3] = (len >> 24) & 0xff;
        nwrite = write(sockfd, buf, 4);
        if (nwrite < 0)
        {
            perror("write error");
            return -1;
        }

        while (len > 0)
        {
            nread = read(fd, buf, 1024);
            len -= nread;
            nwrite = write(sockfd, buf, nread);
            if (nwrite < 0)
            {
                perror("write error");
                return -1;
            }
        }
        close(fd);
    }
    return 0;
}