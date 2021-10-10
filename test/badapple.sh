#!/bin/sh

set -e

badapple_url="https://www.youtube.com/watch?v=9lNZ_Rnr7Jc"
badapple_mp4="badapple.mp4"
badapple_raw="badapple.raw"

width_pix=120
height_pix=20

if ! [ -f "$badapple_mp4" ] ; then
	youtube-dl -o "$badapple_mp4" "$badapple_url"
fi

if ! [ -f "$badapple_raw" ] ; then
	ffmpeg -i "$badapple_mp4" -vf scale="$width_pix:$height_pix" -pix_fmt gray -vcodec rawvideo -f rawvideo "$badapple_raw"
fi

./ledpanel_movie_raw.py -W "$width_pix" -H "$height_pix" "$badapple_raw"
