//
// Created by nohajc on 27.3.17.
//

#include "tracer_sexpinfo.h"
//#include "tracer_output.h"
#include "tracer_state.h"
#include <iostream>

#include <rdt.h>
#include <sstream>

rid_t get_sexp_address(SEXP e) {
    return (rid_t)e;
}

prom_id_t get_promise_id(SEXP promise) {
    if (promise == R_NilValue)
        return RID_INVALID;
    if (TYPEOF(promise) != PROMSXP)
        return RID_INVALID;

    // A new promise is always created for each argument.
    // Even if the argument is already a promise passed from the caller, it gets re-wrapped.
    prom_addr_t prom_addr = get_sexp_address(promise);
    prom_id_t prom_id;

    unsigned int prom_type = TYPEOF(PRCODE(promise));
    unsigned int orig_type = (prom_type == 21) ? TYPEOF(BODY_EXPR(PRCODE(promise))) : 0;
    prom_key_t key(prom_addr, prom_type, orig_type);

    auto & promise_ids = STATE(promise_ids);
    auto it = promise_ids.find(key);
    if (it != promise_ids.end()){
        prom_id = it->second;
    }
    else {
        prom_id = make_promise_id(promise, true);
    }

    return prom_id;
}

prom_id_t make_promise_id(SEXP promise, bool negative) {
    if (promise == R_NilValue)
        return RID_INVALID;

    prom_addr_t prom_addr = get_sexp_address(promise);
    prom_id_t prom_id;

    if (negative) {
        prom_id = --STATE(prom_neg_id_counter);
    }
    else {
        prom_id = STATE(prom_id_counter)++;
    }
    unsigned int prom_type = TYPEOF(PRCODE(promise));
    unsigned int orig_type = (prom_type == 21) ? TYPEOF(BODY_EXPR(PRCODE(promise))) : 0;
    prom_key_t key(prom_addr, prom_type, orig_type);
    STATE(promise_ids)[key] = prom_id;

    auto & already_inserted_negative_promises = STATE(already_inserted_negative_promises);
    already_inserted_negative_promises.insert(prom_id);

    return prom_id;
}

fn_id_t get_function_id(SEXP func) {
    const char * def = get_expression(func);
    assert(def != NULL);
    fn_key_t definition = def;

    auto & function_ids = STATE(function_ids);
    auto it = function_ids.find(definition);

    if (it != function_ids.end()) {
        return it->second;
    } else {
        fn_id_t fn_id = STATE(fn_id_counter)++;
        STATE(function_ids)[definition] = fn_id;
        return fn_id;
    }
}

bool register_inserted_function(fn_id_t id) {
    auto & already_inserted_functions = STATE(already_inserted_functions);
//    bool exists = already_inserted_functions.count(id) > 0;
//    if (exists)
//        return false;
    auto result = already_inserted_functions.insert(id);
    return result.second;
//        return true
}

bool negative_promise_already_inserted(prom_id_t id) {
    auto & already_inserted = STATE(already_inserted_negative_promises);
    return already_inserted.count(id) > 0;
}

bool function_already_inserted(fn_id_t id) {
    auto & already_inserted_functions = STATE(already_inserted_functions);
    return already_inserted_functions.count(id) > 0;
}

fn_addr_t get_function_addr(SEXP func) {
    return get_sexp_address(func);
}

call_id_t make_funcall_id(SEXP function) {
    if (function == R_NilValue)
        return RID_INVALID;

    return ++STATE(call_id_counter);
}

// XXX This is a remnant of the RDT_CALL_ID format
//call_id_t make_funcall_id(SEXP fn_env) {
//    assert(fn_env != NULL);
//    return get_sexp_address(fn_env);
//}

// Wraper for findVar. Does not look up the value if it already is PROMSXP.
SEXP get_promise(SEXP var, SEXP rho) {
    SEXP prom = R_NilValue;

    if (TYPEOF(var) == PROMSXP) {
        prom = var;
    } else if (TYPEOF(var) == SYMSXP) {
        prom = findVar(var, rho);
    }

    return prom;
}

arg_id_t get_argument_id(fn_id_t function_id, const string & argument) {
    arg_key_t key = make_pair(function_id, argument);
    auto iterator = STATE(argument_ids).find(key);

    if (iterator != STATE(argument_ids).end()) {
        return iterator->second;
    }

    arg_id_t argument_id = ++STATE(argument_id_sequence);
    STATE(argument_ids)[key] = argument_id;
    return argument_id;
}



arglist_t get_arguments(SEXP op, SEXP rho) {
    arglist_t arguments;

    for (SEXP formals = FORMALS(op); formals != R_NilValue; formals = CDR(formals)) {
        // Retrieve the argument name.
        SEXP argument_expression = TAG(formals);
        SEXP promise_expression = get_promise(argument_expression, rho);

        if (TYPEOF(promise_expression) == DOTSXP) {
            int i = 0;
            for(SEXP dots = promise_expression; dots != R_NilValue; dots = CDR(dots)) {
                SEXP ddd_argument_expression = TAG(dots);
                SEXP ddd_promise_expression = CAR(dots);
                if (ddd_argument_expression == R_NilValue) {
                    arguments.push_back(std::make_tuple(
                            get_argument_id(get_function_id(op), to_string(i++)),
                            get_promise_id(ddd_promise_expression)
                    )); // ... argument without a name
                }
                else {
                    string ddd_arg_name = get_name(ddd_argument_expression);
                    arguments.push_back(std::make_tuple(
                            ddd_arg_name,
                            get_argument_id(get_function_id(op), ddd_arg_name),
                            get_promise_id(ddd_promise_expression)
                    ), true); // this flag says we're inserting a ... argument
                }
            }
        }
        else {
            // Retrieve the promise for the argument.
            // The call SEXP only contains AST to find the actual argument value, we need to search the environment.
            string arg_name = get_name(argument_expression);
            prom_id_t prom_id = get_promise_id(promise_expression);
            if (prom_id != RID_INVALID)
                arguments.push_back(std::make_tuple(
                        arg_name,
                        get_argument_id(get_function_id(op), arg_name),
                        prom_id
                ));
        }

    }

    return arguments;
}

