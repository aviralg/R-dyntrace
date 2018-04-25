#include <Rdyntrace.h>
#include <Rinternals.h>
#include <deparse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

dyntracer_t *dyntrace_active_dyntracer = NULL;
int dyntrace_check_reentrancy = 1;
const char *dyntrace_active_dyntracer_probe_name = NULL;
int dyntrace_garbage_collector_state = 0;
int dyntrace_privileged_mode_flag = 0;

dyntracer_t *dyntracer_from_sexp(SEXP dyntracer_sexp) {
    return (dyntracer_t *)R_ExternalPtrAddr(dyntracer_sexp);
}

SEXP dyntracer_to_sexp(dyntracer_t *dyntracer, const char *classname) {
    SEXP dyntracer_sexp;
    PROTECT(dyntracer_sexp =
                R_MakeExternalPtr(dyntracer, install(classname), R_NilValue));
    SEXP dyntracer_class = PROTECT(allocVector(STRSXP, 1));
    SET_STRING_ELT(dyntracer_class, 0, mkChar(classname));
    classgets(dyntracer_sexp, dyntracer_class);
    UNPROTECT(2);
    return dyntracer_sexp;
}

dyntracer_t *dyntracer_replace_sexp(SEXP dyntracer_sexp,
                                    dyntracer_t *new_dyntracer) {
    dyntracer_t *old_dyntracer = R_ExternalPtrAddr(dyntracer_sexp);
    R_SetExternalPtrAddr(dyntracer_sexp, new_dyntracer);
    return old_dyntracer;
}

SEXP dyntracer_destroy_sexp(SEXP dyntracer_sexp,
                            void (*destroy_dyntracer)(dyntracer_t *dyntracer)) {
    dyntracer_t *dyntracer = dyntracer_replace_sexp(dyntracer_sexp, NULL);
    /* free dyntracer iff it has not already been freed.
       this check ensures that multiple calls to destroy_dyntracer on the same
       object do not crash the process. */
    if (dyntracer != NULL)
        destroy_dyntracer(dyntracer);
    return R_NilValue;
}

void dyntrace_enable_privileged_mode() { dyntrace_privileged_mode_flag = 1; }

void dyntrace_disable_privileged_mode() { dyntrace_privileged_mode_flag = 0; }

int dyntrace_is_priviliged_mode() { return dyntrace_privileged_mode_flag; }

SEXP do_dyntrace(SEXP call, SEXP op, SEXP args, SEXP rho) {
    int eval_error = FALSE;
    SEXP expression, environment, result;
    dyntracer_t *dyntracer = NULL;
    dyntracer_t *dyntrace_previous_dyntracer = dyntrace_active_dyntracer;

    /* extract objects from argument list */
    dyntracer = dyntracer_from_sexp(eval(CAR(args), rho));
    PROTECT(expression = PRCODE(findVar(CADR(args), rho)));
    PROTECT(environment = eval(CADDR(args), rho));

    if (dyntracer == NULL)
        error("dyntracer is NULL");

    dyntrace_active_dyntracer = dyntracer;

    DYNTRACE_PROBE_DYNTRACE_ENTRY(expression, environment);

    PROTECT(result = R_tryEval(expression, environment, &eval_error));

    /* we want to return a sensible result for
       evaluation of the expression */
    if (eval_error) {
        result = R_NilValue;
    }

    DYNTRACE_PROBE_DYNTRACE_EXIT(expression, environment, result, eval_error);

    dyntrace_active_dyntracer = dyntrace_previous_dyntracer;

    UNPROTECT(3);
    return result;
}

//-----------------------------------------------------------------------------
// helpers
//-----------------------------------------------------------------------------

int dyntrace_is_active() {
    return (dyntrace_active_dyntracer_probe_name != NULL);
}

void dyntrace_disable_garbage_collector() {
    dyntrace_garbage_collector_state = R_GCEnabled;
    R_GCEnabled = 0;
}

void dyntrace_reinstate_garbage_collector() {
    R_GCEnabled = dyntrace_garbage_collector_state;
}

#define DYNTRACE_PARSE_DATA_BUFFER_SIZE (1024 * 1024 * 8)
static char data[DYNTRACE_PARSE_DATA_BUFFER_SIZE];
static LocalParseData dyntrace_parse_data = {
    0,
    0,
    0,
    0,
    /*startline = */ TRUE,
    0,
    NULL,
    /*DeparseBuffer=*/{data, 0, DYNTRACE_PARSE_DATA_BUFFER_SIZE},
    INT_MAX,
    FALSE,
    0,
    TRUE,
#ifdef longstring_WARN
    FALSE,
#endif
    INT_MAX,
    TRUE,
    0,
    FALSE};

const char *serialize_sexp(SEXP s, int opts) {
    dyntrace_parse_data.buffer.data[0] = '\0';
    dyntrace_parse_data.buffer.bufsize = 0;
    dyntrace_parse_data.linenumber = 0;
    dyntrace_parse_data.indent = 0;
    dyntrace_parse_data.opts = opts;
    deparse2buff(s, &dyntrace_parse_data);
    return dyntrace_parse_data.buffer.data;
}

int newhashpjw(const char *s) { return R_Newhashpjw(s); }
