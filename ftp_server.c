#include "head.h"

static char s_path[128];

int serv_init(int port, int listen_backlog)
{
	int listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd == -1)
	{
		perror("socket error");
		return -1;
	}

	int on = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (void *)&on, sizeof(on));

	struct sockaddr_in servaddr;

	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
	{
		perror("bind error");
		return -1;
	}

	if (listen(listenfd, listen_backlog) == -1)
	{
		perror("listen error");
		return -1;
	}

	int flag = fcntl(listenfd, F_GETFL, 0);
	fcntl(listenfd, F_SETFL, flag | O_NONBLOCK);

	return listenfd;
}

int send_file(int sockfd)
{
	int nread;
	int nremain;
	int nwrite;
	unsigned char buf[1024], path[128], *ptr;

	nread = read(sockfd, buf, 4);
	nremain = buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24;
	if (nremain <= 0 || nremain >= 128 || nread != 4)
	{
		fprintf(stderr,
				"read remain error,, nread = %d, nremain = %d, %d %d %d %d\n",
				nread, nremain, buf[0], buf[1], buf[2], buf[3]);
		return -1;
	}
	ptr = buf;
	while (nremain > 0)
	{
		nread = read(sockfd, ptr, nremain);
		if(nread <= 0)
		{
			if(nread == -1 && errno == EAGAIN)
			{
				continue;
			}
			return -1;
		}
		ptr[nread] = '\0';
		ptr += nread;
		nremain -= nread;
	}
	strncpy(path, s_path, strlen(s_path) + 1);
	strcat(path, buf);
	int fd = open(path, O_RDONLY);
	if (fd == -1)
	{
		fprintf(stderr, "filename = %s", path);
		perror("open file error");
		write(sockfd, "0", 1);
		return -1;
	}

	struct stat info;
	fstat(fd, &info);
	int filesize = info.st_size;
	buf[0] = '1';
	buf[1] = filesize & 0xff;
	buf[2] = (filesize >> 8) & 0xff;
	buf[3] = (filesize >> 16) & 0xff;
	buf[4] = (filesize >> 24) & 0xff;
	nwrite = write(sockfd, buf, 5);

	if (nwrite < 0)
	{
		perror("write error");
		return -1;
	}
	while (filesize > 0)
	{
		nread = read(fd, buf, 1024);
		filesize -= nread;
		nwrite = write(sockfd, buf, nread);
		if (nwrite < 0)
		{
			perror("write error");
			return -1;
		}
	}
	close(fd);
}

int recv_file(int sockfd)
{
	int nread;
	int nremain;
	int nwrite;
	unsigned char buf[1024], path[128], *ptr;

	nread = read(sockfd, buf, 4);
	nremain = buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24;
	if (nremain <= 0 || nremain > 128 || nread != 4)
	{
		fprintf(stderr,
				"read remain error,, nread = %d, nremain = %d, %d %d %d %d\n",
				nread, nremain, buf[0], buf[1], buf[2], buf[3]);
		return -1;
	}
	ptr = buf;
	while (nremain > 0)
	{
		nread = read(sockfd, ptr, nremain);
		if(nread <= 0)
		{
			if(nread == -1 && errno == EAGAIN)
			{
				continue;
			}
			return -1;
		}
		ptr[nread] = '\0';
		ptr += nread;
		nremain -= nread;
	}
	strncpy(path, s_path, strlen(s_path) + 1);
	strcat(path, buf);
	int fd = open(path, O_WRONLY | O_CREAT, 0777);
	if (fd == -1)
	{
		perror("open file error");
	}

	nread = read(sockfd, buf, 4);
	nremain = buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24;
	if (nremain <= 0 || nread != 4)
	{
		fprintf(stderr,
				"read remain error,, nread = %d, nremain = %d, %d %d %d %d\n",
				nread, nremain, buf[0], buf[1], buf[2], buf[3]);
		return -1;
	}
	while (nremain > 0)
	{
		nread = read(sockfd, buf, 1024);
		if(nread <= 0)
		{
			if(nread == -1 && errno == EAGAIN)
			{
				continue;
			}
			return -1;
		}
		nremain -= nread;
		nwrite = write(fd, buf, nread);
	}
	close(fd);
}

void free_epoll_event(int epfd, struct connection_data *cd)
{
	epoll_ctl(epfd, EPOLL_CTL_DEL, cd->fd, NULL);
	close(cd->fd);
	free(cd);
}

