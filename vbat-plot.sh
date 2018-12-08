#!/usr/bin/gnuplot
set term png nocrop font "arial,7" size 320,240 background rgb 'black'

set tmargin 1
set rmargin 1
set lmargin 5
set bmargin 3



#graph
set grid linecolor rgb '#454545'
set border 3 linecolor rgb 'white'
set format y '%.2fv'
set format x '%H:%M'
set xtics offset 0,0.25 rotate 
set ytics offset 1,0
set yrange [3.2:4.3]

#data
set datafile separator ";"
set style data histograms
set style line 1 linecolor rgb 'white' linetype 2 linewidth 1
set style line 2 linecolor rgb 'red' linetype 2 linewidth 1
set xdata time
set timefmt '%s'


set output '/dev/shm/vbat-plot.png'
plot '/dev/shm/vbat-start.log' using 1:2 smooth acsplines ls 1 notitle