string sexp_type_to_string(sexp_type s) {
    switch (s) {
        case sexp_type::NIL: return "null";
        case sexp_type::SYM: return "symbol";
        case sexp_type::LIST: return "list";
        case sexp_type::CLOS: return "closure";
        case sexp_type::ENV: return "environment";
        case sexp_type::PROM: return "promise";
        case sexp_type::LANG: return "language";
        case sexp_type::SPECIAL: return "special";
        case sexp_type::BUILTIN: return "built-in";
        case sexp_type::CHAR: return "character";
        case sexp_type::LGL: return "logical";
        case sexp_type::INT: return "integer";
        case sexp_type::REAL: return "real";
        case sexp_type::CPLX: return "complex";
        case sexp_type::STR: return "string";
        case sexp_type::DOT: return "dot";
        case sexp_type::ANY: return "any";
        case sexp_type::VEC: return "vector";
        case sexp_type::EXPR: return "expression";
        case sexp_type::BCODE: return "byte-code";
        case sexp_type::EXTPTR: return "external_pointer";
        case sexp_type::WEAKREF: return "weak_reference";
        case sexp_type::RAW: return "raw";
        case sexp_type::S4: return "s4";
        default: return "<unknown>";
     }
}

string of_call_info(call_info_t info){
    stringstream s;
    s << "fn_type=" << tools::enum_cast(info.fn_type);
    s << " fn_id=" << info.fn_id;
    s << " fn_addr=" << info.fn_addr;
    s << " fn_definition=" << info.fn_definition;
    s << " loc_=" << info.loc;
    s << " callsite_=" << info.callsite;
    s << " fn_compiled_=" << info.fn_compiled;
    s << " name=" << info.name;
    s << " call_id=" << info.call_id;
    s << " call_ptr=" << info.call_ptr;
    s << " parent_call_id=" << info.parent_call_id;
    return s.str();
}

string of_closure_info(closure_info_t info){
    stringstream s;
    s << "fn_type=" << tools::enum_cast(info.fn_type);
    s << " fn_id=" << info.fn_id;
    s << " fn_addr=" << info.fn_addr;
    s << " fn_definition=" << info.fn_definition;
    s << " loc_=" << info.loc;
    s << " callsite_=" << info.callsite;
    s << " fn_compiled_=" << info.fn_compiled;
    s << " name=" << info.name;
    s << " call_id=" << info.call_id;
    s << " call_ptr=" << info.call_ptr;
    s << " parent_call_id=" << info.parent_call_id;

    s << " arguments=";
    bool first = true;
    for (auto arg_ref : info.arguments.all()) {
        const arg_t & argument = arg_ref.get();

        if (first)
            first = false;
        else
            s << ",";

        s << "{" << get<0>(argument) << "," << get<1>(argument) << "}";
    }
    return s.str();
}

string of_builtin_info(builtin_info_t info){
    stringstream s;
    s << "fn_type=" << tools::enum_cast(info.fn_type);
    s << " fn_id=" << info.fn_id;
    s << " fn_addr=" << info.fn_addr;
    s << " fn_definition=" << info.fn_definition;
    s << " loc_=" << info.loc;
    s << " callsite_=" << info.callsite;
    s << " fn_compiled_=" << info.fn_compiled;
    s << " name=" << info.name;
    s << " call_id=" << info.call_id;
    s << " call_ptr=" << info.call_ptr;
    s << " parent_call_id=" << info.parent_call_id;
    return s.str();
}

//struct prom_basic_info_t {
//    prom_id_t     prom_id;
//    sexp_type     prom_type;
//    sexp_type     prom_original_type; // if prom_type is BCODE, then this points what the BCODESXP was compiled from
//};
//
//struct prom_info_t : prom_basic_info_t {
//    string          name;
//    call_id_t       in_call_id;
//    call_id_t       from_call_id;
//    lifestyle_type  lifestyle;
//    int             effective_distance_from_origin;
//    int             actual_distance_from_origin;
//};

string of_prom_info(prom_info_t info){
    stringstream s;
    s << "prom_id=" << info.prom_id;
    s << " prom_type=" << tools::enum_cast(info.prom_type);
    s << " prom_original_type=" << tools::enum_cast(info.prom_original_type);
    s << " name=" << info.name;
    s << " in_call_id=" << info.in_call_id;
    s << " from_call_id=" << info.from_call_id;
    s << " lifestyle=" << tools::enum_cast(info.lifestyle);
    s << " effective_distance=" << info.effective_distance_from_origin;
    s << " actual_distance=" << info.actual_distance_from_origin;
    return s.str();
}

string of_prom_basic_info(prom_basic_info_t info){
    stringstream s;
    s << "prom_id=" << info.prom_id;
    s << " prom_type=" << tools::enum_cast(info.prom_type);
    s << " prom_original_type=" << tools::enum_cast(info.prom_original_type);
    return s.str();
}