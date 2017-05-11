packages <- c(
"dplyr",
"ggplot2",
"haven",
"readr",
"readxl",
"stringr",
"tibble",
"tidyverse",
"grid",
"digest",
"colorspace",
"kernlab",
"vcd",
"rpart",
"survival",
"mclust",
"party",
"mvtnorm",
"igraph")

wrap.path.executor <- function(executor, path)
    function(expr, current_vignette, total_vignettes, vignette_name, vignette_package, ...) {
        write(paste("Vignette ", (current_vignette + 1), "/", total_vignettes,
                        " (", vignette_name, " from ", vignette_package, "), saving to '", path, "'", sep=""),
            stderr())

    executor(eval(expr), path=path, overwrite=if (current_vignette == 0) TRUE else FALSE, ...)
}

start.time <- Sys.time()
for (p in packages)	
	run.all.vignettes.from.package(p, 
		wrap.path.executor(trace.promises.db, path=paste("/data/kondziu/", p, ".sqlite", sep="")))

end.time <- Sys.time()
time.taken <- end.time - start.time
print (paste("execution time:", time.taken))
