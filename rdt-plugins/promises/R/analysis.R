# requires dplyr, igraph, RSQLite

#install.packages(c("dplyr","igraph","RSQLite"))
#library(dplyr)
#library(igraph)

get_trace_strictness <- function(path="trace.sqlite")
    src_sqlite(path) %>% tbl("out_strictness")

get_trace_force_order <- function(path="trace.sqlite")
    src_sqlite(path) %>% tbl("out_force_order")

get_function_by_id <- function(function_id, path="trace.sqlite")
    src_sqlite(path) %>% tbl("functions") %>% filter(id == function_id)

get_function_aliases_by_id <- function(id, path="trace.sqlite")
    src_sqlite(path) %>% tbl("function_names") %>% filter(function_id == id)

# db <-

load_trace <- function(path="trace.sqlite") src_sqlite(path)

# Derive a call graph from a trace. This call graph agregates a concrete call tree by translating calls to function
# definitions.
call_graph_from_trace <- function(db) {
    # 1. make define edges for call graph
    cg <-
        # get data from database
        db %>% tbl("out_call_graph") %>%
        # select only the two columns we need
        select(caller_function, callee_function) %>%
        # flatten to an edge list -- this could be simply `apply(1, c) %>% c` if all nodes > 0, else convert to string
        as.data.frame %>% apply(1, function(x) c(toString(x[1]), toString(x[2]))) %>% c %>%
        # construct graph from edge list
        make_directed_graph

    # 2. fill in attributes: aliases
    V(cg)$label <-
        # get data from database
        (db %>% tbl("function_names") %>%
        # sort by id, so that it fits the data in the graph
        arrange(function_id) %>%
        # retrieve the column we're interested in
        select(names) %>%
        # convert data structure
        as.data.frame)$names

    # 3. fill in attributes: function types
    V(cg)$type <-
        # get data from database
        (db %>% tbl("functions") %>%
        # convert types to strings
        mutate(htype = if (type == 0) "closure" else
                       if (type == 1) "built-in" else
                       if (type == 2) "special" else
                       if (type == 3) "primitive" else NULL) %>%
        # sort by id, so that it fits the data in the graph
        arrange(id) %>%
        # retrieve the column we want
        select(htype) %>%
        # convert data structure
        as.data.frame)$htype

    # 4. color graph by function type
    V(cg)$color <- ifelse(V(cg)$type %in% c("closure", "built-in", NA), "green", "red")

    # 5. weight edges by colors
    E(cg)$weight <- get.edgelist(cg) %>% apply(1, function(edge) if (V(cg)[edge[2]]$color == "red") 0 else 1)

    # Finally, return graph
    cg
}

# Uses a call graph to classify each promise to be either forced locally, relayed, escaped, or not forced.
promise_lifestyle_from_cg <- function(db, cg) {
    promises <- db %>% tbl("promises")
    promise_evaluations <- db %>% tbl("promise_evaluations")
    function_dictionary <- db %>% tbl("calls") %>% select(id, function_id) %>% rename(call_id=id)

    classify <- function(created, forced)
        if (is.na(created) || is.na(forced))
            "virgin"
        else {
            distance <- distances(cg, v=created, to=forced, mode="out")[1]
        if (distance == Inf)
            "escaped"
        else if (distance == 0)
            "local"
        else #if (0 < distance < Inf)
            "vicarious"
        }

    # make an outer join to get evaluated and unevaluated promises
    left_join(promises, promise_evaluations, by=c("id" = "promise_id")) %>%
    # filter out lookups, leave NAs and forces
    filter(is.na(event_type) || event_type == 15) %>%
    # include function ids for in_call_id
    left_join(function_dictionary, by=c("in_call_id" = "call_id")) %>%
    rename(forced_function_id=function_id) %>%
    # include function ids for from_call_id
    left_join(function_dictionary, by=c("from_call_id" = "call_id")) %>%
    rename(created_function_id=function_id) %>%
    # only what we need: promise and function ids
    select(id, created_function_id, forced_function_id) %>%
    # strings!
    mutate(created_function_id=as.character(created_function_id),
           forced_function_id=as.character(forced_function_id)) %>%
    group_by(id) %>% do(mutate(., locality=classify(created_function_id, forced_function_id)))
}

