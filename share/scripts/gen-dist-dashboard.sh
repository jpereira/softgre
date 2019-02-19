#!/bin/bash
dashboard_dirs="fonts images scripts styles views"

dashboard_DATA="webinterface/dashboard/app/index.html \\
                webinterface/dashboard/app/favicon.ico"

for arg in ${dashboard_dirs}; do
    dir="webinterface/dashboard/app/$arg/"
    target="dashboard${arg}"

    echo "${target}dir = \$(pkgdatadir)/webinterface/$arg"
    echo "${target}_DATA = $(ls $dir | sed "s@^@$dir@g" | tr "\n" " ")" 
    echo

    dashboard_DATA="$dashboard_DATA \$(${target}_DATA)"
done

#echo "dashboarddir = \$(pkgdatadir)/webinterface"
#echo "dashboard_DATA = $dashboard_DATA"
#echo "dashboardfiles = $dashboard_DATA"
