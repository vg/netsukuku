rm -v *.ps *.pdf

for i in `ls -1 seg*.dot cycle*.dot`;
do
	circo $i -Tps $i > `basename $i .dot`.ps
done


for i in `ls -1 neato*.dot`;
do
	neato $i -Tps $i > `basename $i .dot`.ps
done

for i in `ls -1 *.ps`; do
	convert $i $(basename $i .ps).pdf
done

for i in `ls -1 *.png`;
do
	convert $i $(basename $i .png).ps
	convert $i $(basename $i .png).pdf
done

for i in `ls -1 *.jpg`;
do
	convert $i $(basename $i .jpg).ps
	convert $i $(basename $i .jpg).pdf
done
