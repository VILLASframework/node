#!/bin/bash
# 
# Rewrite Git history
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
# @license GNU General Public License (version 3)
#
# VILLASnode
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
###################################################################################

#git filter-branch --tree-filter "find . -type f -print0 | xargs -0 sed -i 's/subdomainA\.example\.com/subdomainB.example.com/g'" -- --all

#git filter-branch \
#	--index-filter 'git rm --cached --ignore-unmatch COPYING' \
#	--index-filter 'git rm --cached --ignore-unmatch COPYING.md' \
#	--index-filter 'git rm --cached --ignore-unmatch LICENSE' \
#	--index-filter 'git rm --cached --ignore-unmatch LICENSE.md'  -- --all

git filter-branch --force --prune-empty \
	--tag-name-filter cat \
	--index-filter 'git rm --cached --ignore-unmatch COPYING.md' \
	--tree-filter ~/workspace/rwth/villas/remove_lgpl.sh \
	--env-filter '
CORRECT_NAME="Steffen Vogel"
CORRECT_EMAIL="stvogel@eonerc.rwth-aachen.de"

OLD_EMAIL="noreply@github.com"
if [ "$GIT_COMMITTER_EMAIL" = "$OLD_EMAIL" ]; then
	export GIT_COMMITTER_NAME="$CORRECT_NAME"
	export GIT_COMMITTER_EMAIL="$CORRECT_EMAIL"
fi
if [ "$GIT_AUTHOR_EMAIL" = "$OLD_EMAIL" ]; then
	export GIT_AUTHOR_NAME="$CORRECT_NAME"
	export GIT_AUTHOR_EMAIL="$CORRECT_EMAIL"
fi

OLD_EMAIL="StVogel@eonerc.rwth-aachen.de"
if [ "$GIT_COMMITTER_EMAIL" = "$OLD_EMAIL" ]; then
	export GIT_COMMITTER_NAME="$CORRECT_NAME"
	export GIT_COMMITTER_EMAIL="$CORRECT_EMAIL"
fi
if [ "$GIT_AUTHOR_EMAIL" = "$OLD_EMAIL" ]; then
	export GIT_AUTHOR_NAME="$CORRECT_NAME"
	export GIT_AUTHOR_EMAIL="$CORRECT_EMAIL"
fi

OLD_EMAIL="steffen.vogel@rwth-aachen.de"
if [ "$GIT_COMMITTER_EMAIL" = "$OLD_EMAIL" ]; then
	export GIT_COMMITTER_NAME="$CORRECT_NAME"
	export GIT_COMMITTER_EMAIL="$CORRECT_EMAIL"
fi
if [ "$GIT_AUTHOR_EMAIL" = "$OLD_EMAIL" ]; then
	export GIT_AUTHOR_NAME="$CORRECT_NAME"
	export GIT_AUTHOR_EMAIL="$CORRECT_EMAIL"
fi

OLD_EMAIL="post@steffenvogel.de"
if [ "$GIT_COMMITTER_EMAIL" = "$OLD_EMAIL" ]; then
	export GIT_COMMITTER_NAME="$CORRECT_NAME"
	export GIT_COMMITTER_EMAIL="$CORRECT_EMAIL"
fi
if [ "$GIT_AUTHOR_EMAIL" = "$OLD_EMAIL" ]; then
	export GIT_AUTHOR_NAME="$CORRECT_NAME"
	export GIT_AUTHOR_EMAIL="$CORRECT_EMAIL"
fi

CORRECT_NAME="Umar Farooq"
CORRECT_EMAIL="ufarooq@eonerc.rwth-aachen.de"

OLD_EMAIL="UFarooq@eonerc.rwth-aachen.de"
if [ "$GIT_COMMITTER_EMAIL" = "$OLD_EMAIL" ]; then
	export GIT_COMMITTER_NAME="$CORRECT_NAME"
	export GIT_COMMITTER_EMAIL="$CORRECT_EMAIL"
fi
if [ "$GIT_AUTHOR_EMAIL" = "$OLD_EMAIL" ]; then
	export GIT_AUTHOR_NAME="$CORRECT_NAME"
	export GIT_AUTHOR_EMAIL="$CORRECT_EMAIL"
fi

OLD_EMAIL="umar.farooq@rwth-aachen.de"
if [ "$GIT_COMMITTER_EMAIL" = "$OLD_EMAIL" ]; then
	export GIT_COMMITTER_NAME="$CORRECT_NAME"
	export GIT_COMMITTER_EMAIL="$CORRECT_EMAIL"
fi
if [ "$GIT_AUTHOR_EMAIL" = "$OLD_EMAIL" ]; then
	export GIT_AUTHOR_NAME="$CORRECT_NAME"
	export GIT_AUTHOR_EMAIL="$CORRECT_EMAIL"
fi

CORRECT_NAME="Marija Stevic"
CORRECT_EMAIL="mstevic@eonerc.rwth-aachen.de"

OLD_EMAIL="mstevic@users.noreply.github.com"
if [ "$GIT_COMMITTER_EMAIL" = "$OLD_EMAIL" ]; then
	export GIT_COMMITTER_NAME="$CORRECT_NAME"
	export GIT_COMMITTER_EMAIL="$CORRECT_EMAIL"
fi
if [ "$GIT_AUTHOR_EMAIL" = "$OLD_EMAIL" ]; then
	export GIT_AUTHOR_NAME="$CORRECT_NAME"
	export GIT_AUTHOR_EMAIL="$CORRECT_EMAIL"
fi

OLD_EMAIL="marija.stevic@eonerc.rwth-aachen.de"
if [ "$GIT_COMMITTER_EMAIL" = "$OLD_EMAIL" ]; then
	export GIT_COMMITTER_NAME="$CORRECT_NAME"
	export GIT_COMMITTER_EMAIL="$CORRECT_EMAIL"
fi
if [ "$GIT_AUTHOR_EMAIL" = "$OLD_EMAIL" ]; then
	export GIT_AUTHOR_NAME="$CORRECT_NAME"
	export GIT_AUTHOR_EMAIL="$CORRECT_EMAIL"
fi

OLD_EMAIL="msv@E098.eonerc.rwth-aachen.de"
if [ "$GIT_COMMITTER_EMAIL" = "$OLD_EMAIL" ]; then
	export GIT_COMMITTER_NAME="$CORRECT_NAME"
	export GIT_COMMITTER_EMAIL="$CORRECT_EMAIL"
fi
if [ "$GIT_AUTHOR_EMAIL" = "$OLD_EMAIL" ]; then
	export GIT_AUTHOR_NAME="$CORRECT_NAME"
	export GIT_AUTHOR_EMAIL="$CORRECT_EMAIL"
fi

OLD_EMAIL="msv@E250.eonerc.rwth-aachen.de"
if [ "$GIT_COMMITTER_EMAIL" = "$OLD_EMAIL" ]; then
	export GIT_COMMITTER_NAME="$CORRECT_NAME"
	export GIT_COMMITTER_EMAIL="$CORRECT_EMAIL"
fi
if [ "$GIT_AUTHOR_EMAIL" = "$OLD_EMAIL" ]; then
	export GIT_AUTHOR_NAME="$CORRECT_NAME"
	export GIT_AUTHOR_EMAIL="$CORRECT_EMAIL"
fi
' -- --all