void do_read(int epfd, struct connection_data *cd)
{
	int nread;
	int nwrite;
	int buf[2];
	memset(buf, 0, 2);
	nread = read(cd->fd, buf, 1);
	if (nread <= 0)
	{
		fprintf(stderr, "read head error\n");
		free_epoll_event(epfd, cd);
		return;
	}
	if (buf[0] == '0')
	{
		fprintf(stderr, "recv commend ls\n");
		char buf[128];
		memset(buf, 0, 128);
		DIR *d = opendir(s_path);
		struct dirent *dir = readdir(d);
		while (dir)
		{
			nwrite = write(cd->fd, dir->d_name, strlen(dir->d_name));
			if (nwrite < 0)
			{
				perror("write error");
				return;
			}
			if (nwrite != strlen(dir->d_name))
			{
				free_epoll_event(epfd, cd);
			}
			write(cd->fd, "\n", 1);
			if (nwrite < 0)
			{
				perror("write error");
				return;
			}
			dir = readdir(d);
		}
		free_epoll_event(epfd, cd);
	}
	else if (buf[0] == '1')
	{
		fprintf(stderr, "recv commend get\n");
		send_file(cd->fd);
		free_epoll_event(epfd, cd);
	}
	else if (buf[0] == '2')
	{
		fprintf(stderr, "recv commend put\n");
		recv_file(cd->fd);
		free_epoll_event(epfd, cd);
	}
}

void do_error(int epfd, struct connection_data *cd)
{
	//free
	fprintf(stderr, "connection closed\n");
	free_epoll_event(epfd, cd);
}

void do_accept(int epfd, struct connection_data *cd)
{
	int listenfd = cd->fd;

	struct sockaddr_in cliaddr;
	socklen_t len = sizeof(cliaddr);

	int fd = accept(listenfd, (struct sockaddr *)&cliaddr, &len);
	if (fd == -1)
	{
		perror("accept error");
		return;
	}

	/*int flag = fcntl(listenfd, F_GETFL, 0);
	fcntl(listenfd, F_SETFL, flag | O_NONBLOCK);*/

	struct connection_data *ptr = (struct connection_data *)malloc(sizeof(struct connection_data));
	ptr->fd = fd;
	ptr->read_cb = do_read;
	ptr->write_cb = NULL;
	ptr->error_cb = do_error;
	ptr->arg = NULL;

	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLERR;
	ev.data.ptr = ptr;

	if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1)
	{
		perror("add client connect event error");
		return;
	}
}

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "usage: %s <serv path>\n", argv[0]);
		return -1;
	}
	struct stat buf;
	stat(argv[1], &buf);
	if (S_ISDIR(buf.st_mode) == 0)
	{
		fprintf(stderr, "%s is not a dir\n", argv[1]);
		return -1;
	}
	strcpy(s_path, argv[1]);
	int sleeptime = 0;
	int listenfd = -1;
	while (listenfd == -1)
	{
		sleeptime++;
		listenfd = serv_init(6666, 8);
		if (listenfd == -1)
		{
			sleep(sleeptime);
		}
	}

	int epollfd = epoll_create(1);
	if (epollfd == -1)
	{
		perror("epoll create error");
		return -1;
	}

	struct connection_data *ptr = (struct connection_data *)malloc(sizeof(struct connection_data));
	ptr->fd = listenfd;
	ptr->read_cb = do_accept;
	ptr->write_cb = NULL;
	ptr->error_cb = NULL;
	ptr->arg = NULL;

	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLERR;
	ev.data.ptr = ptr;

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev) == -1)
	{
		perror("add linten event error");
		return -1;
	}

	struct epoll_event ready_ev[10];
	int nready;
	while (1)
	{
		nready = epoll_wait(epollfd, ready_ev, 10, -1);
		if (nready > 0)
		{
			int i;
			for (i = 0; i < nready; i++)
			{
				if (ready_ev[i].events & EPOLLIN)
				{
					ptr = (struct connection_data *)ready_ev[i].data.ptr;
					if (ptr->read_cb != NULL)
						ptr->read_cb(epollfd, ready_ev[i].data.ptr); //调用read_cb
				}
				if (ready_ev[i].events & EPOLLOUT)
				{
					ptr = (struct connection_data *)ready_ev[i].data.ptr;
					if (ptr->write_cb != NULL)
						ptr->write_cb(epollfd, ready_ev[i].data.ptr);
				}
				if (ready_ev[i].events & EPOLLERR)
				{
					ptr = (struct connection_data *)ready_ev[i].data.ptr;
					if (ptr->error_cb != NULL)
						ptr->error_cb(epollfd, ready_ev[i].data.ptr);
				}
			}
		}
		else if (nready < 0)
		{
			perror("epoll wait error");
			return -1;
		}
	}
}