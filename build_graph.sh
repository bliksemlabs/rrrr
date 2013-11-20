#!/bin/bash
if [ "$#" = "0" ]; then
	echo "-s path_to_GTFS source file data"
	echo "-t build target"
else
	if [ "$1" = "-s" ]; then
	       	src=$2;   
	else 
		src="../GTFS/GTFS.zip" 
	fi

	if [ "$3" = "-t" ]; then 
		target=$4 
	else 
		target="built.data" 
	fi
	
	echo "$(tput bold)$(tput setaf 2)---------Building data---------$(tput sgr0)"
	python gtfsdb.py $src $target; 
	echo "$(tput bold)$(tput setaf 2)---------Recalculating transfers---------$(tput sgr0)"
	python transfers.py $target;
	echo "$(tput bold)$(tput setaf 2)---------Adjusting timetables---------$(tput sgr0)"
	python timetable.py $target;
	echo "$(tput bold)$(tput setaf 2)---------COMPLETE!---------$(tput sgr0)"
fi
