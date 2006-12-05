#!/bin/bash

if [ ! -d out ]
then
	mkdir out
fi

for((R=1; R <= 8; R++))
do
		for((i=2; i<=16; i++))
		do
			ii=$((i*i))
			for((S=1; S<=$ii; S++))
			do
				echo Mesh graph $((i*i)): -v -R $R -m $i -s $S
				./q2sim.py -v -R $R -m $i -s $S -p > out/"v-R:$R-m:$i-s:$S"
			done
		done

		for((i=2; i<=256; i++))
		do
			for((S=1; S<=$i; S++))
			do
				echo Random graph $i: -v -R $R -r $i -s $S
				./q2sim.py -v -R $R -r $i -s $S -p > out/"v-R:$R-r:$i-s:$S"
			done
		done

		for((i=2; i<=256; i++))
		do
			for((S=1; S<=$i; S++))
			do
				echo Complete graph $i: -v -R $R -k $i -s $S
				./q2sim.py -v -R $R -k $i -s $S -p > out/"v-R:$R-k:$i-s:$S"
			done
		done
done