# Derive a concrete call tree from a trace. This is a tree built on actual calls (not functions).
concrete_call_tree_from_trace <- function(db, debug=TRUE) {
    if (debug) write('[make cct] start', stderr())
    start.time <- Sys.time()

    calls <- db %>% tbl("calls")
    functions <- db %>% tbl("functions")
    all_call_ids <- {
        call_ids <- calls %>% select(id)
        parent_ids <- calls %>% select(parent_id) %>% rename(id=parent_id)
        union_all(call_ids, parent_ids)
    } %>% distinct(id) %>% arrange(id)

    # 1. define edges for call tree
    if (debug) write('[make cct] edges', stderr())
    edges <-
        calls %>% mutate(id.s = as.character(id), parent_id.s = as.character(parent_id)) %>%
        select(parent_id.s, id.s) %>% rename(parent_id=parent_id.s, id=id.s) %>%
        as.data.frame %>% apply(1, c) %>% c

    # 2. Prepare a list of vertices
    if (debug) write('[make cct] vertices', stderr())
    vertices <-
        edges %>% unique

    # 3. Prepare labels for vertices using function names
    if (debug) write('[make cct] labels', stderr())
    labels <-
        (left_join(all_call_ids, calls, by="id") %>% arrange(id) %>%
        select(function_name) %>%
        as.data.frame)$function_name

    # 4. Designate types and color vertices
    if (debug) write('[make cct] types and colors', stderr())
    vertex.info <-
        left_join(all_call_ids, calls, by="id") %>%
        left_join(functions %>% rename(function_id=id), by="function_id") %>%

        mutate(human_readable_type =
                       if (type == 0) "closure" else
                       if (type == 1) "built-in" else
                       if (type == 2) "special" else
                       if (type == 3) "primitive" else NULL) %>%

        mutate(color = if (type == 0 || type == 2) "green" else
                       if (type == 1 || type == 3) "red" else NULL) %>%

        as.data.frame

    # 5. Weigh edges based on types
    if (debug) write('[make cct] edge weights', stderr())
    weights <-
        edges %>% matrix(ncol=2, byrow=TRUE) %>%
        as.data.frame %>% mutate(id=as.integer(V2)) %>%
        left_join(vertex.info, by="id") %>%
        mutate(weight = ifelse(color == "green", 1, 0))

    # 6. Put the graph together
    if (debug) write('[make cct] create graph', stderr())
    vertex.attributes <- list(name = vertices, label = labels, type = vertex.info$human_readable_types, color = vertex.info$color)
    edge.attributes <- (weight = weights)

    cct <- graph.empty(n = 0, directed = T)
    cct <- add.vertices(cct, length(vertex.attributes$name), attr = vertex.attributes)
    cct <- add.edges(cct, edges, attr = edge.attributes)

    end.time <- Sys.time()
    if(debug) write(paste("[make cct] done, elapsed time:", end.time - start.time), stderr())

    # Finally, return graph
    cct
}

# Uses a concrete call tree to classify each promise to be either forced locally, relayed, escaped, or not forced.
promise_lifestyle_from_cct <- function(db, cct, debug=TRUE) {
    if (debug) write('[lifestyles] start', stderr())
    start.time <- Sys.time()

    promises <- db %>% tbl("promises")
    promise_evaluations <- db %>% tbl("promise_evaluations")

    classify <- function(created, forced) {
        if (is.na(created) || is.na(forced))
            "virgin"
        else {
            distance <- distances(cct, v=forced, to=created, mode="in")[1]
            if (distance == Inf)
                "escaped"
            else if (distance == 0)
                "local"
            else #if (0 < distance < Inf)
                "vicarious"
        }
    }

    if (debug) write('[lifestyles] retrieve promise creation/force', stderr())
    promises <- left_join(promises, promise_evaluations, by=c("id" = "promise_id")) %>%
        filter(is.na(event_type) || event_type == 15) %>%
        rename(created_call_id=in_call_id) %>%
        rename(forced_call_id=from_call_id) %>%
        select(id, created_call_id, forced_call_id) %>%
        mutate(created_call_id=as.character(created_call_id),
               forced_call_id=as.character(forced_call_id)) %>%
        as.data.frame

    if (debug) write('[lifestyles] classify promises', stderr())
    lifestyles <-
        promises %>%

    lifestyles <-
        promises %>% group_by(id) %>% do(mutate(., locality=classify(created_call_id, forced_call_id)))

    end.time <- Sys.time()
    if (debug) write(paste("[lifestyles] elapsed time:", end.time - start.time), stderr())

    lifestyles
}

