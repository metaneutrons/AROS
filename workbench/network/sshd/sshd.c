/*
 * AROS SSH Server using libssh.
 *
 * Auth: public-key (ENVARC:SSH/authorized_keys) + optional password.
 * Username: always "root". One process per connection.
 * Shell sessions bridged to CON: handler.
 */

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <dos/dostags.h>

#include <libssh/libssh.h>
#include <libssh/server.h>
#include <libssh/callbacks.h>

#include <string.h>
#include <stdio.h>

#define SSHD_PORT       22
#define SSHD_HOST_KEY   "ENVARC:SSH/host_key"
#define SSHD_AUTH_KEYS  "ENVARC:SSH/authorized_keys"
#define SSHD_PASSWORD   "ENVARC:SSH/password"

struct Library *SocketBase;

static int auth_pubkey_cb(ssh_session session, const char *user,
                          struct ssh_key_struct *pubkey,
                          char signature_state, void *userdata)
{
    (void)session; (void)userdata;

    if (strcmp(user, "root") != 0)
        return SSH_AUTH_DENIED;

    if (signature_state == SSH_PUBLICKEY_STATE_NONE)
        return SSH_AUTH_SUCCESS;  /* Accept key for probing */

    if (signature_state != SSH_PUBLICKEY_STATE_VALID)
        return SSH_AUTH_DENIED;

    /* Check key against authorized_keys */
    ssh_key ref_key;
    BPTR fh = Open(SSHD_AUTH_KEYS, MODE_OLDFILE);
    if (!fh) return SSH_AUTH_DENIED;

    char line[1024];
    BOOL found = FALSE;
    while (FGets(fh, line, sizeof(line)))
    {
        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\n') continue;

        if (ssh_pki_import_pubkey_base64(line, SSH_KEYTYPE_UNKNOWN, &ref_key) == SSH_OK)
        {
            if (ssh_key_cmp(pubkey, ref_key, SSH_KEY_CMP_PUBLIC) == 0)
                found = TRUE;
            ssh_key_free(ref_key);
            if (found) break;
        }
    }
    Close(fh);

    return found ? SSH_AUTH_SUCCESS : SSH_AUTH_DENIED;
}

static int auth_password_cb(ssh_session session, const char *user,
                            const char *password, void *userdata)
{
    (void)session; (void)userdata;

    if (strcmp(user, "root") != 0)
        return SSH_AUTH_DENIED;

    /* Password auth only if ENVARC:SSH/password exists */
    BPTR fh = Open(SSHD_PASSWORD, MODE_OLDFILE);
    if (!fh) return SSH_AUTH_DENIED;

    char stored[65] = {0};
    Read(fh, stored, 64);
    Close(fh);

    /* TODO: SHA-256 hash 'password' and compare with 'stored' */
    /* For now, direct comparison (insecure placeholder) */
    if (strlen(stored) > 0 && strncmp(password, stored, strlen(stored)) == 0)
        return SSH_AUTH_SUCCESS;

    return SSH_AUTH_DENIED;
}

static void handle_session(ssh_session session)
{
    struct ssh_server_callbacks_struct cb = {
        .auth_pubkey_function = auth_pubkey_cb,
        .auth_password_function = auth_password_cb,
    };
    ssh_callbacks_init(&cb);
    ssh_set_server_callbacks(session, &cb);

    if (ssh_handle_key_exchange(session) != SSH_OK)
        goto done;

    /* Wait for auth */
    ssh_event event = ssh_event_new();
    ssh_event_add_session(event, session);

    int auth_attempts = 0;
    while (!ssh_is_authenticated(session) && auth_attempts < 10)
    {
        ssh_event_dopoll(event, -1);
        auth_attempts++;
    }

    if (!ssh_is_authenticated(session))
        goto done;

    /* Wait for channel request */
    ssh_channel channel = NULL;
    ssh_message msg;
    while ((msg = ssh_message_get(session)) != NULL)
    {
        if (ssh_message_type(msg) == SSH_REQUEST_CHANNEL_OPEN &&
            ssh_message_subtype(msg) == SSH_CHANNEL_SESSION)
        {
            channel = ssh_message_channel_request_open_reply_accept(msg);
            ssh_message_free(msg);
            break;
        }
        ssh_message_reply_default(msg);
        ssh_message_free(msg);
    }

    if (!channel) goto done;

    /* Wait for shell/exec request */
    BOOL got_shell = FALSE;
    while ((msg = ssh_message_get(session)) != NULL)
    {
        if (ssh_message_type(msg) == SSH_REQUEST_CHANNEL &&
            (ssh_message_subtype(msg) == SSH_CHANNEL_REQUEST_SHELL ||
             ssh_message_subtype(msg) == SSH_CHANNEL_REQUEST_PTY))
        {
            ssh_message_channel_request_reply_success(msg);
            if (ssh_message_subtype(msg) == SSH_CHANNEL_REQUEST_SHELL)
                got_shell = TRUE;
        }
        else
        {
            ssh_message_reply_default(msg);
        }
        ssh_message_free(msg);
        if (got_shell) break;
    }

    if (!got_shell) goto done;

    /* Shell loop: bridge SSH channel ↔ AROS Shell */
    ssh_channel_write(channel, "AROS Shell\r\n1.> ", 16);

    char buf[4096];
    int nbytes;
    while (ssh_channel_is_open(channel) && !ssh_channel_is_eof(channel))
    {
        nbytes = ssh_channel_read(channel, buf, sizeof(buf) - 1, 0);
        if (nbytes > 0)
        {
            buf[nbytes] = '\0';
            /* TODO: Feed to SystemTagList() or CON: handler */
            /* Echo for now */
            ssh_channel_write(channel, buf, nbytes);
        }
        else if (nbytes < 0)
            break;
    }

    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);

done:
    ssh_event_free(event);
    ssh_disconnect(session);
    ssh_free(session);
}

int main(int argc, char **argv)
{
    ssh_bind sshbind;

    SocketBase = OpenLibrary("bsdsocket.library", 4);
    if (!SocketBase) { Printf("sshd: no bsdsocket.library\n"); return 20; }

    ssh_init();
    sshbind = ssh_bind_new();

    ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDPORT_STR, "22");
    ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_HOSTKEY, SSHD_HOST_KEY);

    if (ssh_bind_listen(sshbind) < 0)
    {
        Printf("sshd: bind failed: %s\n", ssh_get_error(sshbind));
        return 20;
    }

    Printf("sshd: listening on port 22\n");

    for (;;)
    {
        ssh_session session = ssh_new();
        if (ssh_bind_accept(sshbind, session) == SSH_OK)
        {
            /* TODO: CreateNewProc for concurrent sessions */
            handle_session(session);
        }
        else
        {
            ssh_free(session);
        }
    }

    ssh_bind_free(sshbind);
    ssh_finalize();
    CloseLibrary(SocketBase);
    return 0;
}
