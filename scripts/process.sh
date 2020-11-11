#!/bin/sh

path=/mnt/c/Users/t-makog/Code/verona-io/results
for f in `ls $path/*.txt`; do
	fname=`basename $f`
	results=$path/processed/$fname
	echo "#QPS\tAvg\t50th\t90th\t95th\t99th and confidence" > $results
	grep -A 1 "QPS" $f | awk 'NR % 3 == 2 {print $2}' > /tmp/kogias/lancet1
	grep -A 2  Aggregate $f | awk 'NR % 4 == 3 {print $0}' | sed  's/\((\|)\)/ /g' | sed 's/,//g' > /tmp/kogias/lancet2
	paste /tmp/kogias/lancet1 /tmp/kogias/lancet2 >> $results
done
