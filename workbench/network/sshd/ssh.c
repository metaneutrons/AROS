/*
 * AROS SSH Client using libssh.
 *
 * Usage: ssh [-i keyfile] [-p port] [user@]host [command]
 *
 * Auth: public-key (default: ENVARC:SSH/id_rsa) or password prompt.
 * If no command given, opens interactive shell.
 */

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>

#include <libssh/libssh.h>
#include <string.h>
#include <stdio.h>

#define DEFAULT_KEY     "ENVARC:SSH/id_rsa"
#define DEFAULT_PORT    22
#define DEFAULT_USER    "root"

struct Library *SocketBase;

static int authenticate(ssh_session session, const char *keyfile, const char *user)
{
    int rc;

    /* Try public key first */
    ssh_key privkey;
    rc = ssh_pki_import_privkey_file(keyfile, NULL, NULL, NULL, &privkey);
    if (rc == SSH_OK)
    {
        rc = ssh_userauth_publickey(session, user, privkey);
        ssh_key_free(privkey);
        if (rc == SSH_AUTH_SUCCESS)
            return 0;
    }

    /* Fallback: password */
    char password[128];
    Printf("Password: ");
    Flush(Output());
    /* Simple password read (no echo suppression on AROS) */
    FGets(Input(), password, sizeof(password));
    int len = strlen(password);
    if (len > 0 && password[len-1] == '\n') password[len-1] = '\0';

    rc = ssh_userauth_password(session, user, password);
    memset(password, 0, sizeof(password));

    return (rc == SSH_AUTH_SUCCESS) ? 0 : -1;
}

int main(int argc, char **argv)
{
    const char *host = NULL, *user = DEFAULT_USER;
    const char *keyfile = DEFAULT_KEY, *command = NULL;
    int port = DEFAULT_PORT;
    int i;

    /* Parse args */
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-i") == 0 && i+1 < argc)
            keyfile = argv[++i];
        else if (strcmp(argv[i], "-p") == 0 && i+1 < argc)
            port = atoi(argv[++i]);
        else if (!host)
        {
            /* user@host */
            char *at = strchr(argv[i], '@');
            if (at) { *at = '\0'; user = argv[i]; host = at + 1; }
            else host = argv[i];
        }
        else
            command = argv[i];
    }

    if (!host)
    {
        Printf("Usage: ssh [-i keyfile] [-p port] [user@]host [command]\n");
        return 5;
    }

    SocketBase = OpenLibrary("bsdsocket.library", 4);
    if (!SocketBase) { Printf("ssh: no bsdsocket.library\n"); return 20; }

    ssh_init();
    ssh_session session = ssh_new();

    ssh_options_set(session, SSH_OPTIONS_HOST, host);
    ssh_options_set(session, SSH_OPTIONS_PORT, &port);
    ssh_options_set(session, SSH_OPTIONS_USER, user);

    if (ssh_connect(session) != SSH_OK)
    {
        Printf("ssh: connect failed: %s\n", ssh_get_error(session));
        ssh_free(session); return 20;
    }

    /* TODO: Known hosts verification (ENVARC:SSH/known_hosts) */

    if (authenticate(session, keyfile, user) != 0)
    {
        Printf("ssh: authentication failed\n");
        ssh_disconnect(session); ssh_free(session); return 20;
    }

    ssh_channel channel = ssh_channel_new(session);
    ssh_channel_open_session(channel);

    if (command)
    {
        /* Execute single command */
        ssh_channel_request_exec(channel, command);
        char buf[4096];
        int nbytes;
        while ((nbytes = ssh_channel_read(channel, buf, sizeof(buf), 0)) > 0)
            Write(Output(), buf, nbytes);
        /* Also read stderr */
        while ((nbytes = ssh_channel_read(channel, buf, sizeof(buf), 1)) > 0)
            Write(Output(), buf, nbytes);
    }
    else
    {
        /* Interactive shell */
        ssh_channel_request_pty(channel);
        ssh_channel_request_shell(channel);

        Printf("Connected to %s. Type 'exit' to disconnect.\n", host);

        char buf[4096];
        int nbytes;
        /* Simple loop — no async I/O yet */
        while (ssh_channel_is_open(channel) && !ssh_channel_is_eof(channel))
        {
            nbytes = ssh_channel_read_nonblocking(channel, buf, sizeof(buf), 0);
            if (nbytes > 0)
                Write(Output(), buf, nbytes);

            if (WaitForChar(Input(), 100000))  /* 100ms timeout */
            {
                nbytes = Read(Input(), buf, sizeof(buf));
                if (nbytes > 0)
                    ssh_channel_write(channel, buf, nbytes);
            }
        }
    }

    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    ssh_disconnect(session);
    ssh_free(session);
    ssh_finalize();
    CloseLibrary(SocketBase);
    return 0;
}
