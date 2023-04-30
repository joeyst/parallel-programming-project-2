#!/bin/bash
for initialheight in 5 10 
do
	for nownumrabbits in 5 10
	do 
		for ryegrassgrowspermonth in 5 50 100 
		do
			for onerabbitseatspermonth in 1.0 2.0 5.0 10.0
			do
				echo "---"
				echo "NowHeight $initialheight, NowNumRabbits $nownumrabbits, RyegrassGrowsPerMonth $ryegrassgrowspermonth, OneRabbitsEatsPerMonth $onerabbitseatspermonth"
				g++ proj02.cpp -o proj02 -lm -fopenmp -DINITIALHEIGHT=$initialheight -DNOWNUMRABBITS=$nownumrabbits -DRYEGRASSGROWSPERMONTH=$ryegrassgrowspermonth -DONERABBITSEATSPERMONTH=$onerabbitseatspermonth
				./proj02
				rm proj02
			done
		done
	done
done