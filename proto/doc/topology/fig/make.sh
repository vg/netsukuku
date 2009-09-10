rm -v *.ps

for i in `ls -1 seg*.dot cycle*.dot`;
do
	circo $i -Tps $i > `basename $i .dot`.ps
done


for i in `ls -1 neato*.dot`;
do
	neato $i -Tps $i > `basename $i .dot`.ps
done

for i in `ls -1 *.png`;
do
	convert $i $(basename $i .png).ps
done

for i in `ls -1 *.ps`;
do
	if [ ! -f $(basename $i .png).png ]
	then
		convert $i $(basename $i .png).png
	fi
done


for i in `ls -1 *.jpg`;
do
	convert $i $(basename $i .jpg).ps
done
