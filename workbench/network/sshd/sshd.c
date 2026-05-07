/*
 * Copyright (C) 2026, The AROS Development Team. All rights reserved.
 *
 * AROS SSH Server (sshd) using libssh 0.12.0 + mbedTLS 3.6.6
 *
 * Features:
 *   - Host key generation on first run (RSA 2048)
 *   - Public-key auth (ENVARC:SSH/authorized_keys, OpenSSH format)
 *   - Password auth (SHA-256 hash in ENVARC:SSH/password)
 *   - Concurrent sessions via CreateNewProc
 *   - Shell bridge via PIPE: handler
 *   - CTRL-C signal to shutdown
 */

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <exec/types.h>
#include <dos/dostags.h>
#include <dos/dos.h>

#include <libssh/libssh.h>
#include <libssh/server.h>
#include <libssh/callbacks.h>

#include <mbedtls/pk.h>
#include <mbedtls/rsa.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/sha256.h>
#include <mbedtls/error.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define SSHD_PORT           22
#define SSHD_HOST_KEY       "ENVARC:SSH/host_key"
#define SSHD_AUTH_KEYS      "ENVARC:SSH/authorized_keys"
#define SSHD_PASSWORD_FILE  "ENVARC:SSH/password"
#define SSHD_CONFIG_DIR     "ENVARC:SSH"
#define SSHD_BANNER         "AROS SSH Server"
#define RSA_KEY_BITS        2048
#define MAX_LINE            1024
#define BUF_SIZE            4096

struct Library *SocketBase;
static volatile BOOL g_shutdown = FALSE;

/* --- Utility: SHA-256 hex digest --- */
static void sha256_hex(const char *input, char *output)
{
    unsigned char hash[32];
    mbedtls_sha256((const unsigned char *)input, strlen(input), hash, 0);

    int i;
    for (i = 0; i < 32; i++)
        sprintf(output + (i * 2), "%02x", hash[i]);
    output[64] = '\0';
}

/* --- Host key generation using mbedTLS --- */
static BOOL generate_host_key(void)
{
    mbedtls_pk_context pk;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    const char *pers = "aros_sshd_keygen";
    unsigned char buf[16384];
    int ret;
    BPTR fh;

    Printf("sshd: Generating RSA %ld-bit host key...\n", (long)RSA_KEY_BITS);

    mbedtls_pk_init(&pk);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                 (const unsigned char *)pers, strlen(pers));
    if (ret != 0) goto fail;

    ret = mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
    if (ret != 0) goto fail;

    ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(pk), mbedtls_ctr_drbg_random,
                               &ctr_drbg, RSA_KEY_BITS, 65537);
    if (ret != 0) goto fail;

    /* Write PEM to buffer */
    memset(buf, 0, sizeof(buf));
    ret = mbedtls_pk_write_key_pem(&pk, buf, sizeof(buf));
    if (ret != 0) goto fail;

    /* Ensure config directory exists */
    BPTR lock = CreateDir(SSHD_CONFIG_DIR);
    if (lock) UnLock(lock);

    /* Save to file */
    fh = Open(SSHD_HOST_KEY, MODE_NEWFILE);
    if (!fh) goto fail;

    Write(fh, buf, strlen((char *)buf));
    Close(fh);

    Printf("sshd: Host key saved to %s\n", (IPTR)SSHD_HOST_KEY);

    mbedtls_pk_free(&pk);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return TRUE;

fail:
    Printf("sshd: Failed to generate host key (mbedtls error %ld)\n", (long)ret);
    mbedtls_pk_free(&pk);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return FALSE;
}

/* --- Ensure host key exists --- */
static BOOL ensure_host_key(void)
{
    BPTR lock = Lock(SSHD_HOST_KEY, SHARED_LOCK);
    if (lock)
    {
        UnLock(lock);
        return TRUE;
    }
    return generate_host_key();
}