print_graph <- function(g, file="graph", size=20, labels=NULL, layout=layout.reingold.tilford(g, root="0")) {
    color_function <- function(color) {
        ifelse(color == "red",   "#FFAAAA",
        ifelse(color == "green", "#A5C663",
                                 "#669999"))
    }

    pdf(paste(file, ".pdf", sep=""), height=size, width=size)
    lo <- layout #layout.auto(g)
    plot(0, type="n", ann=FALSE, axes=FALSE, xlim=extendrange(lo[,1]), ylim=extendrange(lo[,2]))
    plot.igraph(g, layout=lo, add=TRUE, rescale=FALSE,
                   edge.arrow.size = .75, edge.curved = seq(-0.3, 0.3, length = ecount(g)),
                   vertex.label=labels,
                   vertex.shape = "rectangle",
                   vertex.color = color_function(V(g)$color),
                   vertex.frame.color = color_function(V(g)$color),
                   vertex.size = (strwidth(V(g)$label) + strwidth("oo")) * 100,
                   vertex.size2 = strheight("I") * 2 * 100)
    dev.off()
}

how_many_promises_are_evaluated_in_the_same_function_in_which_they_are_created <- function() {
    db <- src_sqlite(path)
    cct <- get_trace_concrete_call_tree(db)
    pl <- get_trace_promise_lifespan_for_concrete_call_tree()
    (pl %>% count(classification == "local") %>% as.data.frame)$n
}

where_are_promises_evaluated_cg <- function(path="trace.sqlite") {
    db <- load_trace(path)
    cg <- call_graph_from_trace(db)
    cg.ls <- promise_lifestyle_from_cg(db,cg)
    cg.ls %>% group_by(locality) %>% count(locality)
}

where_are_promises_evaluated_cct <- function(path="trace.sqlite") {
    db <- load_trace(path)
    cct <- concrete_call_tree_from_trace(db)
    cct.ls <- promise_lifestyle_from_cg(db,cct)
    cct.ls %>% group_by(locality) %>% count(locality)
}

what_type_of_promises_are_there <- function(path="trace.sqlite") {

    namer <- function(type) {
             if(type == 0) "NIL"
        else if(type == 1) "SYM"
        else if(type == 2) "LIST"
        else if(type == 3) "CLOS"
        else if(type == 4) "ENV"
        else if(type == 5) "PROM"
        else if(type == 6) "LANG"
        else if(type == 7) "SPECIAL"
        else if(type == 8) "BUILTIN"
        else if(type == 9) "CHAR"
        else if(type == 10) "LGL"
        else if(type == 13) "INT"
        else if(type == 14) "REAL"
        else if(type == 15) "CPLX"
        else if(type == 16) "STR"
        else if(type == 17) "DOT"
        else if(type == 18) "ANY"
        else if(type == 19) "VEC"
        else if(type == 20) "EXPR"
        else if(type == 21) "BCODE"
        else if(type == 22) "EXTRPTR"
        else if(type == 23) "WEAKREF"
        else if(type == 24) "RAW"
        else if(type == 25) "S4"
        else                 NULL
    }

    trivial <- c(0, 1, 9, 10, 13 , 14, 15, 16, 17, 19, 24)

    db %>% tbl("promises") %>%
    group_by(type) %>% count %>% group_by(type) %>%
    do(mutate(., htype=namer(type), classification=ifelse(type %in% trivial, "trivial", "non-trivial"))) %>%
    arrange(n)
}

how_many_promises_are_trivial <- function(path="trace.sqlite") {
    what_type_of_promises_are_there(path) %>% group_by(classification) %>% summarise(count=sum(n))
}

#inner_join(cg.ls %>% rename(locality.cg=locality,creat.cg=created_function_id,force.cg=forced_function_id), cct.ls %>% rename(locality.cct=locality,creat.cct=created_function_id,force.cct=forced_function_id), by="id") %>% as.data.frame #%>% select (id, created_call_id, forced_call_id, locality.cg, locality.cct)

