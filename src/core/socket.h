#ifndef __APE_SOCKET_H
#define __APE_SOCKET_H

#include "common.h"
#include "buffer.h"

#ifdef __WIN32

#include <winsock2.h>

#define ECONNRESET WSAECONNRESET
#define EINPROGRESS WSAEINPROGRESS
#define EALREADY WSAEALREADY
#define ECONNABORTED WSAECONNABORTED
#define ioctl ioctlsocket
#define hstrerror(x) ""
#else
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include <netdb.h>
#endif

#define APE_SOCKET_BACKLOG 2048

/* get a ape_socket pointer from event returns */
#define APE_SOCKET(attach) ((ape_socket *)attach)

#ifdef TCP_CORK
	#define PACK_TCP(fd) \
	do { \
		int __state = 1; \
		setsockopt(fd, IPPROTO_TCP, TCP_CORK, &__state, sizeof(__state)); \
	} while(0)

	#define FLUSH_TCP(fd) \
	do { \
		int __state = 0; \
		setsockopt(fd, IPPROTO_TCP, TCP_CORK, &__state, sizeof(__state)); \
	} while(0)
#else
	#define PACK_TCP(fd)
	#define FLUSH_TCP(fd)
#endif

typedef struct _ape_socket ape_socket;

typedef enum {
	APE_SOCKET_TCP,
	APE_SOCKET_UDP
} ape_socket_proto;

typedef enum {
	APE_SOCKET_UNKNOWN,
	APE_SOCKET_SERVER,
	APE_SOCKET_CLIENT
} ape_socket_type;

typedef enum {
	APE_SOCKET_ONLINE,
	APE_SOCKET_PROGRESS,
	APE_SOCKET_PENDING
} ape_socket_state;

typedef struct {
	void (*on_read)		(ape_socket *, ape_global *);
	void (*on_disconnect)	(ape_socket *, ape_global *);
	void (*on_connect)	(ape_socket *, ape_global *);
} ape_socket_callbacks;

struct _ape_socket {
	ape_fds s;
	
	buffer data_in;
	buffer data_out;
	
	struct {
		int fd;
		int offset;
	} file_out;
	
	void *ctx; 	/* public pointer */
	
	ape_socket_callbacks 	callbacks;
	ape_socket_type 	type;
	ape_socket_proto 	proto;
	ape_socket_state	state;
	
	uint16_t remote_port;
};

ape_socket *APE_socket_new(ape_socket_proto pt, int from);

int APE_socket_listen(ape_socket *socket, uint16_t port, const char *local_ip, ape_global *ape);
int APE_socket_connect(ape_socket *socket, uint16_t port, const char *remote_ip_host, ape_global *ape);

inline int ape_socket_accept(ape_socket *socket, ape_global *ape);
inline int ape_socket_read(ape_socket *socket, ape_global *ape);
int ape_socket_write_file(ape_socket *socket, const char *file, ape_global *ape);

#endif