/* --- Public key authentication callback --- */
static int auth_pubkey_cb(ssh_session session, const char *user,
                          struct ssh_key_struct *pubkey,
                          char signature_state, void *userdata)
{
    (void)session; (void)userdata;

    if (strcmp(user, "root") != 0)
        return SSH_AUTH_DENIED;

    /* Probe phase: accept any key type for further verification */
    if (signature_state == SSH_PUBLICKEY_STATE_NONE)
        return SSH_AUTH_SUCCESS;

    if (signature_state != SSH_PUBLICKEY_STATE_VALID)
        return SSH_AUTH_DENIED;

    /* Verify key against authorized_keys */
    BPTR fh = Open(SSHD_AUTH_KEYS, MODE_OLDFILE);
    if (!fh)
        return SSH_AUTH_DENIED;

    char line[MAX_LINE];
    BOOL found = FALSE;

    while (FGets(fh, line, sizeof(line)))
    {
        /* Strip newline */
        int len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\0')
            continue;

        /* OpenSSH format: "type base64 comment" - extract base64 part */
        char *space1 = strchr(line, ' ');
        if (!space1) continue;

        char *b64_start = space1 + 1;
        char *space2 = strchr(b64_start, ' ');
        if (space2) *space2 = '\0';

        /* Determine key type from prefix */
        enum ssh_keytypes_e ktype = SSH_KEYTYPE_UNKNOWN;
        if (strncmp(line, "ssh-rsa", 7) == 0)
            ktype = SSH_KEYTYPE_RSA;
        else if (strncmp(line, "ssh-ed25519", 11) == 0)
            ktype = SSH_KEYTYPE_ED25519;
        else if (strncmp(line, "ecdsa-sha2", 10) == 0)
            ktype = SSH_KEYTYPE_ECDSA_P256;

        ssh_key ref_key = NULL;
        if (ssh_pki_import_pubkey_base64(b64_start, ktype, &ref_key) == SSH_OK)
        {
            if (ssh_key_cmp(pubkey, ref_key, SSH_KEY_CMP_PUBLIC) == 0)
                found = TRUE;
            ssh_key_free(ref_key);
        }

        if (found) break;
    }

    Close(fh);
    return found ? SSH_AUTH_SUCCESS : SSH_AUTH_DENIED;
}

/* --- Password authentication callback --- */
static int auth_password_cb(ssh_session session, const char *user,
                            const char *password, void *userdata)
{
    (void)session; (void)userdata;

    if (strcmp(user, "root") != 0)
        return SSH_AUTH_DENIED;

    /* Password auth only if password file exists */
    BPTR fh = Open(SSHD_PASSWORD_FILE, MODE_OLDFILE);
    if (!fh)
        return SSH_AUTH_DENIED;

    char stored_hash[65] = {0};
    Read(fh, stored_hash, 64);
    Close(fh);
    stored_hash[64] = '\0';

    /* Strip any trailing whitespace */
    int len = strlen(stored_hash);
    while (len > 0 && (stored_hash[len-1] == '\n' || stored_hash[len-1] == '\r' || stored_hash[len-1] == ' '))
        stored_hash[--len] = '\0';

    /* Hash the provided password and compare */
    char computed_hash[65];
    sha256_hex(password, computed_hash);

    if (strcmp(computed_hash, stored_hash) == 0)
        return SSH_AUTH_SUCCESS;

    return SSH_AUTH_DENIED;
}

