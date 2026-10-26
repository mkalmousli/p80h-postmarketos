/* progress(): tell the screen what p80hd is doing. One datagram "<label>\t<pct>" to the abstract socket "p80h-ui"
 * (p80h-splash draws it as a ring + label; nobody listening is fine). pct -1 = unknown, empty label = done. */
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "p80hd.h"

void progress(const char *label, int pct)
{
	struct sockaddr_un a = {.sun_family = AF_UNIX};
	char msg[160];
	int s = socket(AF_UNIX, SOCK_DGRAM, 0), n = snprintf(msg, sizeof(msg), "%s\t%d", label, pct);

	if (s < 0)
		return;
	memcpy(a.sun_path + 1, "p80h-ui", 7);
	sendto(s, msg, n, MSG_DONTWAIT, (void *)&a, offsetof(struct sockaddr_un, sun_path) + 8);
	close(s);
}
