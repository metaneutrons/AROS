/*
 * AROS SSH Server (sshd)
 *
 * Minimal SSH-2 server using libssh2 + mbedTLS.
 * Single-user (root), public-key + optional password auth.
 *
 * Config files (ENVARC:SSH/):
 *   host_key        — server RSA key (auto-generated on first run)
 *   authorized_keys — one public key per line (OpenSSH format)
 *   password        — SHA-256 hash (optional, enables password auth)
 *
 * Each connection spawns a new process with a CON: shell.
 */

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <dos/dostags.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <libssh2.h>
#include <string.h>
#include <stdio.h>

#define SSHD_PORT       22
#define SSHD_PREFS      "ENVARC:SSH/"
#define SSHD_HOST_KEY   SSHD_PREFS "host_key"
#define SSHD_AUTH_KEYS  SSHD_PREFS "authorized_keys"
#define SSHD_PASSWORD   SSHD_PREFS "password"

struct Library *SocketBase;

/* Forward declarations */
static BOOL auth_publickey(LIBSSH2_SESSION *session, const char *pubkey_blob, size_t len);
static BOOL auth_password(const char *pw);
static void handle_session(int client_fd);
static BOOL generate_host_key(void);
static BOOL load_host_key(LIBSSH2_SESSION *session);

int main(int argc, char **argv)
{
    int server_fd, client_fd;
    struct sockaddr_in addr, client_addr;
    socklen_t addrlen;

    SocketBase = OpenLibrary("bsdsocket.library", 4);
    if (!SocketBase)
    {
        Printf("sshd: Cannot open bsdsocket.library\n");
        return 20;
    }

    libssh2_init(0);

    /* Generate host key if missing */
    if (!Open(SSHD_HOST_KEY, MODE_OLDFILE))
        generate_host_key();

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        Printf("sshd: socket() failed\n");
        return 20;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SSHD_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        Printf("sshd: bind() failed (port %ld)\n", (long)SSHD_PORT);
        CloseSocket(server_fd);
        return 20;
    }

    listen(server_fd, 5);
    Printf("sshd: Listening on port %ld\n", (long)SSHD_PORT);

    for (;;)
    {
        addrlen = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
        if (client_fd < 0)
            continue;

        /* Spawn handler process for this connection */
        struct TagItem proctags[] = {
            { NP_Entry,    (IPTR)handle_session },
            { NP_Name,     (IPTR)"SSH Session" },
            { NP_Priority, 0 },
            { NP_StackSize, 65536 },
            { TAG_DONE,    0 }
        };
        struct Process *p = CreateNewProc(proctags);
        if (p)
        {
            /* Pass client_fd to the new process via UserData */
            p->pr_Task.tc_UserData = (APTR)(IPTR)client_fd;
        }
        else
        {
            CloseSocket(client_fd);
        }
    }

    /* Not reached */
    CloseSocket(server_fd);
    libssh2_exit();
    CloseLibrary(SocketBase);
    return 0;
}

/*
 * handle_session — per-connection SSH handler (runs as separate process).
 */
static void handle_session(int client_fd)
{
    LIBSSH2_SESSION *session;
    LIBSSH2_CHANNEL *channel;
    BOOL authenticated = FALSE;

    session = libssh2_session_init();
    if (!session) goto done;

    libssh2_session_set_blocking(session, 1);

    if (libssh2_session_handshake(session, client_fd) != 0)
        goto done;

    load_host_key(session);

    /* Authentication: try public key first, then password */
    /* libssh2 server-side auth is handled via callbacks */
    /* For now, accept any "root" user with valid key or password */

    /* TODO: Implement full server-side auth via libssh2 callbacks
     * This requires libssh2_server_* API which is not yet stable.
     * Alternative: use raw SSH transport with mbedTLS directly.
     */

    if (!authenticated)
        goto done;

    /* Open shell channel */
    channel = libssh2_channel_open_session(session);
    if (!channel) goto done;

    libssh2_channel_request_pty(channel, "vt100");
    libssh2_channel_shell(channel);

    /* Bridge channel I/O to a CON: handler */
    /* TODO: Create CON: process and bridge stdin/stdout */

    libssh2_channel_close(channel);
    libssh2_channel_free(channel);

done:
    if (session)
    {
        libssh2_session_disconnect(session, "Bye");
        libssh2_session_free(session);
    }
    CloseSocket(client_fd);
}

/*
 * auth_password — check password against ENVARC:SSH/password hash.
 */
static BOOL auth_password(const char *pw)
{
    BPTR fh = Open(SSHD_PASSWORD, MODE_OLDFILE);
    char stored_hash[65];
    if (!fh) return FALSE;  /* No password file = password auth disabled */

    if (Read(fh, stored_hash, 64) == 64)
    {
        stored_hash[64] = '\0';
        /* TODO: SHA-256 hash pw and compare with stored_hash */
    }
    Close(fh);
    return FALSE;  /* Placeholder */
}

/*
 * generate_host_key — create RSA host key on first run.
 */
static BOOL generate_host_key(void)
{
    /* TODO: Use mbedtls_rsa_gen_key() to generate 2048-bit RSA key
     * and write PEM to ENVARC:SSH/host_key */
    Printf("sshd: TODO — generate host key\n");
    return FALSE;
}

static BOOL load_host_key(LIBSSH2_SESSION *session)
{
    (void)session;
    /* TODO: Load PEM key from ENVARC:SSH/host_key */
    return FALSE;
}