/* --- Shell session handler --- */
static void handle_shell(ssh_session session, ssh_channel channel)
{
    BPTR pipe_r, pipe_w;
    char pipe_name[64];

    /* Create a PIPE: pair for shell I/O */
    pipe_w = Open("PIPE:sshd_shell/32768", MODE_NEWFILE);
    if (!pipe_w)
    {
        ssh_channel_write(channel, "Error: cannot create shell pipe\r\n", 33);
        return;
    }

    if (NameFromFH(pipe_w, pipe_name, sizeof(pipe_name)))
        pipe_r = Open(pipe_name, MODE_OLDFILE);
    else
        pipe_r = BNULL;

    if (!pipe_r)
    {
        Close(pipe_w);
        ssh_channel_write(channel, "Error: cannot open shell pipe\r\n", 31);
        return;
    }

    /* Create a second pipe for shell output back to us */
    BPTR out_w = Open("PIPE:sshd_out/32768", MODE_NEWFILE);
    BPTR out_r = BNULL;
    char out_name[64];

    if (out_w && NameFromFH(out_w, out_name, sizeof(out_name)))
        out_r = Open(out_name, MODE_OLDFILE);

    if (!out_r)
    {
        if (out_w) Close(out_w);
        Close(pipe_r);
        Close(pipe_w);
        ssh_channel_write(channel, "Error: cannot create output pipe\r\n", 34);
        return;
    }

    /* Launch shell process with pipes as I/O */
    LONG shell_ret = SystemTags("",
        SYS_Input,  (IPTR)pipe_r,
        SYS_Output, (IPTR)out_w,
        SYS_Asynch, TRUE,
        SYS_UserShell, TRUE,
        NP_CloseInput, FALSE,
        NP_CloseOutput, FALSE,
        TAG_DONE);

    if (shell_ret == -1)
    {
        Close(pipe_r);
        Close(pipe_w);
        Close(out_r);
        Close(out_w);
        ssh_channel_write(channel, "Error: cannot start shell\r\n", 27);
        return;
    }

    /* Bridge loop: SSH channel <-> shell pipes */
    char buf[BUF_SIZE];
    int nbytes;

    while (ssh_channel_is_open(channel) && !ssh_channel_is_eof(channel))
    {
        /* Read from shell output (non-blocking) */
        if (WaitForChar(out_r, 50000)) /* 50ms */
        {
            LONG n = Read(out_r, buf, sizeof(buf));
            if (n > 0)
                ssh_channel_write(channel, buf, n);
            else if (n < 0)
                break;
        }

        /* Read from SSH channel (non-blocking) */
        nbytes = ssh_channel_read_nonblocking(channel, buf, sizeof(buf), 0);
        if (nbytes > 0)
        {
            /* Write to shell stdin */
            Write(pipe_w, buf, nbytes);
        }
        else if (nbytes == SSH_ERROR)
            break;

        /* Check for CTRL-C */
        if (SetSignal(0L, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
            break;
    }

    /* Cleanup: close our ends of the pipes */
    Close(pipe_w);
    Close(out_r);
}

/* --- Per-connection session handler --- */
static void handle_session(ssh_session session)
{
    struct ssh_server_callbacks_struct cb = {0};
    cb.auth_pubkey_function = auth_pubkey_cb;
    cb.auth_password_function = auth_password_cb;
    cb.userdata = NULL;

    ssh_callbacks_init(&cb);
    ssh_set_server_callbacks(session, &cb);
    ssh_set_auth_methods(session, SSH_AUTH_METHOD_PUBLICKEY | SSH_AUTH_METHOD_PASSWORD);

    if (ssh_handle_key_exchange(session) != SSH_OK)
    {
        Printf("sshd: key exchange failed: %s\n", (IPTR)ssh_get_error(session));
        goto done;
    }

    /* Authentication loop */
    ssh_event event = ssh_event_new();
    ssh_event_add_session(event, session);

    int n_auth = 0;
    while (!ssh_is_authenticated(session) && n_auth < 20)
    {
        if (ssh_event_dopoll(event, 1000) == SSH_ERROR)
            break;
        n_auth++;
    }

    ssh_event_free(event);

    if (!ssh_is_authenticated(session))
    {
        Printf("sshd: authentication failed\n");
        goto done;
    }

    /* Wait for channel open request */
    ssh_channel channel = NULL;
    ssh_message msg;
    int timeout = 30;

    while (timeout-- > 0)
    {
        msg = ssh_message_get(session);
        if (!msg) { Delay(25); continue; }

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

    /* Wait for shell/exec/pty request */
    BOOL got_shell = FALSE;
    const char *exec_cmd = NULL;
    timeout = 30;

    while (timeout-- > 0 && !got_shell)
    {
        msg = ssh_message_get(session);
        if (!msg) { Delay(25); continue; }

        if (ssh_message_type(msg) == SSH_REQUEST_CHANNEL)
        {
            int subtype = ssh_message_subtype(msg);
            switch (subtype)
            {
                case SSH_CHANNEL_REQUEST_PTY:
                    ssh_message_channel_request_reply_success(msg);
                    break;
                case SSH_CHANNEL_REQUEST_SHELL:
                    ssh_message_channel_request_reply_success(msg);
                    got_shell = TRUE;
                    break;
                case SSH_CHANNEL_REQUEST_EXEC:
                    exec_cmd = ssh_message_channel_request_command(msg);
                    ssh_message_channel_request_reply_success(msg);
                    got_shell = TRUE;
                    break;
                default:
                    ssh_message_reply_default(msg);
                    break;
            }
        }
        else
        {
            ssh_message_reply_default(msg);
        }
        ssh_message_free(msg);
    }

    if (!got_shell) goto cleanup_channel;

    if (exec_cmd)
    {
        /* Single command execution */
        BPTR out_w = Open("PIPE:sshd_exec/32768", MODE_NEWFILE);
        BPTR out_r = BNULL;
        char out_name[64];

        if (out_w && NameFromFH(out_w, out_name, sizeof(out_name)))
            out_r = Open(out_name, MODE_OLDFILE);

        if (out_r)
        {
            BPTR nil_in = Open("NIL:", MODE_OLDFILE);
            LONG rc = SystemTags(exec_cmd,
                SYS_Input,  (IPTR)nil_in,
                SYS_Output, (IPTR)out_w,
                NP_CloseInput, TRUE,
                NP_CloseOutput, TRUE,
                TAG_DONE);

            /* Read all output */
            char buf[BUF_SIZE];
            LONG n;
            while ((n = Read(out_r, buf, sizeof(buf))) > 0)
                ssh_channel_write(channel, buf, n);

            Close(out_r);
            ssh_channel_request_send_exit_status(channel, rc);
        }
        else
        {
            if (out_w) Close(out_w);
            ssh_channel_write(channel, "Error: cannot execute command\r\n", 30);
        }
    }
    else
    {
        /* Interactive shell */
        handle_shell(session, channel);
    }

cleanup_channel:
    if (channel)
    {
        ssh_channel_send_eof(channel);
        ssh_channel_close(channel);
        ssh_channel_free(channel);
    }

done:
    ssh_disconnect(session);
    ssh_free(session);
}

/* --- Child process entry point for concurrent sessions --- */
struct SessionMsg {
    struct Message msg;
    ssh_session    session;
};

static void session_proc_entry(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    struct MsgPort *port = &me->pr_MsgPort;

    WaitPort(port);
    struct SessionMsg *sm = (struct SessionMsg *)GetMsg(port);
    ssh_session session = sm->session;
    ReplyMsg(&sm->msg);

    handle_session(session);
}

static void spawn_session(ssh_session session)
{
    struct MsgPort *reply_port = CreateMsgPort();
    if (!reply_port)
    {
        handle_session(session);
        return;
    }

    struct Process *proc = CreateNewProcTags(
        NP_Entry,   (IPTR)session_proc_entry,
        NP_Name,    (IPTR)"sshd_session",
        NP_Priority, 0,
        NP_StackSize, 65536,
        TAG_DONE);

    if (!proc)
    {
        DeleteMsgPort(reply_port);
        handle_session(session);
        return;
    }

    /* Send session pointer to child via message */
    struct SessionMsg sm = {0};
    sm.msg.mn_ReplyPort = reply_port;
    sm.msg.mn_Length = sizeof(sm);
    sm.session = session;

    PutMsg(&((struct Process *)proc)->pr_MsgPort, &sm.msg);
    WaitPort(reply_port);
    GetMsg(reply_port);
    DeleteMsgPort(reply_port);
}

/* --- Main --- */
int main(int argc, char **argv)
{
    int port = SSHD_PORT;
    int i;

    /* Parse arguments */
    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc)
            port = atoi(argv[++i]);
    }

    SocketBase = OpenLibrary("bsdsocket.library", 4);
    if (!SocketBase)
    {
        Printf("sshd: Cannot open bsdsocket.library v4\n");
        return RETURN_FAIL;
    }

    /* Ensure host key exists */
    if (!ensure_host_key())
    {
        CloseLibrary(SocketBase);
        return RETURN_FAIL;
    }

    ssh_init();

    ssh_bind sshbind = ssh_bind_new();
    if (!sshbind)
    {
        Printf("sshd: Cannot create SSH bind\n");
        ssh_finalize();
        CloseLibrary(SocketBase);
        return RETURN_FAIL;
    }

    char port_str[8];
    sprintf(port_str, "%d", port);

    ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BINDPORT_STR, port_str);
    ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_HOSTKEY, SSHD_HOST_KEY);
    ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BANNER, SSHD_BANNER);

    if (ssh_bind_listen(sshbind) < 0)
    {
        Printf("sshd: Listen failed: %s\n", (IPTR)ssh_get_error(sshbind));
        ssh_bind_free(sshbind);
        ssh_finalize();
        CloseLibrary(SocketBase);
        return RETURN_FAIL;
    }

    Printf("sshd: " SSHD_BANNER " listening on port %ld\n", (long)port);
    Printf("sshd: Press CTRL-C to shutdown\n");

    /* Accept loop */
    while (!g_shutdown)
    {
        /* Check for CTRL-C */
        if (SetSignal(0L, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C)
        {
            Printf("sshd: Shutdown requested\n");
            break;
        }

        ssh_session session = ssh_new();
        if (!session) continue;

        if (ssh_bind_accept(sshbind, session) == SSH_OK)
        {
            spawn_session(session);
        }
        else
        {
            ssh_free(session);
        }
    }

    ssh_bind_free(sshbind);
    ssh_finalize();
    CloseLibrary(SocketBase);

    Printf("sshd: Stopped\n");
    return RETURN_OK;
}
