/* progress(): tell the screen what p80hd is doing. One datagram "<label>\t<permille>" to the abstract socket "p80h-ui"
 * (p80h-splash draws the bottom progress bar; nobody listening is fine). pm -1 = unknown, empty label = done. */
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "p80hd.h"

void progress(const char *label, int pm)
{
	struct sockaddr_un a = {.sun_family = AF_UNIX};
	char msg[160];
	static int s = -1;
	int n = snprintf(msg, sizeof(msg), "%s\t%d", label, pm);

	if (s < 0 && (s = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0)) < 0)
		return;
	memcpy(a.sun_path + 1, "p80h-ui", 7);
	sendto(s, msg, n, MSG_DONTWAIT, (void *)&a, offsetof(struct sockaddr_un, sun_path) + 8);
}
