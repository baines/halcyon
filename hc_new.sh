#!/bin/bash

PROJ_NAME="$1"
if [[ -z "$PROJ_NAME" ]]; then
	echo "Usage: $0 <project>"
	exit 1
fi

cd "${0%/*}"

if [[ -e "$PROJ_NAME" ]]; then
	echo "Error: That name conflicts with a file/directory in the root."
	exit 1
fi

if ! mkdir "src/$PROJ_NAME" 2>/dev/null; then
	echo "Error: Project \"$PROJ_NAME\" appears to already exist."
	exit 1
fi

MAIN="src/$PROJ_NAME/main.c"

cat > "$MAIN" << EOF
#include "halcyon.h"

int main(int argc, char** argv){
	hc_init("$PROJ_NAME");

	for(;;){

		hc_finish(0);
	}
}
EOF

echo "Created project $PROJ_NAME."
"${EDITOR:-vim}" "$MAIN"
