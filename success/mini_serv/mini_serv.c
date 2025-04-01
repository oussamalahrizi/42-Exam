#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netinet/in.h>


int g_id = 0;
fd_set our_sockets, read_set, write_set;
int server_fd = -1;

typedef struct s_client
{
	int fd;
	int id;
    char *msg;
	struct s_client *next;
}	client;


void fatal()
{
	char *str = "Fatal Error\n";
	write(2, str, strlen(str));
	close(server_fd);
	exit(1);
}



char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		fatal();
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}


int max_fd(client* cl)
{
	int max = server_fd;
	client *head = cl;
	while (head)
	{
		if (head->fd > max)
			max = head->fd;
		head = head->next;
	}
	return max;
}

void broadcast(char *str, client *clients, int s)
{
	client *head = clients;
	while (head)
	{
		if (head->fd != s && FD_ISSET(head->fd, &write_set))
		{
			int ret = send(head->fd, str, strlen(str), 0);
			if (ret < 0)
				fatal();
		}
		head = head->next;
	}
}

client* add_client(client *clients)
{
	int client_fd = accept(server_fd, 0, 0);
	if (client_fd < 0)
		fatal();
	client *new = malloc(sizeof(client));
	if (!new)
		fatal();
    
	FD_SET(client_fd, &our_sockets);
	new->fd = client_fd;
	new->id = g_id;
    new->msg = malloc(1);
    new->msg = 0;
	new->next = NULL;
	g_id++;

	char msg[10000];
	sprintf(msg, "server: client %d just arrived\n", new->id);
	broadcast(msg, clients, client_fd);
	return new;
}

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				fatal();
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

void handle_message(char* buffer, client* cl, client *clients)
{

	char formatted_msg[42 * 1024];
    cl->msg = str_join(cl->msg, buffer);
    char *tmp;
    while (extract_message(&cl->msg, &tmp)) {
        sprintf(formatted_msg, "client %d: %s", cl->id, tmp);
        broadcast(formatted_msg, clients, cl->fd);
    }
}

void remove_client(client** clients, client* rm)
{
	client * prev = NULL;
	client *head = *clients;
	FD_CLR(rm->fd, &our_sockets);
	close(rm->fd);
	while (head)
	{
		if (head == rm)
		{
			if (prev)
				prev->next = head->next;
			else
				*clients = head->next; // this is null if there was one client and to be removed 
		}
		prev = head;
		head = head->next;
	}
	char msg[10000];
	sprintf(msg, "server: client %d just left\n", rm->id);
	broadcast(msg, *clients, rm->fd);
    free(rm->msg);
	free(rm);
}

int main(int ac, char **av)
{
	int connfd, len;
	struct sockaddr_in servaddr, cli;
	client *clients = NULL;

	if (ac != 2)
	{
		char *buf = "Wrong number of arguments\n";
		write(2, buf, strlen(buf));
		exit(1);
	}
	// socket create and verification 
	server_fd = socket(AF_INET, SOCK_STREAM, 0); 
	if (server_fd == -1)
		fatal();
	bzero(&servaddr, sizeof(servaddr)); 
	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1])); 
  
	// Binding newly created socket to given IP and verification 
	if ((bind(server_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		fatal();
	if (listen(server_fd, 10) != 0)
		fatal();
	FD_ZERO(&our_sockets);
	FD_SET(server_fd, &our_sockets);
	
	while (1)
	{
		read_set = our_sockets;
		write_set = our_sockets;
		if (select(max_fd(clients) + 1, &read_set, &write_set, NULL, NULL) < 0)
            fatal();
		if (FD_ISSET(server_fd, &read_set))
		{
			// new client
			client* new = add_client(clients);
			if (!clients)
				clients = new;
			else
            {
                client *tmp = clients;
                while (tmp->next)
                    tmp = tmp->next;
                tmp->next = new;
            }
		}
		client *curr = clients;
		while (curr)
		{
			if (FD_ISSET(curr->fd, &read_set))
			{
				char buffer[4095];
				int ret = recv(curr->fd, buffer, 4094, 0);
				if (ret <= 0)
				{
					remove_client(&clients, curr);
					curr = clients;
                    continue;
				}
				buffer[ret] = 0;
				handle_message(buffer, curr, clients);
			}
			curr = curr->next;
		}
	}
	return 0;
}