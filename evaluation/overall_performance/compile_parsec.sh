#1 parsec path
PARSEC_PATH=$1;

cd $PARSEC_PATH;
source env.sh;
cd -;

for b in blackscholes
do
	for c in gcc-consequence
	do
		parsecmgmt -c $c -p $b -a build;
	done
done
