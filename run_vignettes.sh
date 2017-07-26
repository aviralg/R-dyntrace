#!/bin/bash

#CMD='bin/Rscript rdt-plugins/promises/R/benchmark.R'
#CMD='bin/Rscript compose_testable_vignettes.R'
CMD='bin/R --slave --no-restore --file=compose_testable_vignettes.R --args'

export R_ENABLE_JIT=0
export KEEP_PKG_SOURCE=yes

PACKAGES=

if [ $# -ge 1 ]
then
    PACKAGES="$@"
else
    #PACKAGES="vcd rpart survival mclust party mvtnorm igraph"
    #ALL_PACKAGES="grid ggplot2 haven readr readxl stringr tibble tidyverse digest colorspace kernlab vcd rpart survival mclust party mvtnorm igraph dplyr"
    PACKAGES=`cat r-package-anthology.csv | grep -v '^#' | grep -v '^$' | tr -s ' ' | tail -n +2 | cut -f 2 -d';' | xargs echo`
fi

echo > packages_done

for i in $PACKAGES
do 
    echo "$CMD $i"
    #valgrind --tool=memcheck --leak-check=full --show-reachable=yes \
    #rr record \
    $CMD $i 2>&1 | tee "$i.log" 
    echo "$i" >> packages_done
done   

