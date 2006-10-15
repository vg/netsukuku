rm -v *.ps *.pdf

for i in seg*.dot cycle*.dot;
do
	circo $i -Tps $i > `basename $i .dot`.ps
done

for i in *.png;
do
	convert $i $(basename $i .png).ps
done

for i in *.jpg;
do
	convert $i $(basename $i .png).ps
done

for i in *.ps; do
	epstopdf $i
done
