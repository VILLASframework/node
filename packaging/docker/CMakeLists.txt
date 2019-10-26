# CMakeLists.txt.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

set(DOCKER_FILE Dockerfile)
set(DOCKER_IMAGE registry.git.rwth-aachen.de/acs/public/villas/node)
set(DOCKER_TAG ${CMAKE_PROJECT_GIT_BRANCH})
set(DOCKER_RUN_OPTS
	--interactive
	--tty
	--publish 80:80
	--publish 443:443
	--publish 12000:12000/udp
	--publish 12001:12001/udp
	--publish 2345:2345
	--privileged
	--security-opt seccomp:unconfined
	--volume \"${PROJECT_SOURCE_DIR}:/villas\"
)

foreach(SUFFIX app dev dev-centos dev-ubuntu dev-raspbian)
	add_custom_target(deploy-docker-${SUFFIX}
		COMMAND docker push ${DOCKER_IMAGE}/${SUFFIX}:${DOCKER_TAG}
		COMMAND docker push ${DOCKER_IMAGE}/${SUFFIX}:latest
		COMMENT "Deploy Docker image: ${DOCKER_IMAGE}/${SUFFIX}:${DOCKER_TAG}"
	)

	add_custom_target(run-docker-${SUFFIX}
		COMMAND docker run ${DOCKER_RUN_OPTS} ${DOCKER_IMAGE}/${SUFFIX}:${DOCKER_TAG}
		COMMENT "Run Docker image: ${DOCKER_IMAGE}/${SUFFIX}:${DOCKER_TAG}"
	)

	add_custom_target(docker-${SUFFIX}
		COMMAND docker build
			--file ${CMAKE_CURRENT_SOURCE_DIR}/Dockerfile.${SUFFIX}
			--tag ${DOCKER_IMAGE}/${SUFFIX}:${DOCKER_TAG}
			--tag ${DOCKER_IMAGE}/${SUFFIX}:latest
			--build-arg BUILDER_IMAGE=${DOCKER_IMAGE}/dev:${DOCKER_TAG}
			--build-arg DOCKER_TAG=${DOCKER_TAG}
			--build-arg GIT_BRANCH=${CMAKE_PROJECT_GIT_BRANCH}
			--build-arg GIT_REV=${CMAKE_PROJECT_GIT_REV}
			--build-arg VERSION=${CMAKE_PROJECT_VERSION}
			--build-arg VARIANT=${CMAKE_PROJECT_VARIANT}
			${DOCKER_BUILD_OPTS} ${PROJECT_SOURCE_DIR}
		COMMENT "Build Docker image: ${DOCKER_IMAGE}/${SUFFIX}:${DOCKER_TAG}"
	)

	add_dependencies(deploy-docker-${SUFFIX} docker-${SUFFIX})
	add_dependencies(run-docker-${SUFFIX} docker-${SUFFIX})
endforeach()

# Special cases for 'docker' target
add_custom_target(run-docker
	COMMAND docker run ${DOCKER_RUN_OPTS} ${DOCKER_IMAGE}:${DOCKER_TAG} node -h
	COMMENT "Run Docker image: ${DOCKER_IMAGE}:${DOCKER_TAG}"
)

add_custom_target(docker
	COMMAND docker tag ${DOCKER_IMAGE}/app:${DOCKER_TAG} ${DOCKER_IMAGE}:${DOCKER_TAG}
	COMMAND docker tag ${DOCKER_IMAGE}/app:${DOCKER_TAG} ${DOCKER_IMAGE}:latest
)

add_custom_target(deploy-docker DEPENDS docker-app
	COMMAND docker push ${DOCKER_IMAGE}:${DOCKER_TAG}
	COMMAND docker push ${DOCKER_IMAGE}:latest
)

add_dependencies(docker docker-app)
add_dependencies(docker-app docker-dev)
add_dependencies(deploy-docker deploy-docker-app)
add_dependencies(run-docker docker)
