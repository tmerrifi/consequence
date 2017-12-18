threads=$1;

cd /local_home/tmerrifi/conversion/eval/logging;
make clean &> /dev/null;
make &> /dev/null;
#USAGE: ./logging <number_of_threads> <size_of_array_in_pages> <touches_per_page> <seconds> <pages_per_loop> <commit?>
./logging_first_byte $threads $(( (1<<16)/(1<<12) )) 1 30 10 1
