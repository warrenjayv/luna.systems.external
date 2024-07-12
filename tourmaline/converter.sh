#!/bin/bash

# store ls to an array
array=($(ls *.m4a))

# iterate
for i in ${array[@]};
do
  ffmpeg -i ${i} ${i%.*}'.wav'
done


