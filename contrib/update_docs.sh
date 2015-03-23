#/bin/bash

cd $( cd "$( dirname "$0" )/.." && pwd )

LASTREV=$(git rev-parse HEAD)
git pull --all
NEWREV=$(git rev-parse HEAD)

if [ "$LASTREV" != "$NEWREV" ]; then
	echo "There's a new version. Running doxygen"
	doxygen
fi
