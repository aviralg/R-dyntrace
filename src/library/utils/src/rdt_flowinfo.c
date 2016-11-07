#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <rdtrace.h>

#include "rdt_utils.h"

#define INDENT_LEVEL 2

static FILE *output = NULL;
static uint64_t last = 0;
static uint64_t delta = 0;
static unsigned int depth = 0;

static void print(const char *type, const char *fmt, ...) {
    char *entry = NULL;
    va_list ap;

    va_start(ap, fmt);
    if (!vasprintf(&entry, fmt, ap)) {
        perror("vasprintf()");
        return;
    }
    va_end(ap);

	fprintf(output, 
            "%12"PRId64" %-11s %*s%s\n", 
            delta,
            type, 
            depth * INDENT_LEVEL, "", 
            entry);

    free(entry);
}

static void compute_delta() {
    delta = (timestamp() - last) / 1000;
}

void flowinfo_begin() {
    fprintf(output, "%12s %-11s -- %s\n", "DELTA(us)", "TYPE", "NAME");
    fflush(output);

    last = timestamp();
}

void flowinfo_function_entry(const SEXP call, const SEXP op, const SEXP rho) {
    compute_delta();

    const char *type = is_byte_compiled(call) ? "bc-function" : "function";
    const char *name = get_name(call);
    const char *ns = get_ns_name(op);
    char *loc = get_location(op);

    print(type, "-> %s::%s (loc: %s)", CHKSTR(ns), CHKSTR(name), CHKSTR(loc));

    if (loc) free(loc);
	
    depth++;
    last = timestamp();
}

void flowinfo_function_exit(const SEXP call, const SEXP op, const SEXP rho, const SEXP retval) {
    compute_delta();
    depth -= depth > 0 ? 1 : 0;
    
    const char *type = is_byte_compiled(call) ? "bc-function" : "function";
    const char *name = get_name(call);
    const char *ns = get_ns_name(op);
    char *loc = get_location(op);
    char *result = to_string(retval);
    
    print(type, "<- %s::%s = `%s` (%d) (loc: %s)", CHKSTR(ns), CHKSTR(name), result, TYPEOF(retval), CHKSTR(loc));

    if (loc) free(loc);
    free(result);

    last = timestamp();
}

void flowinfo_builtin_entry(const SEXP call) {
    compute_delta();

    const char *name = get_name(call);

    print("builtin", "-> %s", name);

	depth++;
    last = timestamp();
}

void flowinfo_builtin_exit(const SEXP call, const SEXP retval) {
    compute_delta();
    depth -= depth > 0 ? 1 : 0;

    const char *name = get_name(call);
    char *result = to_string(retval);
    
    print("builtin", "<- %s = `%s` (%d)", CHKSTR(name), result, TYPEOF(retval));

    free(result);

    last = timestamp();
}

void flowinfo_force_promise_entry(const SEXP symbol) {
    compute_delta();

    const char *name = get_name(symbol);
    
    print("promise", "-> %s", CHKSTR(name));

	depth++;
    last = timestamp();
}

void flowinfo_force_promise_exit(const SEXP symbol, const SEXP val) {
    compute_delta();
    depth -= depth > 0 ? 1 : 0;

    const char *name = get_name(symbol);
    char *result = to_string(val);
    
    print("promise", "<- %s = `%s` (%d)", CHKSTR(name), result, TYPEOF(val));

    free(result);

    last = timestamp();
}

void flowinfo_promise_lookup(const SEXP symbol, const SEXP val) {
    compute_delta();

    const char *name = get_name(symbol);
    char *result = to_string(val);
    
    print("lookup", "-- %s = `%s` (%d)", CHKSTR(name), result, TYPEOF(val));

    free(result);
    
    last = timestamp();
}

void flowinfo_error(const SEXP call, const char* message) {
    compute_delta();

    const char *name = get_call(call);
    char *loc = get_location(call);
    
    print("lookup", "-- error in %s : %s (loc: %s)", CHKSTR(name), message, CHKSTR(loc));

    if (loc) free(loc);
    
    depth = 0;
    last = timestamp();
}

static const rdt_handler flowinfo_rdt_handler = {
    &flowinfo_begin,
    NULL,
    &flowinfo_function_entry,
    &flowinfo_function_exit,
    &flowinfo_builtin_entry,
    &flowinfo_builtin_exit,
    &flowinfo_force_promise_entry,
    &flowinfo_force_promise_exit,
    &flowinfo_promise_lookup,
    &flowinfo_error,
    NULL, // probe_vector_alloc
    NULL, // probe_eval_entry
    NULL  // probe_eval_exit        
};

static int flowinfo_setup_tracing(SEXP options) {
    SEXP fname_sexp = get_named_list_element(options, "filename");
    if (fname_sexp == R_NilValue || TYPEOF(fname_sexp) != STRSXP) {
        error("Required argument `filename` is missing or is not string (%d)", TYPEOF(fname_sexp));
        return 0; 
    }

    const char *fname = CHAR(asChar(fname_sexp));
    output = fopen(fname, "wt");
    if (!output) {
        error("Unable to open %s: %s\n", fname, strerror(errno));
        return 0;
    }

    return 1; 
}

static void flowinfo_teardown_tracing() {
    fclose(output);
}

static int running = 0;

SEXP RdtFlowInfo(SEXP options) {
    if (running) {
        flowinfo_teardown_tracing();
        rdt_stop(&flowinfo_rdt_handler);
        running = 0;
    } else {
        if (flowinfo_setup_tracing(options)) { 
            rdt_start(&flowinfo_rdt_handler);
            running = 1;
        } else {
            error("Unable to initialize flowinfo tracing");
        }
    }

    return ScalarLogical(running);
}
