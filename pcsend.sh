#!/bin/sh
if [ $# -ne 1 ]; then
  echo "Usage: $0 filename"
  echo "Triggers send of a file out of the Oberon system to its host."
  echo "(Start PCLink first in Oberon, by middle-click or Alt on PCLink1.Run)"
  exit 1
fi
echo $1 > PCLink.SND
