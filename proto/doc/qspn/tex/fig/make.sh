for i in segABC*.dot cycleABC*.dot;
do
	circo $i -Tps $i > `basename $i .dot`.ps
done

for i in `ls -1 *.ps`; do
	epstopdf $i
done
