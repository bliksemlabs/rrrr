#include "craptor.h"

int main(int argc, char **argv) {

    /* Logging */
    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog(PROGRAM_NAME, LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER);

    syslog(LOG_INFO, "starting up");

    /* Deamonize */
    // daemonize("/tmp/", "/tmp/daemon.pid");

    FCGX_Stream *in, *out, *err;
    FCGX_ParamArray envp;
    int count = 0;
    while(FCGX_Accept(&in, &out, &err, &envp) >= 0) {
        FCGX_FPrintF(out,
               "Content-type: text/html\r\n"
               "\r\n"
               "<title>FastCGI Hello! (C, fcgiapp library)</title>"
               "<h1>FastCGI Hello! (C, fcgiapp library)</h1>"
               "Request number %d running on host <i>%s</i>  "
               "Process ID: %d<br>\n",
	               ++count, FCGX_GetParam("HTTP_HOST", envp), getpid());
        int i = 0;
        while (envp[i] != NULL) {
            FCGX_FPrintF(out, "%s<br>\n", envp[i]);
            i++;
        }
        char *qstring = FCGX_GetParam("QUERY_STRING", envp);
        if (qstring != NULL)
            parse_query_params(qstring, out);
    }
        
    return EXIT_SUCCESS;
}
