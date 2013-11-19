
#include <strings.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>


static pthread_t t;


static int con(const char *host, int port)  {
	int	sd;
	struct	sockaddr_in server;
	struct  hostent *hp, *gethostbyname();

	sd = socket (AF_INET,SOCK_STREAM,0);

	server.sin_family = AF_INET;
	hp = gethostbyname(host);
	bcopy ( hp->h_addr, &(server.sin_addr.s_addr), hp->h_length);
	server.sin_port = htons(port);

	connect(sd, &server, sizeof(server));

	return sd;
}

void eshc_get_current_version() {



}
