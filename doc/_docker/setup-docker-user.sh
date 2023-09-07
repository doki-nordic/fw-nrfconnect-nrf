#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
WORKSPACE_DIR=`realpath $SCRIPT_DIR/../../..`

USER_NAME=$1
USER_ID=$2
GROUP_NAME=$3
GROUP_ID=$4

FILES_USER_ID=`stat -c %u "${BASH_SOURCE[0]}"`

# If the files are owned by the same user as host, we are running from rooted docker daemon.
# We have to create user and group to mimmic the host environment.
# Otherwise, we are running rootless docker daemon. We can continue working as root, since
# ownership will be translated to the calling host user.
if [ "$USER_ID" == "$FILES_USER_ID" ]; then

	groupadd -g $GROUP_ID $GROUP_NAME

	if [ -d /home/$USER_NAME ]; then
		function move_hidden {
			shopt -u dotglob
			GLOBIGNORE=".:.."
			mv /home/$USER_NAME/* /home/tmpdir/
		}
		mv /home/$USER_NAME /home/tmpdir
		useradd $USER_NAME -u $USER_ID -g $GROUP_ID -m -s /bin/bash
		chown $USER_NAME:$GROUP_NAME /home/tmpdir
		move_hidden
		rm -Rf /home/$USER_NAME
		mv /home/tmpdir /home/$USER_NAME
	else
		useradd $USER_NAME -u $USER_ID -g $GROUP_ID -m -s /bin/bash
	fi

	echo "$USER_NAME ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
	echo "root ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

	sudo --user=$USER_NAME $SCRIPT_DIR/a.sh "${@:5}"

else

	$SCRIPT_DIR/a.sh "${@:5}"

fi
